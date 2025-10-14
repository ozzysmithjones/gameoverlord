#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

// Graphics API:
#define INITGUID
#include <d3d11_1.h>
#include <d3dcompiler.h>
// #include <directxmath.h>
// #include <directxcolors.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "geometry.h"
#include "platform_layer.h"

//#define APP_BACKEND
#ifdef APP_BACKEND

typedef result(*init_function)(init_in_params* params, init_out_params* out_params);
typedef result(*update_function)(update_params* params);
typedef void (*shutdown_function)(shutdown_params* params);

//#define HOT_RELOAD_HOST
#ifdef HOT_RELOAD_HOST

// Function pointers for linking dynamically with hot reloading:
init_function init;
update_function update;
shutdown_function shutdown;

// DLL path for hot reload:
#ifndef HOT_RELOAD_DLL_PATH
#define HOT_RELOAD_DLL_PATH "game.dll"
#define HOT_RELOAD_DLL_TEMP_PATH HOT_RELOAD_DLL_PATH ".temp"
#endif

#else
// Function declarations for static linking without hot reloading:
extern result init(init_in_params* in, init_out_params* out);
extern result update(update_params* in);
extern void shutdown(shutdown_params* in);
#endif // HOT_RELOAD_HOST
#endif // APP_BACKEND

result create_bump_allocator(bump_allocator* allocator, size_t capacity) {
    ASSERT(allocator != NULL, return RESULT_FAILURE, "Allocator cannot be NULL");

    allocator->base = VirtualAlloc(NULL, capacity, MEM_RESERVE, PAGE_READWRITE);
    allocator->used_bytes = 0;
    allocator->next_page_bytes = 0;
    allocator->capacity = capacity;

    if (!allocator->base) {
        BUG("Failed to reserve virtual memory for bump allocator. Error: %d", GetLastError());
        return RESULT_FAILURE;
    }

    return RESULT_SUCCESS;
}

void destroy_bump_allocator(bump_allocator* allocator) {
    ASSERT(allocator != NULL, return, "Allocator cannot be NULL");
    if (allocator->base) {
        VirtualFree(allocator->base, 0, MEM_RELEASE);
        memset(allocator, 0, sizeof(*allocator));
    }
}

void* bump_allocate(bump_allocator* allocator, size_t alignment, size_t bytes) {
    ASSERT(allocator != NULL, return NULL, "Allocator cannot be NULL");
    ASSERT(alignment && (alignment & (alignment - 1)) == 0, alignment = 1, "alignment must be a power of two");

    size_t aligned = (allocator->used_bytes + (alignment - 1)) & ~(alignment - 1);
    size_t new_used_bytes = aligned + bytes;
    if (new_used_bytes > allocator->capacity) {
        BUG("Out of memory for bump allocator.");
        return NULL;
    }

    if (new_used_bytes > allocator->next_page_bytes) {
        SYSTEM_INFO native_memory_info;
        GetSystemInfo(&native_memory_info);

        // Need to commit more memory.
        SIZE_T commit_size = ((new_used_bytes - allocator->next_page_bytes) + (native_memory_info.dwPageSize - 1)) & ~(native_memory_info.dwPageSize - 1);
        if (VirtualAlloc((LPBYTE)allocator->base + allocator->next_page_bytes, commit_size, MEM_COMMIT, PAGE_READWRITE) == NULL) {
            BUG("Failed to commit more memory for bump allocator.");
            return NULL;
        }

        allocator->next_page_bytes += commit_size;
    }

    allocator->used_bytes = new_used_bytes;
    return (LPBYTE)allocator->base + aligned;
}

result create_clock(clock* clock) {
    ASSERT(clock != NULL, return RESULT_FAILURE, "Clock cannot be NULL");

    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency)) {
        BUG("Failed to query performance frequency.");
        return RESULT_FAILURE;
    }

    LARGE_INTEGER current_time;
    if (!QueryPerformanceCounter(&current_time)) {
        BUG("Failed to query performance counter.");
        return RESULT_FAILURE;
    }

    clock->frequency = (float)frequency.QuadPart;
    clock->creation_time = (float)current_time.QuadPart;
    clock->time_since_creation = 0.0f;
    clock->time_since_previous_update = 0.0f;

    return RESULT_SUCCESS;
}

void update_clock(clock* clock) {
    ASSERT(clock != NULL, return, "Clock cannot be NULL");

    LARGE_INTEGER current_time;
    if (!QueryPerformanceCounter(&current_time)) {
        BUG("Failed to query performance counter.");
        return;
    }

    float current_time_seconds = (float)current_time.QuadPart / clock->frequency;
    clock->time_since_previous_update = current_time_seconds - clock->previous_update_time;
    clock->time_since_creation = current_time_seconds - clock->creation_time;
    clock->previous_update_time = current_time_seconds;
}

string get_executable_directory(bump_allocator* allocator) {
    char* path = (char*)bump_allocate(allocator, 1, MAX_PATH);
    if (path == NULL) {
        BUG("Failed to allocate memory for executable path.");
        return (string) {
            .text = "", .length = 0
        };
    }
    DWORD length = GetModuleFileNameA(NULL, path, MAX_PATH);
    allocator->used_bytes -= (MAX_PATH - length - 1); // Free unused memory from bump allocator.

    // cut the executable name from the path:
    for (DWORD i = length; i > 0; --i) {
        if (path[i - 1] == '\\' || path[i - 1] == '/') {
            length = i;
            break;
        }
    }

    if (length == 0 || length == MAX_PATH) {
        BUG("Failed to get executable path.");
        return (string) {
            .text = "", .length = 0
        };
    }
    return (string) {
        .text = path, .length = length
    };
}

