#include "windows_helpers.hpp"
#include "png_writer.hpp"

#include <d3d9.h>
#include <chrono>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace {
struct HiddenWindow {
    HWND handle = CreateWindowExW(0, L"STATIC", L"Direct3D capture", WS_POPUP,
                                   0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    HiddenWindow() {
        if (handle == nullptr) throw std::runtime_error("Cannot create the device's hidden window.");
    }
    ~HiddenWindow() { DestroyWindow(handle); }
    HiddenWindow(const HiddenWindow&) = delete;
    HiddenWindow& operator=(const HiddenWindow&) = delete;
};

class SurfaceLock {
public:
    explicit SurfaceLock(IDirect3DSurface9* surface) : surface_(surface) {
        check_hresult(surface_->LockRect(&rectangle, nullptr, D3DLOCK_READONLY), "Lock surface");
    }
    ~SurfaceLock() { surface_->UnlockRect(); }
    SurfaceLock(const SurfaceLock&) = delete;
    SurfaceLock& operator=(const SurfaceLock&) = delete;
    D3DLOCKED_RECT rectangle{};

private:
    IDirect3DSurface9* surface_;
};

unsigned parse_count(std::wstring_view text) {
    if (text.empty()) throw std::invalid_argument("Count must be an integer from 1 to 100.");
    unsigned value = 0;
    for (const wchar_t character : text) {
        if (character < L'0' || character > L'9') {
            throw std::invalid_argument("Count must be an integer from 1 to 100.");
        }
        value = value * 10 + static_cast<unsigned>(character - L'0');
        if (value > 100) throw std::invalid_argument("Count must be an integer from 1 to 100.");
    }
    if (value == 0) throw std::invalid_argument("Count must be an integer from 1 to 100.");
    return value;
}
} // namespace

int wmain(int argc, wchar_t* argv[]) {
    const std::wstring_view option = argc >= 2 ? argv[1] : L"";
    if (argc == 2 && (option == L"--help" || option == L"-h")) {
        std::cout << "Usage: directx-capture [count] [existing-output-directory]\n"
                     "Default: one PNG in the current directory. Count: 1..100.\n"
                     "Files cap0.png, cap1.png, ... are overwritten if present.\n"
                     "--check initializes the device and surface without capturing pixels.\n";
        return 0;
    }
    if (argc > 3 || (option == L"--check" && argc != 2)) {
        std::cerr << "Usage: directx-capture [count] [existing-output-directory]\n";
        return 2;
    }
    try {
        const bool check_only = option == L"--check";
        const unsigned count = argc >= 2 && !check_only ? parse_count(option) : 1;
        const std::filesystem::path directory = argc == 3 ? argv[2] : L".";
        if (!std::filesystem::is_directory(directory)) {
            throw std::invalid_argument("The output directory must already exist.");
        }
        if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) &&
            GetLastError() != ERROR_ACCESS_DENIED) {
            throw std::runtime_error("Cannot enable per-monitor DPI awareness.");
        }

        ComPtr<IDirect3D9> d3d(Direct3DCreate9(D3D_SDK_VERSION));
        if (d3d.get() == nullptr) throw std::runtime_error("Direct3DCreate9 failed.");
        D3DDISPLAYMODE mode{};
        check_hresult(d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode), "Get display mode");
        const HiddenWindow window;
        D3DPRESENT_PARAMETERS parameters{};
        parameters.Windowed = TRUE;
        parameters.BackBufferCount = 1;
        parameters.BackBufferWidth = 1;
        parameters.BackBufferHeight = 1;
        parameters.BackBufferFormat = D3DFMT_UNKNOWN;
        parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
        parameters.hDeviceWindow = window.handle;

        ComPtr<IDirect3DDevice9> device;
        check_hresult(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window.handle,
                                        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &parameters,
                                        device.put()), "Create Direct3D device");
        ComPtr<IDirect3DSurface9> surface;
        check_hresult(device->CreateOffscreenPlainSurface(mode.Width, mode.Height,
                       D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, surface.put(), nullptr), "Create capture surface");
        if (check_only) {
            std::cout << "Device and surface ready: " << mode.Width << " x " << mode.Height << ".\n";
            return 0;
        }

        const auto start = std::chrono::steady_clock::now();
        for (unsigned index = 0; index < count; ++index) {
            check_hresult(device->GetFrontBufferData(0, surface.get()), "Read front buffer");
            const SurfaceLock locked(surface.get());
            if (locked.rectangle.Pitch <= 0 ||
                mode.Height > std::numeric_limits<std::size_t>::max() /
                              static_cast<unsigned>(locked.rectangle.Pitch)) {
                throw std::runtime_error("Invalid capture surface pitch.");
            }
            const std::size_t stride = static_cast<unsigned>(locked.rectangle.Pitch);
            save_png(mode.Width, mode.Height, stride,
                     static_cast<const std::uint8_t*>(locked.rectangle.pBits), stride * mode.Height,
                     directory / ("cap" + std::to_string(index) + ".png"));
        }
        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        std::cout << "Saved " << count << " PNG file(s) in " << elapsed
                  << " seconds, including encoding and file I/O.\n";
        return 0;
    } catch (const std::invalid_argument& error) {
        std::cerr << error.what() << '\n';
        return 2;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
