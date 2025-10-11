#include <vulkan/vulkan.h>
#include "platform_layer.h"
IMPLEMENT_CAPPED_ARRAY(textures, texture, MAX_TEXTURES)

// These function needs to be implemented in the platform layer .c file.
VkSurfaceKHR create_window_surface(VkInstance instance, window* window);
bool check_window_presentation_support(VkPhysicalDevice device, uint32_t queue_family_index, VkSurfaceKHR window_surface);

// Implementation details:
typedef struct {
    uint32_t graphics_family;
    uint32_t present_family;
} queue_family_indices;

typedef struct {
    window* window;
    VkSurfaceKHR surface;
} vulkan_window;

typedef struct {
    VkPhysicalDevice physical_device;
    queue_family_indices queue_family_indices;
    VkQueue graphics_queue;
    VkQueue present_queue;
    VkDevice handle;
} vulkan_device;

typedef struct {
    VkFormat image_format;
    VkExtent2D extent;
    uint32_t image_count;
    VkImage* images;
    VkSwapchainKHR handle;
} vulkan_swapchain;

typedef struct {
    VkRenderPass render_pass;
    VkDeviceMemory image_memory;
    VkImageView* image_views;
    VkFramebuffer* framebuffers;
} vulkan_render_targets;

typedef struct {
    VkShaderModule vert_shader;
    VkShaderModule frag_shader;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout layout;
    VkPipeline handle;
} vulkan_graphics_pipeline;

#define MAX_FRAMES_IN_FLIGHT 2

typedef struct {
    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
    VkFence in_flight_fence;
    VkCommandBuffer command_buffer;
    VkDescriptorSet descriptor_set;
} vulkan_frame_execution;

typedef struct {
    VkInstance instance;
    vulkan_window window;
    vulkan_device device;
    vulkan_swapchain swapchain;
    vulkan_render_targets render_targets;
    vulkan_graphics_pipeline graphics_pipeline;

    VkCommandPool graphics_command_pool;
    VkCommandPool transfer_command_pool;
    VkDescriptorPool descriptor_pool;
    vulkan_frame_execution frame_executions[MAX_FRAMES_IN_FLIGHT];
    VkImage sprite_sheet_image;
    VkDeviceMemory sprite_sheet_memory;
} sprite_renderer_internals;

/*

---- Basic Explanation of Vulkan API ---

Vulkan is a complicated API that requires specifying a bunch of things upfront for performance. One of the things that it does is force the programmer to define the resource usage
first before defining the graphics pipelines themselves. The terms can be confusing, but here is a brief explanation of some of the key concepts:

- A "Render Pass" is where you tell Vulkan what resources you want to stay in fast GPU memory for the duration of multiple operations (subpasses). For example, you can say that you want to keep a color buffer
  and a depth buffer in fast memory for the duration of the render pass. A render pass can have multiple subpasses, which are individual steps in the rendering process that can read from and write to the resources defined in the render pass.
- A "Subpass" is a single step in the rendering process that can read from and write to the resources defined in the render pass. For example, you can have a subpass that renders the scene to a color buffer,
  and another subpass that applies post-processing effects to the color buffer.

Note that the above two just tell vulkan the resource usage. Render Pass is where you declare what resources you want use and where they will be bound, and Subpass is where you define what happens to those resources in a single step and how they will be loaded.
Note that a subpass does not define _what_ happens in that step, it's just how that step will modify the memory.

- A "pipeline" is where you actually define the behaviour that performs the memory modification expected by a subpass. A pipeline typically refers to a whole graphics pipeline from start to finish. Each pipeline
is associated with a render pass and a subpass index. You can chain multiple pipelines together, one for each subpass. For example, you might want to draw the scene to a color buffer in one subpass, then do some lighting calculations in a separate subpass.

- A "Framebuffer" is where you actually apply the Render Pass resource specification to a specific set of images. The subpass will expect the images to be in certain binding points, and the framebuffer is where you tell vulkan which images to bind to those binding points.

To draw something in Vulkan, first you use vkCmdBeginRenderPass, specifying the frame buffer (actual images) and render pass (resource slots) to use. Then you bind the graphics pipeline (the actual drawing behaviour) that is compatible with the subpass index you are in.
Then you issue draw commands, and finally you call vkCmdEndRenderPass to finish the render pass. If you want to switch to another subpass within a renderpass, you can call vkCmdNextSubpass, bind a different pipeline, and issue more draw commands.
Note that a pipeline always needs to be bound if you are doing draw commands for a specific subpass. The resource usage (subpass) and the actual drawing behaviour (pipeline) are separate concepts, but every subpass normally just uses one pipeline.

If I was to rename these concepts, I would call render pass "expected_resource_usages", subpass "resource_usage", pipeline "pipeline", and framebuffer "image_bindings". But I guess the vulkan names are cooler.

-------------------------------------------

How vulkan shaders reference these resources:
The "Render Pass" contains a global array of the resources. Every subpass uses a subset of the these resources. This "subset" is defined as an array of index values, basically a lookup table that maps the index values used in the shader to the index values for the global array in the Render Pass.
For example:

Shader (uses resource at index 0) -> Subpass [1, 2, 3] index 0 is 1 -> Render Pass [color1, color2, color3] index 1 is color2

Shaders reference resources in two different ways. There's output attachments (which are just referenced with layout(location = X) The indexing scheme above). And there's input attachments, which are referenced with layout(input_attachment_index = X) The indexing scheme is the same for both, but they are separate arrays.
The subpass has a color attachment array (for output attachments) and an input attachment array (for input attachments). Each array contains indices into the global array in the render pass.

This is how vulkan achieves sharing resources between subpasses. The render pass has a global array of resources, and each subpass defines how it uses a subset of those resources. The shaders reference the resources using indices into the subpass arrays, which are then mapped to the global array in the render pass.

------------------------------------------
*/

#pragma region Selecting Device