result find_first_file_with_extension(string directory, string extension, bump_allocator* allocator, string* out_full_path) {
    ASSERT(allocator != NULL, return RESULT_FAILURE, "Allocator cannot be NULL");
    ASSERT(out_full_path != NULL, return RESULT_FAILURE, "Output full path cannot be NULL");
    ASSERT(extension.length > 0, return RESULT_FAILURE, "Extension cannot be empty");
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%.*s*%.*s", directory.length, directory.text, extension.length, extension.text);
    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        BUG("Failed to find any files in directory: %.*s", directory.length, directory.text);
        return RESULT_FAILURE;
    }

    do {

        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            // Found a file, construct the full path:
            size_t full_path_length = directory.length + strlen(find_data.cFileName);
            char* full_path = (char*)bump_allocate(allocator, 1, full_path_length + 1);
            if (full_path == NULL) {
                BUG("Failed to allocate memory for full path.");
                FindClose(find_handle);
                return RESULT_FAILURE;
            }
            snprintf(full_path, full_path_length + 1, "%.*s%s", directory.length, directory.text, find_data.cFileName);
            out_full_path->text = full_path;
            out_full_path->length = (uint32_t)full_path_length;
            FindClose(find_handle);
            return RESULT_SUCCESS;
        }
    } while (FindNextFileA(find_handle, &find_data));

    return RESULT_FAILURE;
}

bool file_exists(string path) {
    DWORD file_attributes = GetFileAttributesA(path.text);
    return (file_attributes != INVALID_FILE_ATTRIBUTES);
}

result read_entire_file(string path, bump_allocator* allocator, string* out_file_contents) {
    ASSERT(allocator != NULL, return RESULT_FAILURE, "Allocator cannot be NULL");
    ASSERT(out_file_contents != NULL, return RESULT_FAILURE, "Output file contents cannot be NULL");

    HANDLE file_handle = CreateFileA(path.text, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE) {
        BUG("Failed to open file for reading: %.*s", path.length, path.text);
        return RESULT_FAILURE;
    }

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file_handle, &file_size)) {
        BUG("Failed to get file size for reading: %.*s", path.length, path.text);
        CloseHandle(file_handle);
        return RESULT_FAILURE;
    }

    if (file_size.QuadPart > UINT32_MAX) {
        BUG("File too large to read into memory: %.*s", path.length, path.text);
        CloseHandle(file_handle);
        return RESULT_FAILURE;
    }

    void* buffer = bump_allocate(allocator, 1, (size_t)file_size.QuadPart);
    if (buffer == NULL) {
        BUG("Failed to allocate memory for reading file: %.*s", path.length, path.text);
        CloseHandle(file_handle);
        return RESULT_FAILURE;
    }
    DWORD bytes_read;
    if (!ReadFile(file_handle, buffer, (DWORD)file_size.QuadPart, &bytes_read, NULL) || bytes_read != (DWORD)file_size.QuadPart) {
        BUG("Failed to read file: %.*s", path.length, path.text);
        CloseHandle(file_handle);
        return RESULT_FAILURE;
    }
    CloseHandle(file_handle);
    out_file_contents->text = (const char*)buffer;
    out_file_contents->length = (uint32_t)file_size.QuadPart;
    return RESULT_SUCCESS;
}

result write_entire_file(string path, const void* data, size_t size) {
    ASSERT(data != NULL, return RESULT_FAILURE, "Data pointer cannot be NULL");
    ASSERT(size > 0, return RESULT_FAILURE, "Size must be greater than zero");

    HANDLE file_handle = CreateFileA(path.text, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE) {
        BUG("Failed to open file for writing: %.*s", path.length, path.text);
        return RESULT_FAILURE;
    }
    DWORD bytes_written;
    if (!WriteFile(file_handle, data, (DWORD)size, &bytes_written, NULL) || bytes_written != (DWORD)size) {
        BUG("Failed to write file: %.*s", path.length, path.text);
        CloseHandle(file_handle);
        return RESULT_FAILURE;
    }
    CloseHandle(file_handle);
    return RESULT_SUCCESS;
}


typedef struct input {
    uint64_t keys_pressed_bitset[4];
    uint64_t keys_modified_this_frame_bitset[4];
    int32_t mouse_x;
    int32_t mouse_y;
    bool closed_window;
} input;

typedef enum {
    WINDOW_MODE_WINDOWED,
    WINDOW_MODE_FULLSCREEN,
    WINDOW_MODE_BORDERLESS_FULLSCREEN
} window_mode;

typedef struct {
    HWND handle;
    HDC hdc;
    input input_state;
    window_mode mode;
} window;

static LRESULT window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    window* w = (window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)(lParam);
        window* w = (window*)(pCreate->lpCreateParams);
        ASSERT(w != NULL, return -1, "Window internals cannot be NULL in WM_CREATE");
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)w);
        break;
    }
    case WM_CLOSE:
        ASSERT(w != NULL, return 0, "Window internals cannot be NULL in WM_CLOSE");
        w->input_state.closed_window = true;
        PostQuitMessage(0);
        break;
    case WM_DESTROY:
        ASSERT(w != NULL, return 0, "Window internals cannot be NULL in WM_DESTROY");
        w->input_state.closed_window = true;
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        ASSERT(w != NULL, return 0, "Window internals cannot be NULL in WM_KEYDOWN/WM_SYSKEYDOWN");
        int key = (int)wParam;
        ASSERT(key >= 0 && key < 256, return 0, "Unrecognized key code %d found in WM_KEYDOWN/WM_SYSKEYDOWN event.", key);
        size_t index = key >> 6;
        uint64_t mask = (uint64_t)1 << (key & 63);
        w->input_state.keys_pressed_bitset[index] |= mask;
        w->input_state.keys_modified_this_frame_bitset[index] |= mask;
    } break;
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        ASSERT(w != NULL, return 0, "Window internals cannot be NULL in WM_KEYUP/WM_SYSKEYUP");
        int key = (int)wParam;
        ASSERT(key >= 0 && key < 256, return 0, "Unrecognized key code %d found in WM_KEYUP/WM_SYSKEYUP event.", key);
        size_t index = key >> 6;
        uint64_t mask = (uint64_t)1 << (key & 63);
        w->input_state.keys_pressed_bitset[index] &= ~mask;
        w->input_state.keys_modified_this_frame_bitset[index] |= mask;
    } break;
    case WM_MOUSEMOVE: {
        ASSERT(w != NULL, return 0, "Window internals cannot be NULL in WM_MOUSEMOVE");
        w->input_state.mouse_x = GET_X_LPARAM(lParam);
        w->input_state.mouse_y = GET_Y_LPARAM(lParam);
    } break;
    default: {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    }

    return 0;
}

