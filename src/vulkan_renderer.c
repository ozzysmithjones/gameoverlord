#include <vulkan/vulkan.h>
#include "platform_layer.h"
#include "renderer.h"

// public interface (just copy this to the platform layer .c file)
result create_renderer(window* window, renderer* out_renderer);
void destroy_renderer(renderer* renderer);

// These function needs to be implemented in the platform layer .c file.
VkSurfaceKHR create_window_surface(VkInstance instance, window* window);
bool check_window_presentation_support(VkPhysicalDevice device, uint32_t queue_family_index, VkSurfaceKHR window_surface);

// Implementation details:

typedef struct {
    uint32_t graphics_family;
    uint32_t present_family;
    uint32_t transfer_family;
} queue_family_indices;

typedef struct {
    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue transfer_queue;
} queues;


typedef struct {
    VkFormat image_format;
    VkExtent2D extent;
    uint32_t image_count;
    VkImage* images;
    VkSwapchainKHR handle;
} swapchain;

typedef struct {
    VkRenderPass render_pass;
    VkImageView* image_views;
    VkFramebuffer* framebuffers;
} render_targets;

typedef struct {
    VkInstance instance;
    VkSurfaceKHR window_surface;
    VkPhysicalDevice physical_device;
    queue_family_indices queue_family_indices;
    VkDevice device;
    queues queues;
    swapchain swapchain;
    render_targets render_targets;
    VkPipeline graphics_pipeline;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
} renderer_internals;

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
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, window_surface, &format_count, NULL);
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
    bool one_family_graphics_and_present = false;

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

    // search for a queue family that supports transfer operations. 
    // We priotize a queue family that we haven't picked before for graphics or present, but if we don't find one, it's not a big deal.
    out_queue_families->transfer_family = out_queue_families->graphics_family; // Default to graphics family if no better option is found.
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if ((queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && (i != out_queue_families->graphics_family) && (i != out_queue_families->present_family)) {
            out_queue_families->transfer_family = i;
            break;
        }
    }

    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);

    // Rate discrete GPUs higher than integrated GPUs
    uint32_t score = 0;
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

