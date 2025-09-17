#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <vulkan/vulkan.h>
#ifdef _WIN32
#include <vulkan/vulkan_win32.h>
#endif
#include "platform_layer.h"


// public interface (just copy this to the platform layer .c file)
result create_renderer(window* window, renderer* out_renderer);
void destroy_renderer(renderer* renderer);

// Needs to be implemented in platform layer .c file.
HWND get_window_handle(window* w);

// Implementation details:

typedef struct {
    uint32_t graphics_family;
    uint32_t present_family;
    uint32_t transfer_family;
} queue_family_indices;

typedef struct renderer {
    VkInstance instance;
    VkSurfaceKHR window_surface;
    VkPhysicalDevice physical_device;
    queue_family_indices queue_family_indices;
    VkDevice device;
    VkSwapchainKHR swapchain;
    VkRenderPass render_pass;
    VkPipeline graphics_pipeline;
    VkFramebuffer framebuffer;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
} renderer;

static VkSurfaceKHR create_window_surface(VkInstance instance, window* w) {
    ASSERT(instance != VK_NULL_HANDLE, return VK_NULL_HANDLE, "Vulkan instance cannot be NULL");
    ASSERT(w != NULL, return VK_NULL_HANDLE, "Window pointer cannot be NULL");
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = GetModuleHandle(NULL),
        .hwnd = get_window_handle(w),
    };
#else
#error Unsupported platform for Vulkan, add the code to create a window surface here.
#endif
    VkSurfaceKHR surface;
    if (vkCreateWin32SurfaceKHR(instance, &create_info, NULL, &surface) != VK_SUCCESS) {
        BUG("Failed to create Win32 Vulkan surface.");
        return VK_NULL_HANDLE;
    }
    return surface;
}

// Rate the physical device, score of 0 means the device is not suitable.
static uint32_t rate_physical_device(VkPhysicalDevice device, VkSurfaceKHR window_surface, queue_family_indices* out_queue_families) {
    ASSERT(device != VK_NULL_HANDLE, return 0, "Physical device cannot be NULL");
    ASSERT(window_surface != VK_NULL_HANDLE, return 0, "Window surface cannot be NULL");
    ASSERT(out_queue_families != NULL, return 0, "Output queue family indices cannot be NULL");

    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);
    // VkPhysicalDeviceFeatures device_features;
    // vkGetPhysicalDeviceFeatures(device, &device_features);

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

        if (vkGetPhysicalDeviceWin32PresentationSupportKHR(device, i)) {
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

static result select_physical_device(renderer* renderer, VkSurfaceKHR window_surface, VkPhysicalDevice* out_physical_device, queue_family_indices* out_queue_families) {
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

result create_renderer(window* window, renderer* out_renderer) {
    ASSERT(window != NULL, return RESULT_FAILURE, "Window pointer cannot be NULL");
    ASSERT(out_renderer != NULL, return RESULT_FAILURE, "Output renderer pointer cannot be NULL");
    memset(out_renderer, 0, sizeof(*out_renderer));

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

    if (vkCreateInstance(&create_info, NULL, &out_renderer->instance) != VK_SUCCESS) {
        BUG("Failed to create Vulkan instance.");
        return RESULT_FAILURE;
    }

    out_renderer->window_surface = create_window_surface(out_renderer->instance, window);
    if (out_renderer->window_surface == VK_NULL_HANDLE) {
        BUG("Failed to create window surface.");
        return RESULT_FAILURE;
    }

    if (select_physical_device(out_renderer, out_renderer->window_surface, &out_renderer->physical_device, &out_renderer->queue_family_indices) != RESULT_SUCCESS) {
        BUG("Failed to select a suitable physical device.");
        return RESULT_FAILURE;
    }

    return RESULT_SUCCESS;
}

void destroy_renderer(renderer* renderer) {
    ASSERT(renderer != NULL, return, "Renderer pointer cannot be NULL");
    vkDestroySurfaceKHR(renderer->instance, renderer->window_surface, NULL);
    vkDestroyInstance(renderer->instance, NULL);
    memset(renderer, 0, sizeof(*renderer));
}