// Rate the physical device, score of 0 means the device is not suitable.
static uint32_t rate_physical_device(VkPhysicalDevice device, VkSurfaceKHR window_surface, queue_family_indices* out_queue_families) {
    ASSERT(device != VK_NULL_HANDLE, return 0, "Physical device cannot be NULL");
    ASSERT(window_surface != VK_NULL_HANDLE, return 0, "Window surface cannot be NULL");
    ASSERT(out_queue_families != NULL, return 0, "Output queue family indices cannot be NULL");

    // check to ensure that the device has swapchain support
    // This is done by checking for the presence of the VK_KHR_swapchain extension

    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
    if (extension_count == 0) {
        return 0;
    }

    VkExtensionProperties* extensions = (VkExtensionProperties*)bump_allocate(&temp_allocator, alignof(VkExtensionProperties), extension_count * sizeof(VkExtensionProperties));
    if (extensions == NULL) {
        BUG("Failed to allocate memory for device extension properties.");
        return 0;
    }

    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, extensions);
    bool has_swapchain_extension = false;
    for (uint32_t i = 0; i < extension_count; ++i) {
        if (strcmp(extensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            has_swapchain_extension = true;
            break;
        }
    }
    if (!has_swapchain_extension) {
        return 0;
    }

    // If it has the swapchain, to be extra sure, we should try getting the supported surface formats and present modes.
    // I think it's highly unlikely that a device would support the swapchain extension but not have any supported formats or present modes for the window, but just in case.
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, window_surface, &format_count, NULL);
    if (format_count == 0) {
        return 0;
    }

    // Enumerate over the device's queue families to see if it supports presenting to our window surface.

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    ASSERT(queue_family_count > 0, return 0, "Physical device has no queue families.");
    VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)bump_allocate(&temp_allocator, alignof(VkQueueFamilyProperties), queue_family_count * sizeof(VkQueueFamilyProperties));
    if (queue_families == NULL) {
        BUG("Failed to allocate memory for queue family properties.");
        return 0;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);


    // Search for presentation and graphics support in the queue families. If we find a queue family that supports both, we stop searching (best case).

    bool found_graphics = false;
    bool found_present = false;
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            out_queue_families->graphics_family = i;
            found_graphics = true;
        }

        if (check_window_presentation_support(device, i, window_surface)) {
            out_queue_families->present_family = i;
            found_present = true;
            if (out_queue_families->graphics_family == out_queue_families->present_family) {
                break; // Found a queue family that supports both graphics and present.
            }
        }
    }

    if (!found_graphics || !found_present) {
        return 0; // Device is not suitable, it doesn't support graphics or present.
    }

    /*` For now, just use the graphics family for transfer as well.
    // search for a queue family that supports transfer operations.
    // We priotize a queue family that we haven't picked before for graphics or present, but if we don't find one, it's not a big deal.
    out_queue_families->transfer_family = out_queue_families->graphics_family; // Default to graphics family if no better option is found.
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if ((queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && (i != out_queue_families->graphics_family) && (i != out_queue_families->present_family)) {
            out_queue_families->transfer_family = i;
            break;
        }
    }
    */

    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);

    // Rate discrete GPUs higher than integrated GPUs
    uint32_t score = 1;
    if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }
    else if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 500;
    }

    // Give a small score boost based on the maximum supported image dimension (just as a rough measure of performance)
    score += device_properties.limits.maxImageDimension2D / 1000;
    return score;
}

static result select_physical_device(VkInstance vulkan_instance, VkSurfaceKHR window_surface, VkPhysicalDevice* out_physical_device, queue_family_indices* out_queue_families) {
    ASSERT(window_surface != VK_NULL_HANDLE, return RESULT_FAILURE, "Window surface cannot be NULL");
    ASSERT(out_physical_device != NULL, return RESULT_FAILURE, "Output physical device pointer cannot be NULL");
    ASSERT(out_queue_families != NULL, return RESULT_FAILURE, "Output queue family indices cannot be NULL");

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(vulkan_instance, &device_count, NULL);
    ASSERT(device_count > 0, return RESULT_FAILURE, "No Vulkan physical devices found.");
    VkPhysicalDevice* devices = (VkPhysicalDevice*)bump_allocate(&temp_allocator, alignof(VkPhysicalDevice), device_count * sizeof(VkPhysicalDevice));
    if (devices == NULL) {
        BUG("Failed to allocate memory for physical device list.");
        return RESULT_FAILURE;
    }

    vkEnumeratePhysicalDevices(vulkan_instance, &device_count, devices);

    // Select the best physical device
    uint32_t best_score = 0;
    for (uint32_t i = 0; i < device_count; ++i) {
        queue_family_indices indices = { 0 };
        uint32_t score = rate_physical_device(devices[i], window_surface, &indices);
        if (score > best_score) {
            best_score = score;
            memcpy(out_queue_families, &indices, sizeof(queue_family_indices));
            *out_physical_device = devices[i];
        }
    }

    return RESULT_SUCCESS;
}

static result create_logical_device(VkInstance vulkan_instance, VkSurfaceKHR window_surface, vulkan_device* out_device) {
    ASSERT(out_device != NULL, return RESULT_FAILURE, "Output device pointer cannot be NULL");

    if (select_physical_device(vulkan_instance, window_surface, &out_device->physical_device, &out_device->queue_family_indices) != RESULT_SUCCESS) {
        BUG("Failed to select a suitable physical device.");
        return RESULT_FAILURE;
    }

    float queue_priority = 1.0f;
    uint32_t unique_queue_families[2] = { out_device->queue_family_indices.graphics_family, out_device->queue_family_indices.present_family };
    uint32_t unique_queue_family_count = 0;

    // Count unique queue families
    for (uint32_t i = 0; i < 2; ++i) {
        bool found = false;
        for (uint32_t j = 0; j < unique_queue_family_count; ++j) {
            if (unique_queue_families[i] == unique_queue_families[j]) {
                found = true;
                break;
            }
        }
        if (!found) {
            unique_queue_families[unique_queue_family_count++] = unique_queue_families[i];
        }
    }

    VkDeviceQueueCreateInfo* queue_create_infos = (VkDeviceQueueCreateInfo*)bump_allocate(&temp_allocator, alignof(VkDeviceQueueCreateInfo), unique_queue_family_count * sizeof(VkDeviceQueueCreateInfo));
    if (queue_create_infos == NULL) {
        BUG("Failed to allocate memory for device queue create infos.");
        return RESULT_FAILURE;
    }

    for (uint32_t i = 0; i < unique_queue_family_count; ++i) {
        queue_create_infos[i] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = unique_queue_families[i],
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        };
    }

    // Specify device features we want to use
    VkPhysicalDeviceFeatures device_features = { 0 };

    const char* required_device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

#ifndef NDEBUG
    const char* validation_layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };
    uint32_t validation_layer_count = sizeof(validation_layers) / sizeof(validation_layers[0]);
#else
    const char** validation_layers = NULL;
    uint32_t validation_layer_count = 0;
#endif

    // Create the logical device
    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = unique_queue_family_count,
        .pQueueCreateInfos = queue_create_infos,
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = sizeof(required_device_extensions) / sizeof(required_device_extensions[0]),
        .ppEnabledExtensionNames = required_device_extensions,
        .enabledLayerCount = validation_layer_count,
        .ppEnabledLayerNames = validation_layers,
    };

    if (vkCreateDevice(out_device->physical_device, &create_info, NULL, &out_device->handle) != VK_SUCCESS) {
        BUG("Failed to create logical device.");
        return RESULT_FAILURE;
    }

    vkGetDeviceQueue(out_device->handle, out_device->queue_family_indices.graphics_family, 0, &out_device->graphics_queue);
    vkGetDeviceQueue(out_device->handle, out_device->queue_family_indices.present_family, 0, &out_device->present_queue);
    //vkGetDeviceQueue(out_device->handle, out_device->queue_family_indices.transfer_family, 0, &out_device->transfer_queue);
    return RESULT_SUCCESS;
}