static result create_window(window* w, const char* title, uint32_t width, uint32_t height, window_mode mode) {
    ASSERT(w != NULL, return RESULT_FAILURE, "Window pointer cannot be NULL");
    const char* class_name = "window_class";
    HINSTANCE process_instance = GetModuleHandle(NULL);
    memset(w, 0, sizeof(*w));

    static bool class_registered = false;
    if (!class_registered) {
        WNDCLASS wc = { 0 };
        wc.lpfnWndProc = window_proc;
        wc.hInstance = process_instance;
        wc.lpszClassName = class_name;
        if (!RegisterClass(&wc)) {
            BUG("Failed to register window class.");
            return RESULT_FAILURE;
        }
        class_registered = true;
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);

    switch (mode) {
    case WINDOW_MODE_WINDOWED:
        style = WS_OVERLAPPEDWINDOW;
        break;
    case WINDOW_MODE_FULLSCREEN:
        style = WS_POPUP;
        width = screen_width;
        height = screen_height;
        break;
    case WINDOW_MODE_BORDERLESS_FULLSCREEN:
        style = WS_POPUP | WS_VISIBLE;
        width = screen_width;
        height = screen_height;
        break;
    }

    w->mode = mode;

    w->handle = CreateWindowEx(
        0,
        class_name,
        title,
        style,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL,
        NULL,
        process_instance,
        w
    );

    if (!w->handle) {
        BUG("Failed to create window.");
        return RESULT_FAILURE;
    }

    ShowWindow((HWND)w->handle, SW_SHOW);
    UpdateWindow((HWND)w->handle);
    return RESULT_SUCCESS;
}

typedef struct window_size {
    uint32_t width;
    uint32_t height;
} window_size;

static window_size get_window_size(window* window) {
    ASSERT(window != NULL, return (window_size){0}, "Window pointer cannot be NULL");
    RECT rect;
    if (GetClientRect(window->handle, &rect)) {
        return (window_size) {
            rect.right - rect.left, rect.bottom - rect.top
        };
    }
    BUG("Failed to get window size.");
    return (window_size) {
        0
    };
}