static result select_physical_device(renderer_internals* renderer, VkSurfaceKHR window_surface, VkPhysicalDevice* out_physical_device, queue_family_indices* out_queue_families) {
    ASSERT(renderer != NULL, return RESULT_FAILURE, "Renderer pointer cannot be NULL");
    ASSERT(window_surface != VK_NULL_HANDLE, return RESULT_FAILURE, "Window surface cannot be NULL");
    ASSERT(out_physical_device != NULL, return RESULT_FAILURE, "Output physical device pointer cannot be NULL");
    ASSERT(out_queue_families != NULL, return RESULT_FAILURE, "Output queue family indices cannot be NULL");

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(renderer->instance, &device_count, NULL);
    ASSERT(device_count > 0, return RESULT_FAILURE, "No Vulkan physical devices found.");
    VkPhysicalDevice* devices = (VkPhysicalDevice*)bump_allocate(&temp_allocator, alignof(VkPhysicalDevice), device_count * sizeof(VkPhysicalDevice));
    if (devices == NULL) {
        BUG("Failed to allocate memory for physical device list.");
        return RESULT_FAILURE;
    }
    vkEnumeratePhysicalDevices(renderer->instance, &device_count, devices);

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

static result create_logical_device(renderer_internals* renderer, VkPhysicalDevice physical_device, queue_family_indices queue_family_indices, VkDevice* out_device, queues* out_queues) {
    ASSERT(renderer != NULL, return RESULT_FAILURE, "Renderer pointer cannot be NULL");
    ASSERT(out_device != NULL, return RESULT_FAILURE, "Output device pointer cannot be NULL");
    ASSERT(out_queues != NULL, return RESULT_FAILURE, "Output queues pointer cannot be NULL");

    float queue_priority = 1.0f;
    uint32_t unique_queue_families[3] = { queue_family_indices.graphics_family, queue_family_indices.present_family, queue_family_indices.transfer_family };
    uint32_t unique_queue_family_count = 0;

    // Count unique queue families
    for (uint32_t i = 0; i < 3; ++i) {
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
    if (vkCreateDevice(physical_device, &create_info, NULL, out_device) != VK_SUCCESS) {
        BUG("Failed to create logical device.");
        return RESULT_FAILURE;
    }
    vkGetDeviceQueue(*out_device, queue_family_indices.graphics_family, 0, &out_queues->graphics_queue);
    vkGetDeviceQueue(*out_device, queue_family_indices.present_family, 0, &out_queues->present_queue);
    vkGetDeviceQueue(*out_device, queue_family_indices.transfer_family, 0, &out_queues->transfer_queue);
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

static result create_swapchain(renderer_internals* renderer, window* window, swapchain* out_swapchain) {
    ASSERT(renderer != NULL, return RESULT_FAILURE, "Renderer pointer cannot be NULL");
    ASSERT(window != NULL, return RESULT_FAILURE, "Window pointer cannot be NULL");
    ASSERT(out_swapchain != NULL, return RESULT_FAILURE, "Output swapchain cannot be NULL");

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer->physical_device, renderer->window_surface, &capabilities);

    // Get the supported surface formats
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physical_device, renderer->window_surface, &format_count, NULL);
    ASSERT(format_count > 0, return RESULT_FAILURE, "No surface formats found.");

    VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)bump_allocate(&temp_allocator, alignof(VkSurfaceFormatKHR), format_count * sizeof(VkSurfaceFormatKHR));
    if (formats == NULL) {
        BUG("Failed to allocate memory for surface formats.");
        return RESULT_FAILURE;
    }

    vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physical_device, renderer->window_surface, &format_count, formats);

    // Get the supported present modes
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->physical_device, renderer->window_surface, &present_mode_count, NULL);
    ASSERT(present_mode_count > 0, return RESULT_FAILURE, "No present modes found");

    VkPresentModeKHR* present_modes = (VkPresentModeKHR*)bump_allocate(&temp_allocator, alignof(VkPresentModeKHR), present_mode_count * sizeof(VkPresentModeKHR));
    if (present_modes == NULL) {
        BUG("Failed to allocate memory for present modes.");
        return RESULT_FAILURE;
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->physical_device, renderer->window_surface, &present_mode_count, present_modes);

    // Pick the best present mode and surface format
    VkSurfaceFormatKHR surface_format = pick_surface_format(formats, format_count);
    VkPresentModeKHR present_mode = pick_present_mode(present_modes, present_mode_count);

    VkExtent2D swap_extent = pick_swap_extent(&capabilities, window);
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
        .surface = renderer->window_surface,
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

    uint32_t queue_family_indices[] = { renderer->queue_family_indices.graphics_family, renderer->queue_family_indices.present_family };
    if (renderer->queue_family_indices.graphics_family != renderer->queue_family_indices.present_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0; // Optional
        create_info.pQueueFamilyIndices = NULL; // Optional
    }

    if (vkCreateSwapchainKHR(renderer->device, &create_info, NULL, &out_swapchain->handle) != VK_SUCCESS) {
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

    vkGetSwapchainImagesKHR(renderer->device, out_swapchain->handle, &image_count, out_swapchain->images);
    return RESULT_SUCCESS;
}