static VkSurfaceFormatKHR pick_surface_format(const VkSurfaceFormatKHR* available_formats, uint32_t format_count) {
    ASSERT(available_formats != NULL, return (VkSurfaceFormatKHR){0}, "Available formats pointer cannot be NULL");
    ASSERT(format_count > 0, return (VkSurfaceFormatKHR){0}, "Format count must be greater than 0");

    for (uint32_t i = 0; i < format_count; ++i) {
        VkSurfaceFormatKHR format = available_formats[i];
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return available_formats[0];
}

static VkPresentModeKHR pick_present_mode(const VkPresentModeKHR* available_present_modes, uint32_t present_mode_count) {
    ASSERT(available_present_modes != NULL, return VK_PRESENT_MODE_FIFO_KHR, "Available present modes pointer cannot be NULL");
    ASSERT(present_mode_count > 0, return VK_PRESENT_MODE_FIFO_KHR, "Present mode count must be greater than 0");
    for (uint32_t i = 0; i < present_mode_count; ++i) {
        if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return available_present_modes[i];
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR; // FIFO is guaranteed to be available
}

static VkExtent2D pick_swap_extent(const VkSurfaceCapabilitiesKHR* capabilities, window* window) {
    ASSERT(capabilities != NULL, return (VkExtent2D){0}, "Surface capabilities pointer cannot be NULL");
    ASSERT(window != NULL, return (VkExtent2D){0}, "Window pointer cannot be NULL");

    if (capabilities->currentExtent.width != UINT32_MAX) {
        return capabilities->currentExtent;
    }

    window_size size = get_window_size(window);
    VkExtent2D actual_extent = {
        .width = (uint32_t)size.width,
        .height = (uint32_t)size.height,
    };

    if (actual_extent.width < capabilities->minImageExtent.width) {
        actual_extent.width = capabilities->minImageExtent.width;
    }
    else if (actual_extent.width > capabilities->maxImageExtent.width) {
        actual_extent.width = capabilities->maxImageExtent.width;
    }
    if (actual_extent.height < capabilities->minImageExtent.height) {
        actual_extent.height = capabilities->minImageExtent.height;
    }
    else if (actual_extent.height > capabilities->maxImageExtent.height) {
        actual_extent.height = capabilities->maxImageExtent.height;
    }

    return actual_extent;
}

static result create_swapchain(vulkan_device* device, vulkan_window* window, vulkan_swapchain* out_swapchain) {
    ASSERT(device != NULL, return RESULT_FAILURE, "Device pointer cannot be NULL");
    ASSERT(window != NULL, return RESULT_FAILURE, "Window pointer cannot be NULL");
    ASSERT(out_swapchain != NULL, return RESULT_FAILURE, "Output swapchain cannot be NULL");
    ASSERT(device->handle != VK_NULL_HANDLE, return RESULT_FAILURE, "Logical device handle cannot be NULL");
    ASSERT(window->surface != VK_NULL_HANDLE, return RESULT_FAILURE, "Window surface cannot be NULL");
    ASSERT(device->physical_device != VK_NULL_HANDLE, return RESULT_FAILURE, "Physical device handle cannot be NULL");
    ASSERT(device->queue_family_indices.graphics_family != UINT32_MAX, return RESULT_FAILURE, "Graphics queue family index is invalid");
    ASSERT(device->queue_family_indices.present_family != UINT32_MAX, return RESULT_FAILURE, "Present queue family index is invalid");
    //ASSERT(device->queue_family_indices.transfer_family != UINT32_MAX, return RESULT_FAILURE, "Transfer queue family index is invalid");

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device, window->surface, &capabilities);

    // Get the supported surface formats
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, window->surface, &format_count, NULL);
    ASSERT(format_count > 0, return RESULT_FAILURE, "No surface formats found.");

    VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)bump_allocate(&temp_allocator, alignof(VkSurfaceFormatKHR), format_count * sizeof(VkSurfaceFormatKHR));
    if (formats == NULL) {
        BUG("Failed to allocate memory for surface formats.");
        return RESULT_FAILURE;
    }

    vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, window->surface, &format_count, formats);

    // Get the supported present modes
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, window->surface, &present_mode_count, NULL);
    ASSERT(present_mode_count > 0, return RESULT_FAILURE, "No present modes found");

    VkPresentModeKHR* present_modes = (VkPresentModeKHR*)bump_allocate(&temp_allocator, alignof(VkPresentModeKHR), present_mode_count * sizeof(VkPresentModeKHR));
    if (present_modes == NULL) {
        BUG("Failed to allocate memory for present modes.");
        return RESULT_FAILURE;
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, window->surface, &present_mode_count, present_modes);

    // Pick the best present mode and surface format
    VkSurfaceFormatKHR surface_format = pick_surface_format(formats, format_count);
    VkPresentModeKHR present_mode = pick_present_mode(present_modes, present_mode_count);

    VkExtent2D swap_extent = pick_swap_extent(&capabilities, window->window);
    if (swap_extent.width == 0 || swap_extent.height == 0) {
        BUG("Failed to pick swap extent.");
        return RESULT_FAILURE;
    }

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    // specify the format of the swap chain
    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = window->surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = swap_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    uint32_t queue_family_indices[] = { device->queue_family_indices.graphics_family, device->queue_family_indices.present_family };
    if (device->queue_family_indices.graphics_family != device->queue_family_indices.present_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0; // Optional
        create_info.pQueueFamilyIndices = NULL; // Optional
    }

    if (vkCreateSwapchainKHR(device->handle, &create_info, NULL, &out_swapchain->handle) != VK_SUCCESS) {
        BUG("Failed to create swapchain.");
        return RESULT_FAILURE;
    }

    out_swapchain->image_format = surface_format.format;
    out_swapchain->extent = swap_extent;
    out_swapchain->image_count = image_count;
    out_swapchain->images = (VkImage*)bump_allocate(&permanent_allocator, alignof(VkImage), image_count * sizeof(VkImage));
    if (out_swapchain->images == NULL) {
        BUG("Failed to allocate memory for swapchain images.");
        return RESULT_FAILURE;
    }

    vkGetSwapchainImagesKHR(device->handle, out_swapchain->handle, &image_count, out_swapchain->images);
    return RESULT_SUCCESS;
}