static input* update_window_input(window* window) {
    ASSERT(window != NULL, return NULL, "Window pointer cannot be NULL");
    input* input_state = &window->input_state;
    memset(input_state->keys_modified_this_frame_bitset, 0, sizeof(input_state->keys_modified_this_frame_bitset));

    // Poll for events
    MSG msg;
    while (PeekMessage(&msg, window->handle, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            input_state->closed_window = true;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return input_state;
}

static void destroy_window(window* window) {
    ASSERT(window != NULL, return, "Window pointer cannot be NULL");
    DestroyWindow(window->handle);
    memset(window, 0, sizeof(*window));
}

bool is_key_down(input* input_state, keyboard_key key) {
    ASSERT(input_state != NULL, return false, "Input state cannot be NULL");
    ASSERT(key >= 0 && key < 256, return false, "Key %d out of range", key);
    size_t index = key >> 6;
    uint64_t mask = (uint64_t)1 << (key & 63);
    return (input_state->keys_pressed_bitset[index] & mask) != 0 &&
        (input_state->keys_modified_this_frame_bitset[index] & mask) != 0;
}

bool is_key_held_down(input* input_state, keyboard_key key) {
    ASSERT(input_state != NULL, return false, "Input state cannot be NULL");
    ASSERT(key >= 0 && key < 256, return false, "Key %d out of range", key);
    size_t index = key >> 6;
    uint64_t mask = (uint64_t)1 << (key & 63);
    return (input_state->keys_pressed_bitset[index] & mask) != 0;
}

bool is_key_up(input* input_state, keyboard_key key) {
    ASSERT(input_state != NULL, return false, "Input state cannot be NULL");
    ASSERT(key >= 0 && key < 256, return false, "Key %d out of range", key);
    size_t index = key >> 6;
    uint64_t mask = (uint64_t)1 << (key & 63);
    return (input_state->keys_pressed_bitset[index] & mask) == 0 &&
        (input_state->keys_modified_this_frame_bitset[index] & mask) != 0;
}

#pragma region shaders
string vertex_shader_source =
CSTR(STRINGIFY(
    cbuffer view_projection : register(b0) {
    matrix view_proj;
};

struct VS_INPUT {
    // Vertex data
    float2 vertex_position : POSITION;
    float2 vertex_texcoord : TEXCOORD;

    // Instance data
    float2 sprite_position : SPRITE_POSITION;
    float2 sprite_texcoord : SPRITE_TEXCOORD;
    float2 sprite_scale : SPRITE_SCALE;
    float sprite_rotation : SPRITE_ROTATION;
};

struct PS_INPUT {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

PS_INPUT main(VS_INPUT input) {
    PS_INPUT output = (PS_INPUT)0;
    float cos_theta = cos(input.sprite_rotation);
    float sin_theta = sin(input.sprite_rotation);
    float2 rotated_position = float2(
        input.vertex_position.x * cos_theta - input.vertex_position.y * sin_theta,
        input.vertex_position.x * sin_theta + input.vertex_position.y * cos_theta
    );
    float2 scaled_position = rotated_position * input.sprite_scale;
    float2 world_position = scaled_position + input.sprite_position;

    // Use the view_proj matrix for proper transformation
    output.position = mul(float4(world_position, 0.0f, 1.0f), view_proj);

    output.texcoord = input.sprite_texcoord + (input.vertex_texcoord * input.sprite_scale);
    return output;
}
));

string pixel_shader_source =
CSTR(STRINGIFY(
    Texture2D spriteSheetTexture : register(t0);
SamplerState spriteSheetSampler : register(s0);

struct PS_INPUT {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET{
    //return float4(1.0, 0.0, 0.0, 1.0);
    return spriteSheetTexture.Sample(spriteSheetSampler, input.texcoord);
}
));

#pragma endregion

typedef struct {
    vector2 position;
    vector2 texcoord;
    vector2 scale;
    float rotation;
} sprite_instance;

typedef struct {
    sprite_instance* elements;
    size_t count;
} sprite_instances;

typedef struct graphics {
    vector2int sprite_sheet_size;
    window_size cached_window_size;
    window* window;
    HINSTANCE process_instance;
    D3D_DRIVER_TYPE driver_type;
    D3D_FEATURE_LEVEL feature_level;
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    IDXGISwapChain* swap_chain;
    ID3D11RenderTargetView* render_target_view;
    ID3D11VertexShader* vertex_shader;
    ID3D11PixelShader* pixel_shader;
    ID3D11InputLayout* input_layout;
    ID3D11SamplerState* sampler_state;
    ID3D11RasterizerState* rasterizer_state;
    ID3D11BlendState* blend_state;
    ID3D11Texture2D* sprite_sheet_texture;
    ID3D11ShaderResourceView* sprite_sheet_shader_resource;
    ID3D11Buffer* instance_buffer;
    D3D11_MAPPED_SUBRESOURCE instance_buffer_mapping;
    ID3D11Buffer* vertex_buffer;
    ID3D11Buffer* constant_buffer;
    matrix view_projection;
    sprite_instances sprite_instances;
} graphics;

void draw_sprite(graphics* graphics, vector2 position, vector2 scale, vector2int texcoord, vector2int texscale, float rotation) {
    ASSERT(graphics != NULL, return, "Graphics pointer cannot be NULL");
    if (graphics->sprite_instances.count >= MAX_SPRITES) {
        BUG("Exceeded maximum number of sprites per frame (%d). Increase MAX_SPRITES or reduce draw calls.", MAX_SPRITES);
        return;
    }

    sprite_instance* instance = &graphics->sprite_instances.elements[graphics->sprite_instances.count];
    ++graphics->sprite_instances.count;

    memset(instance, 0, sizeof(*instance));

    // Need to convert from pixel coordinates to normalized device coordinates (-1 to 1)
    float ndc_x = ((float)position.x / (float)graphics->cached_window_size.width) * 2.0f - 1.0f;
    float ndc_y = 1.0f - ((float)position.y / (float)graphics->cached_window_size.height) * 2.0f;
    instance->position = (vector2){ ndc_x, ndc_y };
    instance->scale = (vector2){ (float)scale.x / (float)graphics->cached_window_size.width * 2.0f, (float)scale.y / (float)graphics->cached_window_size.height * 2.0f };
    instance->texcoord = (vector2){ (float)texcoord.x / (float)graphics->sprite_sheet_size.x, (float)texcoord.y / (float)graphics->sprite_sheet_size.y };
    instance->rotation = rotation;
}

vector2int get_display_size(graphics* graphics) {
    ASSERT(graphics != NULL, return ((vector2int) {
        0, 0
    }), "Graphics pointer cannot be NULL");
    return (vector2int) {
        graphics->cached_window_size.width, graphics->cached_window_size.height
    };
}

static result compile_shader(const char* code, size_t code_length, LPCSTR shader_model, ID3DBlob** out_blob) {
    ASSERT(code != NULL, return RESULT_FAILURE, "Shader code cannot be NULL");
    ASSERT(code_length > 0, return RESULT_FAILURE, "Shader code length must be greater than zero");
    ASSERT(out_blob != NULL, return RESULT_FAILURE, "Output blob pointer cannot be NULL");

    DWORD compile_flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    compile_flags |= D3D11_CREATE_DEVICE_DEBUG | D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* shader_blob = NULL;
    ID3DBlob* error_blob = NULL;

    HRESULT hr = D3DCompile(
        code,
        code_length,
        NULL,
        NULL,
        NULL,
        "main",
        shader_model,
        compile_flags,
        0,
        &shader_blob,
        &error_blob
    );

    if (FAILED(hr)) {
        if (error_blob) {
            BUG("Shader compilation error: %.*s", (int)error_blob->lpVtbl->GetBufferSize(error_blob), (char*)error_blob->lpVtbl->GetBufferPointer(error_blob));
            error_blob->lpVtbl->Release(error_blob);
        }
        else {
            BUG("Failed to compile shader. HRESULT: 0x%08X", hr);
        }
        if (shader_blob) shader_blob->lpVtbl->Release(shader_blob);
        return RESULT_FAILURE;
    }

    *out_blob = shader_blob;
    return RESULT_SUCCESS;
}

static result create_graphics(window* window, bump_allocator* temp_allocator, graphics* graphics) {
    ASSERT(window != NULL, return RESULT_FAILURE, "Window pointer cannot be NULL");
    ASSERT(graphics != NULL, return RESULT_FAILURE, "Graphics pointer cannot be NULL");
    memset(graphics, 0, sizeof(*graphics));

    graphics->window = window;
    graphics->process_instance = GetModuleHandle(NULL);
    graphics->driver_type = D3D_DRIVER_TYPE_HARDWARE;
    graphics->feature_level = D3D_FEATURE_LEVEL_11_0;

    window_size size = get_window_size(window);
    if (size.width == 0 || size.height == 0) {
        BUG("Failed to get valid window size.");
        return RESULT_FAILURE;
    }

    graphics->cached_window_size = size;

    DXGI_SWAP_CHAIN_DESC swap_chain_desc = { 0 };
    swap_chain_desc.BufferCount = 1;
    swap_chain_desc.BufferDesc.Width = size.width;
    swap_chain_desc.BufferDesc.Height = size.height;
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
    swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.OutputWindow = window->handle;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.Windowed = (window->mode != WINDOW_MODE_FULLSCREEN);

    UINT device_flags = 0;
#ifdef _DEBUG
    device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        graphics->driver_type,
        NULL,
        device_flags,
        &graphics->feature_level,
        1,
        D3D11_SDK_VERSION,
        &swap_chain_desc,
        &graphics->swap_chain,
        &graphics->device,
        NULL,
        &graphics->context
    );

    if (FAILED(hr)) {
        BUG("Failed to create Direct3D device and swap chain. HRESULT: 0x%08X", hr);
        return RESULT_FAILURE;
    }

    // Create render target view:
    {
        ID3D11Texture2D* back_buffer = NULL;
        hr = graphics->swap_chain->lpVtbl->GetBuffer(graphics->swap_chain, 0, &IID_ID3D11Texture2D, (LPVOID*)&back_buffer);
        if (FAILED(hr)) {
            BUG("Failed to get back buffer from swap chain. HRESULT: 0x%08X", hr);
            return RESULT_FAILURE;
        }

        hr = graphics->device->lpVtbl->CreateRenderTargetView(graphics->device, (ID3D11Resource*)back_buffer, NULL, &graphics->render_target_view);
        back_buffer->lpVtbl->Release(back_buffer);
        if (FAILED(hr)) {
            BUG("Failed to create render target view. HRESULT: 0x%08X", hr);
            return RESULT_FAILURE;
        }
        // Set render target without depth stencil view
        graphics->context->lpVtbl->OMSetRenderTargets(graphics->context, 1, &graphics->render_target_view, NULL);
    }
    // Set viewport:
    {
        D3D11_VIEWPORT viewport = { 0 };
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.Width = (FLOAT)size.width;
        viewport.Height = (FLOAT)size.height;
        graphics->context->lpVtbl->RSSetViewports(graphics->context, 1, &viewport);
    }
    // Vertex shader:
    {
        ID3DBlob* vertex_shader_blob = NULL;

        if (compile_shader(vertex_shader_source.text, vertex_shader_source.length, "vs_5_0", &vertex_shader_blob) != RESULT_SUCCESS) {
            BUG("Failed to compile vertex shader.");
            return RESULT_FAILURE;
        }

        LPVOID buffer_pointer = vertex_shader_blob->lpVtbl->GetBufferPointer(vertex_shader_blob);
        SIZE_T buffer_size = vertex_shader_blob->lpVtbl->GetBufferSize(vertex_shader_blob);
        hr = graphics->device->lpVtbl->CreateVertexShader(graphics->device, buffer_pointer, buffer_size, NULL, &graphics->vertex_shader);

        if (FAILED(hr)) {
            vertex_shader_blob->lpVtbl->Release(vertex_shader_blob);
            BUG("Failed to create vertex shader. HRESULT: 0x%08X", hr);
            return RESULT_FAILURE;
        }

        // Input layout for vertex shader:
        {
            D3D11_INPUT_ELEMENT_DESC input_element_descs[] = {
                // Vertex data
                { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },

                // Instance data`
                { "SPRITE_POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
                { "SPRITE_TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 8, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
                { "SPRITE_SCALE", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
                { "SPRITE_ROTATION", 0, DXGI_FORMAT_R32_FLOAT, 1, 24, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            };

            // Check shader blob is valid before using
            if (!buffer_pointer || buffer_size == 0) {
                BUG("Invalid vertex shader blob for CreateInputLayout");
                return RESULT_FAILURE;
            }

            hr = graphics->device->lpVtbl->CreateInputLayout(
                graphics->device,
                input_element_descs,
                (UINT)ARRAY_LENGTH(input_element_descs),
                buffer_pointer,
                buffer_size,
                &graphics->input_layout
            );

            if (FAILED(hr)) {
                vertex_shader_blob->lpVtbl->Release(vertex_shader_blob);
                BUG("Failed to create input layout. HRESULT: 0x%08X", hr);
                return RESULT_FAILURE;
            }

            graphics->context->lpVtbl->IASetInputLayout(graphics->context, graphics->input_layout);
            graphics->context->lpVtbl->IASetPrimitiveTopology(graphics->context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }

        graphics->context->lpVtbl->VSSetShader(graphics->context, graphics->vertex_shader, NULL, 0);
        vertex_shader_blob->lpVtbl->Release(vertex_shader_blob);
    }
    // Pixel shader:
    {
        ID3DBlob* pixel_shader_blob = NULL;

        if (compile_shader(pixel_shader_source.text, pixel_shader_source.length, "ps_5_0", &pixel_shader_blob) != RESULT_SUCCESS) {
            BUG("Failed to compile pixel shader.");
            return RESULT_FAILURE;
        }

        LPVOID buffer_pointer = pixel_shader_blob->lpVtbl->GetBufferPointer(pixel_shader_blob);
        SIZE_T buffer_size = pixel_shader_blob->lpVtbl->GetBufferSize(pixel_shader_blob);
        hr = graphics->device->lpVtbl->CreatePixelShader(graphics->device, buffer_pointer, buffer_size, NULL, &graphics->pixel_shader);
        pixel_shader_blob->lpVtbl->Release(pixel_shader_blob);
        if (FAILED(hr)) {
            BUG("Failed to create pixel shader. HRESULT: 0x%08X", hr);
            return RESULT_FAILURE;
        }

        graphics->context->lpVtbl->PSSetShader(graphics->context, graphics->pixel_shader, NULL, 0);
    }

    // constant buffer for view-projection matrix in vertex shader:
    {
        D3D11_BUFFER_DESC buffer_desc = { 0 };
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.ByteWidth = sizeof(matrix);
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        buffer_desc.MiscFlags = 0;
        buffer_desc.StructureByteStride = 0;

        hr = graphics->device->lpVtbl->CreateBuffer(graphics->device, &buffer_desc, NULL, &graphics->constant_buffer);
        if (FAILED(hr)) {

            BUG("Failed to create constant buffer. HRESULT: 0x%08X", hr);
            return RESULT_FAILURE;
        }
    }

    // Vertex buffer for a unit quad (two triangles):
    {
        // A unit quad centered at the origin, with texture coordinates from (0,0) to (1,1)
        float vertex_data[] = {
            // Position      // TexCoord
            -0.5f, -0.5f,   0.0f, 1.0f,
             0.5f, -0.5f,   1.0f, 1.0f,
             0.5f,  0.5f,   1.0f, 0.0f,

            -0.5f, -0.5f,   0.0f, 1.0f,
             0.5f,  0.5f,   1.0f, 0.0f,
            -0.5f,  0.5f,   0.0f, 0.0f,
        };

        D3D11_BUFFER_DESC buffer_desc = { 0 };
        buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
        buffer_desc.ByteWidth = sizeof(vertex_data);
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        buffer_desc.CPUAccessFlags = 0;
        buffer_desc.MiscFlags = 0;
        buffer_desc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA init_data = { 0 };
        init_data.pSysMem = vertex_data;

        hr = graphics->device->lpVtbl->CreateBuffer(graphics->device, &buffer_desc, &init_data, &graphics->vertex_buffer);
        if (FAILED(hr)) {
            BUG("Failed to create vertex buffer. HRESULT: 0x%08X", hr);
            return RESULT_FAILURE;
        }

        UINT stride = sizeof(float) * 4; // Each vertex has 4 floats (2 for position, 2 for texcoord)
        UINT offset = 0;
        graphics->context->lpVtbl->IASetVertexBuffers(graphics->context, 0, 1, &graphics->vertex_buffer, &stride, &offset);
    }

    // sprite buffer for instance data will be set per draw call
    {
        D3D11_BUFFER_DESC buffer_desc = { 0 };
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.ByteWidth = (UINT)(sizeof(sprite_instance) * MAX_SPRITES); // Space for max_sprites sprites, can be adjusted as needed
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        buffer_desc.MiscFlags = 0;
        buffer_desc.StructureByteStride = 0;

        hr = graphics->device->lpVtbl->CreateBuffer(graphics->device, &buffer_desc, NULL, &graphics->instance_buffer);
        if (FAILED(hr)) {
            BUG("Failed to create instance vertex buffer. HRESULT: 0x%08X", hr);
            return RESULT_FAILURE;
        }

        UINT stride = sizeof(sprite_instance);
        UINT offset = 0;
        graphics->context->lpVtbl->IASetVertexBuffers(graphics->context, 1, 1, &graphics->instance_buffer, &stride, &offset);
    }

    // Sampler state:
    {
        D3D11_SAMPLER_DESC sampler_desc = { 0 };
        sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampler_desc.MinLOD = 0;
        sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

        hr = graphics->device->lpVtbl->CreateSamplerState(graphics->device, &sampler_desc, &graphics->sampler_state);
        if (FAILED(hr)) {
            BUG("Failed to create sampler state. HRESULT: 0x%08X", hr);
            return RESULT_FAILURE;
        }

        graphics->context->lpVtbl->PSSetSamplers(graphics->context, 0, 1, &graphics->sampler_state);
    }

    // Sprite Sheet Texture:
    {
        string current_directory = get_executable_directory(temp_allocator);
        string sprite_sheet_path = { 0 };
        if (find_first_file_with_extension(current_directory, (string)CSTR(".png"), temp_allocator, &sprite_sheet_path) != RESULT_SUCCESS) {
            BUG("Failed to find a .png file in the executable directory: %.*s", current_directory.length, current_directory.text);
            return RESULT_FAILURE;
        }

        int image_width, image_height, image_channels;
        unsigned char* image_data = stbi_load(sprite_sheet_path.text, &image_width, &image_height, &image_channels, STBI_rgb_alpha);
        if (!image_data) {
            BUG("Failed to load image: %.*s", sprite_sheet_path.length, sprite_sheet_path.text);
            return RESULT_FAILURE;
        }

        graphics->sprite_sheet_size = (vector2int){ image_width, image_height };

        D3D11_TEXTURE2D_DESC texture_desc = { 0 };
        texture_desc.Width = image_width;
        texture_desc.Height = image_height;
        texture_desc.MipLevels = 1;
        texture_desc.ArraySize = 1;
        texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.Usage = D3D11_USAGE_IMMUTABLE;
        texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texture_desc.CPUAccessFlags = 0;
        texture_desc.MiscFlags = 0;
        texture_desc.SampleDesc.Quality = 0;

        D3D11_SUBRESOURCE_DATA init_data = { 0 };
        init_data.pSysMem = image_data;
        init_data.SysMemPitch = image_width * 4; // 4 bytes per pixel (RGBA)
        init_data.SysMemSlicePitch = 0;

        hr = graphics->device->lpVtbl->CreateTexture2D(graphics->device, &texture_desc, &init_data, &graphics->sprite_sheet_texture);
        stbi_image_free(image_data);
        if (FAILED(hr)) {
            BUG("Failed to create texture for sprite sheet. HRESULT: 0x%08X", hr);
            return RESULT_FAILURE;
        }

        hr = graphics->device->lpVtbl->CreateShaderResourceView(graphics->device, (ID3D11Resource*)graphics->sprite_sheet_texture, NULL, &graphics->sprite_sheet_shader_resource);
        if (FAILED(hr)) {
            BUG("Failed to create shader resource view for sprite sheet texture. HRESULT: 0x%08X", hr);
            return RESULT_FAILURE;
        }

        graphics->context->lpVtbl->PSSetShaderResources(graphics->context, 0, 1, &graphics->sprite_sheet_shader_resource);
    }

    // Rasterizer state:
    {
        D3D11_RASTERIZER_DESC rasterizer_desc = { 0 };
        rasterizer_desc.FillMode = D3D11_FILL_SOLID;
        rasterizer_desc.CullMode = D3D11_CULL_NONE;
        rasterizer_desc.FrontCounterClockwise = FALSE;
        rasterizer_desc.DepthBias = 0;
        rasterizer_desc.DepthBiasClamp = 0.0f;
        rasterizer_desc.SlopeScaledDepthBias = 0.0f;
        rasterizer_desc.DepthClipEnable = TRUE;  // Changed to TRUE to ensure proper clipping
        rasterizer_desc.ScissorEnable = FALSE;   // Explicitly set scissor to off
        rasterizer_desc.MultisampleEnable = FALSE;
        rasterizer_desc.AntialiasedLineEnable = FALSE;

        hr = graphics->device->lpVtbl->CreateRasterizerState(graphics->device, &rasterizer_desc, &graphics->rasterizer_state);
        if (FAILED(hr)) {
            BUG("Failed to create rasterizer state. HRESULT: 0x%08X", hr);
            return RESULT_FAILURE;
        }

        // Set the rasterizer state to use
        graphics->context->lpVtbl->RSSetState(graphics->context, graphics->rasterizer_state);
    }

    // Blend state for transparent sprites:
    {
        D3D11_BLEND_DESC blend_desc = { 0 };
        blend_desc.RenderTarget[0].BlendEnable = TRUE;
        blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        hr = graphics->device->lpVtbl->CreateBlendState(graphics->device, &blend_desc, &graphics->blend_state);
        if (FAILED(hr)) {
            BUG("Failed to create blend state. HRESULT: 0x%08X", hr);
            return RESULT_FAILURE;
        }

        // Set the blend state
        float blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        graphics->context->lpVtbl->OMSetBlendState(graphics->context, graphics->blend_state, blend_factor, 0xFFFFFFFF);
    }    // Use orthographic matrix for 2D rendering instead of identity
    graphics->view_projection = identity_matrix();//orthographic_matrix(0.0f, (float)size.width, (float)size.height, 0.0f, -1.0f, 1.0f);
    return RESULT_SUCCESS;
}

static void draw_graphics(graphics* graphics, matrix view_projection) {
    ASSERT(graphics != NULL, return, "Graphics pointer cannot be NULL");

    // Update constant buffer with view-projection matrix
    D3D11_MAPPED_SUBRESOURCE mapped_resource;
    HRESULT hr = graphics->context->lpVtbl->Map(graphics->context, (ID3D11Resource*)graphics->constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
    if (FAILED(hr)) {
        BUG("Failed to map constant buffer. HRESULT: 0x%08X", hr);
        return;
    }
    memcpy(mapped_resource.pData, &view_projection, sizeof(matrix));
    graphics->context->lpVtbl->Unmap(graphics->context, (ID3D11Resource*)graphics->constant_buffer, 0);
    graphics->context->lpVtbl->VSSetConstantBuffers(graphics->context, 0, 1, &graphics->constant_buffer);

    // Clear render target
    float clear_color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
    graphics->context->lpVtbl->ClearRenderTargetView(graphics->context, graphics->render_target_view, clear_color);

    // Draw call
    UINT vertex_count = 6; // Two triangles per quad
    UINT instance_count = (UINT)graphics->sprite_instances.count;
    UINT start_vertex_location = 0;
    UINT start_instance_location = 0;

    if (instance_count > 0) {
        graphics->context->lpVtbl->RSSetState(graphics->context, graphics->rasterizer_state);
        graphics->context->lpVtbl->OMSetRenderTargets(graphics->context, 1, &graphics->render_target_view, NULL);

        float blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        graphics->context->lpVtbl->OMSetBlendState(graphics->context, graphics->blend_state, blend_factor, 0xFFFFFFFF);
        graphics->context->lpVtbl->DrawInstanced(graphics->context, vertex_count, instance_count, start_vertex_location, start_instance_location);
    }

    graphics->swap_chain->lpVtbl->Present(graphics->swap_chain, 1, 0);
}

static void destroy_graphics(graphics* graphics) {
    ASSERT(graphics != NULL, return, "Graphics pointer cannot be NULL");

    if (graphics->context) {
        graphics->context->lpVtbl->ClearState(graphics->context);
    }
    if (graphics->sprite_sheet_shader_resource) {
        graphics->sprite_sheet_shader_resource->lpVtbl->Release(graphics->sprite_sheet_shader_resource);
        graphics->sprite_sheet_shader_resource = NULL;
    }
    if (graphics->sprite_sheet_texture) {
        graphics->sprite_sheet_texture->lpVtbl->Release(graphics->sprite_sheet_texture);
        graphics->sprite_sheet_texture = NULL;
    }
    if (graphics->blend_state) {
        graphics->blend_state->lpVtbl->Release(graphics->blend_state);
        graphics->blend_state = NULL;
    }
    if (graphics->rasterizer_state) {
        graphics->rasterizer_state->lpVtbl->Release(graphics->rasterizer_state);
        graphics->rasterizer_state = NULL;
    }
    if (graphics->sampler_state) {
        graphics->sampler_state->lpVtbl->Release(graphics->sampler_state);
        graphics->sampler_state = NULL;
    }
    if (graphics->instance_buffer) {
        graphics->instance_buffer->lpVtbl->Release(graphics->instance_buffer);
        graphics->instance_buffer = NULL;
    }
    if (graphics->vertex_buffer) {
        graphics->vertex_buffer->lpVtbl->Release(graphics->vertex_buffer);
        graphics->vertex_buffer = NULL;
    }
    if (graphics->constant_buffer) {
        graphics->constant_buffer->lpVtbl->Release(graphics->constant_buffer);
        graphics->constant_buffer = NULL;
    }
    if (graphics->input_layout) {
        graphics->input_layout->lpVtbl->Release(graphics->input_layout);
        graphics->input_layout = NULL;
    }
    if (graphics->pixel_shader) {
        graphics->pixel_shader->lpVtbl->Release(graphics->pixel_shader);
        graphics->pixel_shader = NULL;
    }
    if (graphics->vertex_shader) {
        graphics->vertex_shader->lpVtbl->Release(graphics->vertex_shader);
        graphics->vertex_shader = NULL;
    }
    if (graphics->render_target_view) {
        graphics->render_target_view->lpVtbl->Release(graphics->render_target_view);
        graphics->render_target_view = NULL;
    }
    if (graphics->swap_chain) {
        graphics->swap_chain->lpVtbl->Release(graphics->swap_chain);
        graphics->swap_chain = NULL;
    }
    if (graphics->context) {
        graphics->context->lpVtbl->Release(graphics->context);
        graphics->context = NULL;
    }
    if (graphics->device) {
        graphics->device->lpVtbl->Release(graphics->device);
        graphics->device = NULL;
    }

    memset(graphics, 0, sizeof(*graphics));
}


#ifdef HOT_RELOAD_HOST
static HMODULE app_dll = NULL;

typedef enum {
    HOT_RELOAD_IF_DLL_UPDATED,
    HOT_RELOAD_FORCED,
} hot_reload_condition;

static bool potential_hot_reload(hot_reload_condition hot_reload_condition) {
    static uint64_t last_edit_time = 0;
    uint64_t edit_time = last_edit_time;

    if (hot_reload_condition == HOT_RELOAD_IF_DLL_UPDATED) {
        // Check the last edit time of the DLL file
        // If the DLL file is updated, the time will be different, and we need to reload the DLL
        WIN32_FILE_ATTRIBUTE_DATA file_info;
        if (!GetFileAttributesExA(HOT_RELOAD_DLL_PATH, GetFileExInfoStandard, &file_info)) {
            BUG("Failed to get file attributes for %s", HOT_RELOAD_DLL_PATH);
            return false;
        }

        edit_time = (((uint64_t)file_info.ftLastWriteTime.dwHighDateTime) << 32) | file_info.ftLastWriteTime.dwLowDateTime;
        if (edit_time <= last_edit_time) {
            return false; // No change
        }
    }
    else {
        DEBUG_ASSERT(hot_reload_condition == HOT_RELOAD_FORCED, return false, "Invalid hot reload condition");
    }

    if (app_dll) {
        FreeLibrary(app_dll);
        app_dll = NULL;
        init = NULL;
        update = NULL;
        shutdown = NULL;
    }

    // Copy the DLL to a temporary file to avoid locking issues
    if (!CopyFileA(HOT_RELOAD_DLL_PATH, HOT_RELOAD_DLL_TEMP_PATH, FALSE)) {
        BUG("Failed to copy %s to %s", HOT_RELOAD_DLL_PATH, HOT_RELOAD_DLL_TEMP_PATH);
        return false;
    }

    app_dll = LoadLibraryA(HOT_RELOAD_DLL_TEMP_PATH);
    if (!app_dll) {
        BUG("Failed to load %s", HOT_RELOAD_DLL_TEMP_PATH);
        return false;
    }

    init = (init_function)GetProcAddress(app_dll, "init");
    update = (update_function)GetProcAddress(app_dll, "update");
    shutdown = (shutdown_function)GetProcAddress(app_dll, "shutdown");
    if (!init || !update || !shutdown) {
        BUG("Failed to get function addresses from %s", HOT_RELOAD_DLL_TEMP_PATH);
        return false;
    }

    last_edit_time = edit_time;
    return true;
}

#endif // HOT_RELOAD_HOST

static struct {
    memory_allocators memory_allocators;
    window window;
    graphics graphics;
    clock clock;
    void* app_state;
} app; // <- this static variable is only used globally in WinMain, create_app() and destroy_app() (but it's members may be passed to function calls)

static result create_app(void) {
    if (create_clock(&app.clock) != RESULT_SUCCESS) {
        BUG("Failed to create application clock (for measuring delta time).");
        return RESULT_FAILURE;
    }

    if (create_bump_allocator(&app.memory_allocators.permanent_allocator, 1024 * 1024 * 1024) != RESULT_SUCCESS) {
        BUG("Failed to create permanent memory allocator.");
        return RESULT_FAILURE;
    }

    if (create_bump_allocator(&app.memory_allocators.temp_allocator, 64 * 1024 * 1024) != RESULT_SUCCESS) {
        BUG("Failed to create temporary memory allocator.");
        return RESULT_FAILURE;
    }

    if (create_window(&app.window, "Game Overlord", 1280, 720, WINDOW_MODE_WINDOWED) != RESULT_SUCCESS) {
        BUG("Failed to create application window.");
        return RESULT_FAILURE;
    }

    init_in_params in_params = { 0 };
    in_params.memory_allocators = &app.memory_allocators;
    init_out_params out_params = { 0 };

    if (init(&in_params, &out_params) != RESULT_SUCCESS) {
        BUG("Failed to initialize app.");
        return RESULT_FAILURE;
    }

    if (create_graphics(&app.window, &app.memory_allocators.temp_allocator, &app.graphics) != RESULT_SUCCESS) {
        BUG("Failed to create graphics context.");
        return RESULT_FAILURE;
    }

    app.app_state = out_params.app_state;
    return RESULT_SUCCESS;
}

static void destroy_app(void) {
    shutdown_params shutdown_params = { 0 };
    shutdown_params.app_state = app.app_state;
    shutdown_params.memory_allocators = &app.memory_allocators;
    shutdown(&shutdown_params);

    destroy_graphics(&app.graphics);
    destroy_window(&app.window);
    destroy_bump_allocator(&app.memory_allocators.permanent_allocator);
    destroy_bump_allocator(&app.memory_allocators.temp_allocator);

#ifdef HOT_RELOAD_HOST
    FreeLibrary(app_dll);
#endif
}


#ifdef APP_BACKEND
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
#ifdef HOT_RELOAD_HOST
    if (!potential_hot_reload(HOT_RELOAD_FORCED)) {
        BUG("Failed to load initial app DLL.");
        FreeLibrary(app_dll);
        return -1;
    }
#endif

    if (create_app() != RESULT_SUCCESS) {
        BUG("Failed to create application.");
        goto cleanup;
    }

    // update the clock just before the first frame so delta time is not too big.
    update_clock(&app.clock);

    /*-----------------------------------------------------------------*/
    // Main loop
    while (1) {
        update_clock(&app.clock);
        reset_bump_allocator(&app.memory_allocators.temp_allocator);

#ifdef HOT_RELOAD_HOST
        potential_hot_reload(HOT_RELOAD_IF_DLL_UPDATED);
#endif
        input* input_state = update_window_input(&app.window);
        if (input_state->closed_window) {
            break;
        }

        { // Map the instance buffer to update instance data
            HRESULT hr = app.graphics.context->lpVtbl->Map(app.graphics.context, (ID3D11Resource*)app.graphics.instance_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &app.graphics.instance_buffer_mapping);
            if (FAILED(hr)) {
                BUG("Failed to map instance buffer. HRESULT: 0x%08X", hr);
                return -1;
            }

            app.graphics.sprite_instances.elements = (sprite_instance*)app.graphics.instance_buffer_mapping.pData;
            app.graphics.sprite_instances.count = 0;
        }

        { // Update app
            update_params update_params = { 0 };
            update_params.clock = app.clock;
            update_params.graphics = &app.graphics;
            update_params.memory_allocators = &app.memory_allocators;
            update_params.input = input_state;
            update_params.app_state = app.app_state;

            if (update(&update_params) != RESULT_SUCCESS) {
                BUG("Failed to update app.");
                break;
            }
        }

        { // Unmap the instance buffer after updating sprite data (presumably from update app)
            app.graphics.context->lpVtbl->Unmap(app.graphics.context, (ID3D11Resource*)app.graphics.instance_buffer, 0);
        }

        draw_graphics(&app.graphics, app.graphics.view_projection);

    }

cleanup:
    destroy_app();
    return 0;
}

#endif // APP_BACKEND