static result create_render_pass(renderer_internals* renderer, VkFormat image_format, VkRenderPass* out_render_pass) {
    ASSERT(renderer != NULL, return RESULT_FAILURE, "Renderer pointer cannot be NULL");
    ASSERT(out_render_pass != NULL, return RESULT_FAILURE, "Output render pass pointer cannot be NULL");

    VkAttachmentDescription color_attachment = {
        .format = image_format,
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

    if (vkCreateRenderPass(renderer->device, &render_pass_info, NULL, out_render_pass) != VK_SUCCESS) {
        BUG("Failed to create render pass.");
        return RESULT_FAILURE;
    }

    return RESULT_SUCCESS;
}

static result create_render_targets(renderer_internals* renderer, swapchain* swapchain, render_targets* out_render_targets) {
    ASSERT(renderer != NULL, return RESULT_FAILURE, "Renderer pointer cannot be NULL");
    ASSERT(swapchain != NULL, return RESULT_FAILURE, "Swapchain pointer cannot be NULL");
    ASSERT(out_render_targets != NULL, return RESULT_FAILURE, "Output render targets pointer cannot be NULL");
    ASSERT(swapchain->image_count > 0, return RESULT_FAILURE, "Swapchain must have at least one image.");
    ASSERT(out_render_targets->render_pass != VK_NULL_HANDLE, return RESULT_FAILURE, "Render pass has not been created.");

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

        if (vkCreateImageView(renderer->device, &view_info, NULL, &out_render_targets->image_views[i]) != VK_SUCCESS) {
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

        if (vkCreateFramebuffer(renderer->device, &framebuffer_info, NULL, &out_render_targets->framebuffers[i]) != VK_SUCCESS) {
            BUG("Failed to create framebuffer.");
            return RESULT_FAILURE;
        }
    }

    return RESULT_SUCCESS;
}



STATIC_ASSERT(sizeof(renderer) == sizeof(renderer_internals), renderer_struct_too_small);
STATIC_ASSERT(alignof(renderer) >= alignof(renderer_internals), renderer_struct_misaligned);

result create_renderer(window* window, renderer* out_renderer) {
    ASSERT(window != NULL, return RESULT_FAILURE, "Window pointer cannot be NULL");
    ASSERT(out_renderer != NULL, return RESULT_FAILURE, "Output renderer pointer cannot be NULL");
    memset(out_renderer, 0, sizeof(*out_renderer));
    renderer_internals* r = (renderer_internals*)&out_renderer->internals;

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

    r->window_surface = create_window_surface(r->instance, window);
    if (r->window_surface == VK_NULL_HANDLE) {
        BUG("Failed to create window surface.");
        return RESULT_FAILURE;
    }

    if (select_physical_device(r, r->window_surface, &r->physical_device, &r->queue_family_indices) != RESULT_SUCCESS) {
        BUG("Failed to select a suitable physical device.");
        return RESULT_FAILURE;
    }

    if (create_logical_device(r, r->physical_device, r->queue_family_indices, &r->device, &r->queues) != RESULT_SUCCESS) {
        BUG("Failed to create logical device.");
        return RESULT_FAILURE;
    }

    if (create_swapchain(r, window, &r->swapchain) != RESULT_SUCCESS) {
        BUG("Failed to create swapchain.");
        return RESULT_FAILURE;
    }

    if (create_render_pass(r, r->swapchain.image_format, &r->render_targets.render_pass) != RESULT_SUCCESS) {
        BUG("Failed to create render pass.");
        return RESULT_FAILURE;
    }

    if (create_render_targets(r, &r->swapchain, &r->render_targets) != RESULT_SUCCESS) {
        BUG("Failed to create render targets.");
        return RESULT_FAILURE;
    }

    return RESULT_SUCCESS;
}

void destroy_renderer(renderer* renderer) {
    ASSERT(renderer != NULL, return, "Renderer pointer cannot be NULL");
    renderer_internals* r = (renderer_internals*)&renderer->internals;

    for (uint32_t i = 0; i < r->swapchain.image_count; ++i) {
        vkDestroyFramebuffer(r->device, r->render_targets.framebuffers[i], NULL);
        vkDestroyImageView(r->device, r->render_targets.image_views[i], NULL);
    }

    vkDestroyRenderPass(r->device, r->render_targets.render_pass, NULL);
    vkDestroySwapchainKHR(r->device, r->swapchain.handle, NULL);
    vkDestroySurfaceKHR(r->instance, r->window_surface, NULL);
    vkDestroyDevice(r->device, NULL);
    vkDestroyInstance(r->instance, NULL);
    memset(renderer, 0, sizeof(*renderer));
}