#pragma region Compiled shaders
static uint8_t compiled_frag_shader_spv[640] = {
    0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00, 0x0B, 0x00, 0x0D, 0x00,
    0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x02, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x47, 0x4C, 0x53, 0x4C, 0x2E, 0x73, 0x74, 0x64, 0x2E, 0x34, 0x35, 0x30,
    0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x07, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x6D, 0x61, 0x69, 0x6E, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00,
    0x02, 0x00, 0x00, 0x00, 0xC2, 0x01, 0x00, 0x00, 0x04, 0x00, 0x0A, 0x00,
    0x47, 0x4C, 0x5F, 0x47, 0x4F, 0x4F, 0x47, 0x4C, 0x45, 0x5F, 0x63, 0x70,
    0x70, 0x5F, 0x73, 0x74, 0x79, 0x6C, 0x65, 0x5F, 0x6C, 0x69, 0x6E, 0x65,
    0x5F, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x69, 0x76, 0x65, 0x00, 0x00,
    0x04, 0x00, 0x08, 0x00, 0x47, 0x4C, 0x5F, 0x47, 0x4F, 0x4F, 0x47, 0x4C,
    0x45, 0x5F, 0x69, 0x6E, 0x63, 0x6C, 0x75, 0x64, 0x65, 0x5F, 0x64, 0x69,
    0x72, 0x65, 0x63, 0x74, 0x69, 0x76, 0x65, 0x00, 0x05, 0x00, 0x04, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x6D, 0x61, 0x69, 0x6E, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x05, 0x00, 0x09, 0x00, 0x00, 0x00, 0x6F, 0x75, 0x74, 0x5F,
    0x63, 0x6F, 0x6C, 0x6F, 0x72, 0x00, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00,
    0x0D, 0x00, 0x00, 0x00, 0x74, 0x65, 0x78, 0x5F, 0x73, 0x61, 0x6D, 0x70,
    0x6C, 0x65, 0x72, 0x00, 0x05, 0x00, 0x06, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x69, 0x6E, 0x5F, 0x66, 0x72, 0x61, 0x67, 0x74, 0x65, 0x78, 0x63, 0x6F,
    0x6F, 0x72, 0x64, 0x00, 0x47, 0x00, 0x04, 0x00, 0x09, 0x00, 0x00, 0x00,
    0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00,
    0x0D, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x47, 0x00, 0x04, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x02, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x21, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x16, 0x00, 0x03, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x20, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x3B, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x19, 0x00, 0x09, 0x00, 0x0A, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x03, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x0C, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x04, 0x00,
    0x0C, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x17, 0x00, 0x04, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x04, 0x00,
    0x10, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x36, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x02, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x3D, 0x00, 0x04, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x3D, 0x00, 0x04, 0x00,
    0x0F, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x57, 0x00, 0x05, 0x00, 0x07, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x03, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0xFD, 0x00, 0x01, 0x00,
    0x38, 0x00, 0x01, 0x00
};

static uint8_t compiled_vert_shader_spv[1056] = {
    0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00, 0x0B, 0x00, 0x0D, 0x00,
    0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x02, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x47, 0x4C, 0x53, 0x4C, 0x2E, 0x73, 0x74, 0x64, 0x2E, 0x34, 0x35, 0x30,
    0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x6D, 0x61, 0x69, 0x6E, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
    0x16, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00,
    0xC2, 0x01, 0x00, 0x00, 0x04, 0x00, 0x0A, 0x00, 0x47, 0x4C, 0x5F, 0x47,
    0x4F, 0x4F, 0x47, 0x4C, 0x45, 0x5F, 0x63, 0x70, 0x70, 0x5F, 0x73, 0x74,
    0x79, 0x6C, 0x65, 0x5F, 0x6C, 0x69, 0x6E, 0x65, 0x5F, 0x64, 0x69, 0x72,
    0x65, 0x63, 0x74, 0x69, 0x76, 0x65, 0x00, 0x00, 0x04, 0x00, 0x08, 0x00,
    0x47, 0x4C, 0x5F, 0x47, 0x4F, 0x4F, 0x47, 0x4C, 0x45, 0x5F, 0x69, 0x6E,
    0x63, 0x6C, 0x75, 0x64, 0x65, 0x5F, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74,
    0x69, 0x76, 0x65, 0x00, 0x05, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x6D, 0x61, 0x69, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x06, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x66, 0x72, 0x61, 0x67, 0x5F, 0x74, 0x65, 0x78,
    0x63, 0x6F, 0x6F, 0x72, 0x64, 0x00, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00,
    0x0B, 0x00, 0x00, 0x00, 0x69, 0x6E, 0x5F, 0x74, 0x65, 0x78, 0x63, 0x6F,
    0x6F, 0x72, 0x64, 0x00, 0x05, 0x00, 0x06, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x67, 0x6C, 0x5F, 0x50, 0x65, 0x72, 0x56, 0x65, 0x72, 0x74, 0x65, 0x78,
    0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x06, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x67, 0x6C, 0x5F, 0x50, 0x6F, 0x73, 0x69, 0x74,
    0x69, 0x6F, 0x6E, 0x00, 0x06, 0x00, 0x07, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x67, 0x6C, 0x5F, 0x50, 0x6F, 0x69, 0x6E, 0x74,
    0x53, 0x69, 0x7A, 0x65, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x07, 0x00,
    0x11, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x67, 0x6C, 0x5F, 0x43,
    0x6C, 0x69, 0x70, 0x44, 0x69, 0x73, 0x74, 0x61, 0x6E, 0x63, 0x65, 0x00,
    0x06, 0x00, 0x07, 0x00, 0x11, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x67, 0x6C, 0x5F, 0x43, 0x75, 0x6C, 0x6C, 0x44, 0x69, 0x73, 0x74, 0x61,
    0x6E, 0x63, 0x65, 0x00, 0x05, 0x00, 0x03, 0x00, 0x13, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00, 0x16, 0x00, 0x00, 0x00,
    0x69, 0x6E, 0x5F, 0x70, 0x6F, 0x73, 0x69, 0x74, 0x69, 0x6F, 0x6E, 0x00,
    0x47, 0x00, 0x04, 0x00, 0x09, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x1E, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00,
    0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x48, 0x00, 0x05, 0x00, 0x11, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x0B, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00,
    0x11, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x47, 0x00, 0x03, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x16, 0x00, 0x00, 0x00,
    0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x02, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x21, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x16, 0x00, 0x03, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x20, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x3B, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x0A, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x04, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x17, 0x00, 0x04, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x15, 0x00, 0x04, 0x00, 0x0E, 0x00, 0x00, 0x00,
    0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x04, 0x00,
    0x0E, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x1C, 0x00, 0x04, 0x00, 0x10, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x0F, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x06, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x0D, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x10, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x12, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x04, 0x00,
    0x12, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x15, 0x00, 0x04, 0x00, 0x14, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x04, 0x00, 0x14, 0x00, 0x00, 0x00,
    0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x04, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x2B, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x20, 0x00, 0x04, 0x00,
    0x1D, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00,
    0x36, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x02, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x3D, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x0C, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x03, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x3D, 0x00, 0x04, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00,
    0x51, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00,
    0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x51, 0x00, 0x05, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x50, 0x00, 0x07, 0x00, 0x0D, 0x00, 0x00, 0x00,
    0x1C, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x00,
    0x18, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x41, 0x00, 0x05, 0x00,
    0x1D, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
    0x15, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x03, 0x00, 0x1E, 0x00, 0x00, 0x00,
    0x1C, 0x00, 0x00, 0x00, 0xFD, 0x00, 0x01, 0x00, 0x38, 0x00, 0x01, 0x00
};

#pragma endregion

static result create_render_targets(VkDevice device, vulkan_swapchain* swapchain, vulkan_render_targets* out_render_targets) {
    ASSERT(device != VK_NULL_HANDLE, return RESULT_FAILURE, "Device handle cannot be NULL");
    ASSERT(swapchain != NULL, return RESULT_FAILURE, "Swapchain pointer cannot be NULL");
    ASSERT(out_render_targets != NULL, return RESULT_FAILURE, "Output render targets pointer cannot be NULL");
    ASSERT(swapchain->image_count > 0, return RESULT_FAILURE, "Swapchain must have at least one image.");

    VkAttachmentDescription color_attachment = {
        .format = swapchain->image_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    if (vkCreateRenderPass(device, &render_pass_info, NULL, &out_render_targets->render_pass) != VK_SUCCESS) {
        BUG("Failed to create render pass.");
        return RESULT_FAILURE;
    }

    out_render_targets->image_views = (VkImageView*)bump_allocate(&permanent_allocator, alignof(VkImageView), swapchain->image_count * sizeof(VkImageView));
    if (out_render_targets->image_views == NULL) {
        BUG("Failed to allocate memory for image views.");
        return RESULT_FAILURE;
    }

    for (uint32_t i = 0; i < swapchain->image_count; ++i) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchain->image_format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        if (vkCreateImageView(device, &view_info, NULL, &out_render_targets->image_views[i]) != VK_SUCCESS) {
            BUG("Failed to create image view.");
            return RESULT_FAILURE;
        }
    }

    out_render_targets->framebuffers = (VkFramebuffer*)bump_allocate(&permanent_allocator, alignof(VkFramebuffer), swapchain->image_count * sizeof(VkFramebuffer));
    if (out_render_targets->framebuffers == NULL) {
        BUG("Failed to allocate memory for framebuffers.");
        return RESULT_FAILURE;
    }

    for (uint32_t i = 0; i < swapchain->image_count; ++i) {
        VkImageView attachments[] = { out_render_targets->image_views[i] };
        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = out_render_targets->render_pass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = swapchain->extent.width,
            .height = swapchain->extent.height,
            .layers = 1,
        };

        if (vkCreateFramebuffer(device, &framebuffer_info, NULL, &out_render_targets->framebuffers[i]) != VK_SUCCESS) {
            BUG("Failed to create framebuffer.");
            return RESULT_FAILURE;
        }
    }

    return RESULT_SUCCESS;
}

static result create_graphics_pipeline(vulkan_device* device, vulkan_swapchain* swapchain, vulkan_render_targets* render_targets, vulkan_graphics_pipeline* out_graphics_pipeline) {
    ASSERT(device != NULL, return RESULT_FAILURE, "Device pointer cannot be NULL");
    ASSERT(device->handle != VK_NULL_HANDLE, return RESULT_FAILURE, "Device handle cannot be NULL");
    ASSERT(device->physical_device != VK_NULL_HANDLE, return RESULT_FAILURE, "Physical device handle cannot be NULL");
    ASSERT(swapchain != NULL, return RESULT_FAILURE, "Swapchain pointer cannot be NULL");
    ASSERT(render_targets != NULL, return RESULT_FAILURE, "Render targets pointer cannot be NULL");
    ASSERT(out_graphics_pipeline != NULL, return RESULT_FAILURE, "Output graphics pipeline pointer cannot be NULL");

    // Create the pipeline layout:

    VkDescriptorSetLayoutBinding texture_sampler_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = NULL,
    };

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &texture_sampler_binding,
    };

    if (vkCreateDescriptorSetLayout(device->handle, &descriptor_set_layout_info, NULL, &out_graphics_pipeline->descriptor_set_layout) != VK_SUCCESS) {
        BUG("Failed to create descriptor set layout.");
        return RESULT_FAILURE;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &out_graphics_pipeline->descriptor_set_layout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL,
    };

    if (vkCreatePipelineLayout(device->handle, &pipeline_layout_info, NULL, &out_graphics_pipeline->layout) != VK_SUCCESS) {
        BUG("Failed to create pipeline layout.");
        return RESULT_FAILURE;
    }

    // Create the shader modules for the graphics pipeline:
    VkShaderModuleCreateInfo vert_create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(compiled_vert_shader_spv),
        .pCode = (const uint32_t*)compiled_vert_shader_spv,
    };

    if (vkCreateShaderModule(device->handle, &vert_create_info, NULL, &out_graphics_pipeline->vert_shader) != VK_SUCCESS) {
        BUG("Failed to create vertex shader module.");
        return RESULT_FAILURE;
    }

    VkShaderModuleCreateInfo frag_create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(compiled_frag_shader_spv),
        .pCode = (const uint32_t*)compiled_frag_shader_spv,
    };

    if (vkCreateShaderModule(device->handle, &frag_create_info, NULL, &out_graphics_pipeline->frag_shader) != VK_SUCCESS) {
        BUG("Failed to create fragment shader module.");
        vkDestroyShaderModule(device->handle, out_graphics_pipeline->vert_shader, NULL);
        return RESULT_FAILURE;
    }

    // Shader stages:
    VkPipelineShaderStageCreateInfo shader_stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = out_graphics_pipeline->vert_shader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = out_graphics_pipeline->frag_shader,
            .pName = "main",
        }
    };

    // Vertex input state:
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkVertexInputBindingDescription vertex_binding_description = {
        .binding = 0,
        .stride = sizeof(float) * 4, // Assuming each vertex has 2 position floats and 2 UV floats
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription vertex_attribute_descriptions[2] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT, // vec2 position
            .offset = 0,
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT, // vec2 UV
            .offset = sizeof(float) * 2,
        }
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_binding_description,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = vertex_attribute_descriptions,
    };

    // Viewport and scissor:
    // Viewport is how the image is stretched to fit the window
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)swapchain->extent.width,
        .height = (float)swapchain->extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    // Scissor is how the image is cropped to fit the window
    VkRect2D scissor = {
        .offset = { 0, 0 },
        .extent = swapchain->extent,
    };

    VkPipelineViewportStateCreateInfo viewport_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };

    // Rasterizer:
    VkPipelineRasterizationStateCreateInfo rasterizer_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    // Color ourput info:

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo color_blend_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };

    VkPipelineMultisampleStateCreateInfo multisampling_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state_info,
        .pRasterizationState = &rasterizer_info,
        .pMultisampleState = &multisampling_info,
        .pColorBlendState = &color_blend_info,
        .pDepthStencilState = NULL, // No depth/stencil buffer
        .pDynamicState = &dynamic_state_info,
        .layout = out_graphics_pipeline->layout,
        .renderPass = render_targets->render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    if (vkCreateGraphicsPipelines(device->handle, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &out_graphics_pipeline->handle) != VK_SUCCESS) {
        BUG("Failed to create graphics pipeline.");
        return RESULT_FAILURE;
    }

    return RESULT_SUCCESS;
}

typedef enum {
    COMMAND_POOL_BUFFERS_RESETABLE = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    COMMAND_POOL_BUFFERS_SHORT_LIVED = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
} command_pool_flags;

static result create_command_pool(VkDevice device, uint32_t queue_family_index, command_pool_flags flags, VkCommandPool* out_command_pool) {
    ASSERT(device != VK_NULL_HANDLE, return RESULT_FAILURE, "Device handle cannot be NULL");
    ASSERT(out_command_pool != NULL, return RESULT_FAILURE, "Output command pool pointer cannot be NULL");

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = flags,
        .queueFamilyIndex = queue_family_index,
    };

    if (vkCreateCommandPool(device, &pool_info, NULL, out_command_pool) != VK_SUCCESS) {
        BUG("Failed to create command pool.");
        return RESULT_FAILURE;
    }

    return RESULT_SUCCESS;
}

typedef enum {
    COMMAND_BUFFER_PRIMARY = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    COMMAND_BUFFER_SECONDARY = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
} command_buffer_type;

static result create_command_buffers(VkDevice device, VkCommandPool command_pool, command_buffer_type buffer_type, uint32_t buffer_count, VkCommandBuffer* out_command_buffers) {
    ASSERT(device != VK_NULL_HANDLE, return RESULT_FAILURE, "Device handle cannot be NULL");
    ASSERT(command_pool != VK_NULL_HANDLE, return RESULT_FAILURE, "Command pool handle cannot be NULL");
    ASSERT(buffer_count > 0, return RESULT_FAILURE, "Buffer count must be greater than zero");
    ASSERT(out_command_buffers != NULL, return RESULT_FAILURE, "Output command buffers pointer cannot be NULL");

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = buffer_count,
    };

    if (vkAllocateCommandBuffers(device, &alloc_info, out_command_buffers) != VK_SUCCESS) {
        BUG("Failed to allocate command buffers.");
        return RESULT_FAILURE;
    }

    return RESULT_SUCCESS;
}

static result create_descriptor_pool(VkDevice device, VkDescriptorType descriptor_type, uint32_t max_sets, VkDescriptorPool* out_descriptor_pool) {
    ASSERT(device != VK_NULL_HANDLE, return RESULT_FAILURE, "Device handle cannot be NULL");
    ASSERT(max_sets > 0, return RESULT_FAILURE, "Max sets must be greater than zero");
    ASSERT(out_descriptor_pool != NULL, return RESULT_FAILURE, "Output descriptor pool pointer cannot be NULL");

    VkDescriptorPoolSize pool_size = {
        .type = descriptor_type,
        .descriptorCount = max_sets,
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
        .maxSets = max_sets,
    };

    if (vkCreateDescriptorPool(device, &pool_info, NULL, out_descriptor_pool) != VK_SUCCESS) {
        BUG("Failed to create descriptor pool.");
        return RESULT_FAILURE;
    }

    return RESULT_SUCCESS;
}

static result create_descriptor_sets(VkDevice device, VkDescriptorPool descriptor_pool, VkDescriptorSetLayout* layouts, uint32_t set_count, VkDescriptorSet* out_descriptor_sets) {
    ASSERT(device != VK_NULL_HANDLE, return RESULT_FAILURE, "Device handle cannot be NULL");
    ASSERT(descriptor_pool != VK_NULL_HANDLE, return RESULT_FAILURE, "Descriptor pool handle cannot be NULL");
    ASSERT(set_count > 0, return RESULT_FAILURE, "Set count must be greater than zero");
    ASSERT(layouts != NULL, return RESULT_FAILURE, "Layouts pointer cannot be NULL");
    ASSERT(out_descriptor_sets != NULL, return RESULT_FAILURE, "Output descriptor sets pointer cannot be NULL");

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = set_count,
        .pSetLayouts = layouts,
    };

    if (vkAllocateDescriptorSets(device, &alloc_info, out_descriptor_sets) != VK_SUCCESS) {
        BUG("Failed to allocate descriptor sets.");
        return RESULT_FAILURE;
    }

    return RESULT_SUCCESS;
}

static result begin_single_time_commands(VkDevice device, VkCommandPool command_pool, VkCommandBuffer* out_command_buffer) {
    ASSERT(device != VK_NULL_HANDLE, return RESULT_FAILURE, "Device handle cannot be NULL");
    ASSERT(command_pool != VK_NULL_HANDLE, return RESULT_FAILURE, "Command pool handle cannot be NULL");
    ASSERT(out_command_buffer != NULL, return RESULT_FAILURE, "Output command buffer pointer cannot be NULL");

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = command_pool,
        .commandBufferCount = 1,
    };

    if (vkAllocateCommandBuffers(device, &alloc_info, out_command_buffer) != VK_SUCCESS) {
        BUG("Failed to allocate command buffer.");
        return RESULT_FAILURE;
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vkBeginCommandBuffer(*out_command_buffer, &begin_info) != VK_SUCCESS) {
        BUG("Failed to begin command buffer.");
        return RESULT_FAILURE;
    }

    return RESULT_SUCCESS;
}

static result end_single_time_commands(VkDevice device, VkCommandPool command_pool, VkQueue queue, VkCommandBuffer command_buffer) {
    ASSERT(device != VK_NULL_HANDLE, return RESULT_FAILURE, "Device handle cannot be NULL");
    ASSERT(command_pool != VK_NULL_HANDLE, return RESULT_FAILURE, "Command pool handle cannot be NULL");
    ASSERT(queue != VK_NULL_HANDLE, return RESULT_FAILURE, "Queue handle cannot be NULL");
    ASSERT(command_buffer != VK_NULL_HANDLE, return RESULT_FAILURE, "Command buffer handle cannot be NULL");

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        BUG("Failed to end command buffer.");
        return RESULT_FAILURE;
    }

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };

    if (vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
        BUG("Failed to submit command buffer.");
        return RESULT_FAILURE;
    }

    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);

    return RESULT_SUCCESS;
}

static result submit_image_transition_command(VkDevice device, VkCommandPool command_pool, VkQueue command_queue, VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, VkAccessFlags available_memory, VkAccessFlags visible_memory, VkPipelineStageFlags dependent_stages, VkPipelineStageFlags output_stages) {
    VkCommandBuffer command_buffer = { 0 };
    if (begin_single_time_commands(device, command_pool, &command_buffer) != RESULT_SUCCESS) {
        return RESULT_FAILURE;
    }

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcAccessMask = available_memory,
        .dstAccessMask = visible_memory,
    };

    vkCmdPipelineBarrier(
        command_buffer,
        dependent_stages,
        output_stages,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );

    end_single_time_commands(device, command_pool, command_queue, command_buffer);
    return RESULT_SUCCESS;
}

static result submit_buffer_to_image_command(VkDevice device, VkCommandPool command_pool, VkQueue command_queue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer command_buffer = { 0 };
    if (begin_single_time_commands(device, command_pool, &command_buffer) != RESULT_SUCCESS) {
        return RESULT_FAILURE;
    }

    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = { 0, 0, 0 },
        .imageExtent = {
            .width = width,
            .height = height,
            .depth = 1,
        },
    };

    vkCmdCopyBufferToImage(
        command_buffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    end_single_time_commands(device, command_pool, command_queue, command_buffer);
    return RESULT_SUCCESS;
}

static result find_memory_type(VkPhysicalDevice physical_device, VkMemoryPropertyFlags required_property_flags, uint32_t type_filter, uint32_t* out_memory_type_index) {
    ASSERT(physical_device != VK_NULL_HANDLE, return RESULT_FAILURE, "Physical device handle cannot be NULL");
    ASSERT(out_memory_type_index != NULL, return RESULT_FAILURE, "Output memory type index pointer cannot be NULL");

    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & required_property_flags) == required_property_flags) {
            *out_memory_type_index = i;
            return RESULT_SUCCESS;
        }
    }

    BUG("Failed to find suitable memory type.");
    return RESULT_FAILURE;
}

static result create_buffer(VkDevice device, VkPhysicalDevice physical_device, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, VkDeviceSize size, VkBuffer* out_buffer, VkDeviceMemory* out_buffer_memory) {
    ASSERT(device != VK_NULL_HANDLE, return RESULT_FAILURE, "Device handle cannot be NULL");
    ASSERT(physical_device != VK_NULL_HANDLE, return RESULT_FAILURE, "Physical device handle cannot be NULL");
    ASSERT(size > 0, return RESULT_FAILURE, "Buffer size must be greater than zero");
    ASSERT(out_buffer != NULL, return RESULT_FAILURE, "Output buffer pointer cannot be NULL");
    ASSERT(out_buffer_memory != NULL, return RESULT_FAILURE, "Output buffer memory pointer cannot be NULL");

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage_flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    if (vkCreateBuffer(device, &buffer_info, NULL, out_buffer) != VK_SUCCESS) {
        BUG("Failed to create buffer.");
        return RESULT_FAILURE;
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device, *out_buffer, &mem_requirements);

    uint32_t memory_type_index;
    if (find_memory_type(physical_device, property_flags, mem_requirements.memoryTypeBits, &memory_type_index) != RESULT_SUCCESS) {
        vkDestroyBuffer(device, *out_buffer, NULL);
        return RESULT_FAILURE;
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = memory_type_index,
    };

    if (vkAllocateMemory(device, &alloc_info, NULL, out_buffer_memory) != VK_SUCCESS) {
        BUG("Failed to allocate buffer memory.");
        vkDestroyBuffer(device, *out_buffer, NULL);
        return RESULT_FAILURE;
    }

    vkBindBufferMemory(device, *out_buffer, *out_buffer_memory, 0);
    return RESULT_SUCCESS;
}

static result create_image(vulkan_device* device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* out_image, VkDeviceMemory* out_image_memory) {
    ASSERT(device != NULL, return RESULT_FAILURE, "Device pointer cannot be NULL");
    ASSERT(device->handle != VK_NULL_HANDLE, return RESULT_FAILURE, "Device handle cannot be NULL");
    ASSERT(width > 0 && height > 0, return RESULT_FAILURE, "Image dimensions must be greater than zero");
    ASSERT(out_image != NULL, return RESULT_FAILURE, "Output image pointer cannot be NULL");
    ASSERT(out_image_memory != NULL, return RESULT_FAILURE, "Output image memory pointer cannot be NULL");

    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent = {
            .width = width,
            .height = height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = format,
        .tiling = tiling,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .flags = 0,
    };

    if (vkCreateImage(device->handle, &image_info, NULL, out_image) != VK_SUCCESS) {
        BUG("Failed to create image.");
        return RESULT_FAILURE;
    }

    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device->handle, *out_image, &mem_requirements);

    uint32_t memory_type_index;
    if (find_memory_type(device->physical_device, properties, mem_requirements.memoryTypeBits, &memory_type_index) != RESULT_SUCCESS) {
        vkDestroyImage(device->handle, *out_image, NULL);
        return RESULT_FAILURE;
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = memory_type_index,
    };

    if (vkAllocateMemory(device->handle, &alloc_info, NULL, out_image_memory) != VK_SUCCESS) {
        BUG("Failed to allocate image memory.");
        vkDestroyImage(device->handle, *out_image, NULL);
        return RESULT_FAILURE;
    }

    vkBindImageMemory(device->handle, *out_image, *out_image_memory, 0);
    return RESULT_SUCCESS;
}

static result upload_texture_to_gpu(vulkan_device* device, VkCommandPool command_pool, VkQueue queue, texture* texture, VkFormat format, VkImageTiling tiling, VkImage* out_image, VkDeviceMemory* out_memory) {
    ASSERT(device != NULL, return RESULT_FAILURE, "Device pointer cannot be NULL");
    ASSERT(device->handle != VK_NULL_HANDLE, return RESULT_FAILURE, "Device handle cannot be NULL");
    ASSERT(command_pool != VK_NULL_HANDLE, return RESULT_FAILURE, "Command pool handle cannot be NULL");
    ASSERT(queue != VK_NULL_HANDLE, return RESULT_FAILURE, "Queue handle cannot be NULL");
    ASSERT(texture != NULL, return RESULT_FAILURE, "Texture pointer cannot be NULL");
    ASSERT(texture->width > 0 && texture->height > 0, return RESULT_FAILURE, "Texture dimensions must be greater than zero");
    ASSERT(texture->pixels != NULL, return RESULT_FAILURE, "Texture data pointer cannot be NULL");
    ASSERT(out_image != NULL, return RESULT_FAILURE, "Output image pointer cannot be NULL");
    ASSERT(out_memory != NULL, return RESULT_FAILURE, "Output image memory pointer cannot be NULL");

    result result = RESULT_SUCCESS;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory buffer_memory = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory image_memory = VK_NULL_HANDLE;

    if (create_buffer(device->handle, device->physical_device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, texture->width * texture->height * 4, &buffer, &buffer_memory) != RESULT_SUCCESS) {
        BUG("Failed to create staging buffer for texture upload.");
        result = RESULT_FAILURE;
        goto cleanup;
    }

    if (create_image(device, texture->width, texture->height, format, tiling, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &image, &image_memory) != RESULT_SUCCESS) {
        BUG("Failed to create image for texture upload.");
        result = RESULT_FAILURE;
        goto cleanup;
    }

    if (submit_image_transition_command(device->handle, command_pool, queue, image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT) != RESULT_SUCCESS) {
        BUG("Failed to transition image layout for texture upload.");
        result = RESULT_FAILURE;
        goto cleanup;
    }

    if (submit_buffer_to_image_command(device->handle, command_pool, queue, buffer, image, texture->width, texture->height) != RESULT_SUCCESS) {
        BUG("Failed to copy buffer to image for texture upload.");
        result = RESULT_FAILURE;
        goto cleanup;
    }

    if (submit_image_transition_command(device->handle, command_pool, queue, image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) != RESULT_SUCCESS) {
        BUG("Failed to transition image layout for shader read.");
        result = RESULT_FAILURE;
        goto cleanup;
    }

cleanup:
    *out_image = image;
    *out_memory = image_memory;
    vkFreeMemory(device->handle, buffer_memory, NULL);
    vkDestroyBuffer(device->handle, buffer, NULL);
    return result;
}

static result create_frame_executions(vulkan_device* device, VkCommandPool command_pool, uint32_t frame_count, vulkan_frame_execution* out_frame_executions) {
    ASSERT(device != NULL, return RESULT_FAILURE, "Device pointer cannot be NULL");
    ASSERT(device->handle != VK_NULL_HANDLE, return RESULT_FAILURE, "Device handle cannot be NULL");
    ASSERT(command_pool != VK_NULL_HANDLE, return RESULT_FAILURE, "Command pool handle cannot be NULL");
    ASSERT(frame_count > 0, return RESULT_FAILURE, "Frame count must be greater than zero");
    ASSERT(out_frame_executions != NULL, return RESULT_FAILURE, "Output frame executions pointer cannot be NULL");

    for (uint32_t i = 0; i < frame_count; ++i) {
        if (create_command_pool(device->handle, device->queue_family_indices.graphics_family], COMMAND_POOL_BUFFERS_RESETABLE, &out_frame_executions[i].command_pool) != RESULT_SUCCESS) {
            BUG("Failed to create command pool for frame execution.");
            return RESULT_FAILURE;
        }

        if (create_command_buffers(device->handle, command_pool, COMMAND_BUFFER_PRIMARY, 1, &out_frame_executions[i].command_buffer) != RESULT_SUCCESS) {
            BUG("Failed to create command buffer for frame execution.");
            return RESULT_FAILURE;
        }

        // Semaphores are used to implement task precedence on the GPU, such as A must occur before B, which must occur before C.
        VkSemaphoreCreateInfo semaphore_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        if (vkCreateSemaphore(device->handle, &semaphore_info, NULL, &out_frame_executions[i].image_available_semaphore) != VK_SUCCESS) {
            BUG("Failed to create image available semaphore for frame execution.");
            return RESULT_FAILURE;
        }

        if (vkCreateSemaphore(device->handle, &semaphore_info, NULL, &out_frame_executions[i].render_finished_semaphore) != VK_SUCCESS) {
            BUG("Failed to create render finished semaphore for frame execution.");
            return RESULT_FAILURE;
        }

        // Fences are used to block the CPU while we wait for tasks on the GPU to finish. 
        VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        if (vkCreateFence(device->handle, &fence_info, NULL, &out_frame_executions[i].in_flight_fence) != VK_SUCCESS) {
            BUG("Failed to create in-flight fence for frame execution.");
            return RESULT_FAILURE;
        }
    }

    return RESULT_SUCCESS;
}


STATIC_ASSERT(sizeof(sprite_renderer) == sizeof(sprite_renderer_internals), renderer_struct_too_small);
STATIC_ASSERT(alignof(sprite_renderer) >= alignof(sprite_renderer_internals), renderer_struct_misaligned);

result create_sprite_renderer(window* window, sprite_renderer* out_renderer, texture texture) {
    ASSERT(window != NULL, return RESULT_FAILURE, "Window pointer cannot be NULL");
    ASSERT(out_renderer != NULL, return RESULT_FAILURE, "Output renderer pointer cannot be NULL");
    memset(out_renderer, 0, sizeof(*out_renderer));
    sprite_renderer_internals* r = (sprite_renderer_internals*)&out_renderer->internals;

    // Create the Vulkan instance
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Game Overlord",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    // Get required extensions for window system integration
#ifdef _WIN32
    const char* extensions[] = { "VK_KHR_surface", "VK_KHR_win32_surface" };
    const uint32_t extension_count = 2;
#else
#error Unsupported platform for Vulkan, add the code to get required window extensions here.
#endif

    // If in debug mode, enable validation layers
#ifdef NDEBUG
    const uint32_t layer_count = 0;
    const char* const* layers = NULL;
#else
    const uint32_t layer_count = 1;
    const char* const layers[] = { "VK_LAYER_KHRONOS_validation" };
#endif

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions,
        .enabledLayerCount = layer_count,
        .ppEnabledLayerNames = layers,
    };

    if (vkCreateInstance(&create_info, NULL, &r->instance) != VK_SUCCESS) {
        BUG("Failed to create Vulkan instance.");
        return RESULT_FAILURE;
    }

    r->window.window = window;
    r->window.surface = create_window_surface(r->instance, window);
    if (r->window.surface == VK_NULL_HANDLE) {
        BUG("Failed to create window surface.");
        return RESULT_FAILURE;
    }

    if (create_logical_device(r->instance, r->window.surface, &r->device) != RESULT_SUCCESS) {
        BUG("Failed to create logical device.");
        return RESULT_FAILURE;
    }

    if (create_swapchain(&r->device, &r->window, &r->swapchain) != RESULT_SUCCESS) {
        BUG("Failed to create swapchain.");
        return RESULT_FAILURE;
    }

    if (create_render_targets(r->device.handle, &r->swapchain, &r->render_targets) != RESULT_SUCCESS) {
        BUG("Failed to create render targets.");
        return RESULT_FAILURE;
    }

    if (create_graphics_pipeline(&r->device, &r->swapchain, &r->render_targets, &r->graphics_pipeline) != RESULT_SUCCESS) {
        BUG("Failed to create graphics pipeline.");
        return RESULT_FAILURE;
    }

    if (create_command_pool(r->device.handle, r->device.graphics_queue, COMMAND_POOL_BUFFERS_RESETABLE, &r->graphics_command_pool) != RESULT_SUCCESS) {
        BUG("Failed to create command pool.");
        return RESULT_FAILURE;
    }

    if (upload_texture_to_gpu(&r->device, r->graphics_command_pool, r->device.graphics_queue, &texture, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, &r->sprite_sheet_image, &r->sprite_sheet_memory) != RESULT_SUCCESS) {
        BUG("Failed to upload texture to GPU.");
        return RESULT_FAILURE;
    }

    if (create_frame_executions(&r->device, r->graphics_command_pool, r->swapchain.image_count, r->frame_executions) != RESULT_SUCCESS) {
        BUG("Failed to create frame executions.");
        return RESULT_FAILURE;
    }

    return RESULT_SUCCESS;
}

void destroy_sprite_renderer(sprite_renderer* renderer) {
    ASSERT(renderer != NULL, return, "Renderer pointer cannot be NULL");
    sprite_renderer_internals* r = (sprite_renderer_internals*)&renderer->internals;

    for (uint32_t i = 0; i < r->swapchain.image_count; ++i) {
        vkDestroyFence(r->device.handle, r->frame_executions[i].in_flight_fence, NULL);
        vkDestroySemaphore(r->device.handle, r->frame_executions[i].render_finished_semaphore, NULL);
        vkDestroySemaphore(r->device.handle, r->frame_executions[i].image_available_semaphore, NULL);
    }

    vkDestroyCommandPool(r->device.handle, r->graphics_command_pool, NULL);
    vkDestroyImage(r->device.handle, r->sprite_sheet_image, NULL);
    vkFreeMemory(r->device.handle, r->sprite_sheet_memory, NULL);

    vkDestroyPipelineLayout(r->device.handle, r->graphics_pipeline.layout, NULL);
    vkDestroyShaderModule(r->device.handle, r->graphics_pipeline.frag_shader, NULL);
    vkDestroyShaderModule(r->device.handle, r->graphics_pipeline.vert_shader, NULL);

    for (uint32_t i = 0; i < r->swapchain.image_count; ++i) {
        vkDestroyFramebuffer(r->device.handle, r->render_targets.framebuffers[i], NULL);
        vkDestroyImageView(r->device.handle, r->render_targets.image_views[i], NULL);
    }

    vkDestroyRenderPass(r->device.handle, r->render_targets.render_pass, NULL);
    vkDestroySwapchainKHR(r->device.handle, r->swapchain.handle, NULL);
    vkDestroySurfaceKHR(r->instance, r->window.surface, NULL);
    vkDestroyDevice(r->device.handle, NULL);
    vkDestroyInstance(r->instance, NULL);
    memset(renderer, 0, sizeof(*renderer));
}






