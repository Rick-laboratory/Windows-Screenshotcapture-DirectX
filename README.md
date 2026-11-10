# Direct3D 9 Screen Capture

A Windows C++ example that captures the default adapter's front buffer with Direct3D 9 and saves PNG files through Windows Imaging Component (WIC).

The code separates device setup, surface locking, COM ownership, and PNG encoding. Every fallible Direct3D/WIC call is checked, and capture buffers are processed one frame at a time.

## Build

Requirements: Windows 10 version 1703+, CMake 3.16+, and a C++17 compiler with Windows SDK headers or MinGW-w64. The project links `d3d9`, `windowscodecs`, `ole32`, `uuid`, and `user32`; ATL and D3DX are not required.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

For MinGW-w64 with Ninja, add `-G Ninja`. Visual Studio executables are in `build/Release/`; Ninja executables are in `build/`.

## Run

```powershell
.\build\directx-capture.exe                # One screenshot: cap0.png
.\build\directx-capture.exe 10 .           # Ten screenshots in the current directory
.\build\directx-capture.exe --check        # Initialize the device without capturing pixels
.\build\directx-capture.exe --help
```

The output directory must already exist. Counts must be `1..100`. Files are named `cap0.png`, `cap1.png`, and so on; existing files with those names are overwritten. Unicode directory names are supported.

Exit codes: `0` on success, `1` for device/encoding/file errors, `2` for invalid arguments. Reported elapsed time includes capture, PNG encoding, and file I/O.

## Code guide

| File | Purpose |
| --- | --- |
| `main.cpp` | Hidden device window, Direct3D setup, scoped surface locking, and frame loop |
| `windows_helpers.hpp` | Small COM ownership and HRESULT helpers; balanced COM initialization |
| `png_writer.hpp` / `png_writer.cpp` | Checked image layout, opaque BGRA normalization, and WIC encoding |
| `tests/png_test.cpp` | PNG encode/decode roundtrip using synthetic pixels |

Direct3D returns a 32-bit surface with a row pitch that may include padding. The writer copies rows according to that pitch and sets alpha to `255`: desktop captures are treated as opaque images, avoiding accidentally transparent output. It verifies that the encoder accepts the requested pixel format.

One normalization buffer is allocated per frame. Memory usage does not grow with the screenshot count, and no manually paired `new[]`/`delete` buffers are needed. A successful `CoInitializeEx`, including `S_FALSE`, is balanced by `CoUninitialize`.

## Validation and platform limits

The synthetic test verifies PNG dimensions, row padding, RGB channels, opaque alpha, Unicode filenames, and write failures without capturing the desktop. `--check` is a separate device-initialization probe.

[`GetFrontBufferData`](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-getfrontbufferdata) is a legacy, slow capture path. This example is intended for learning and occasional screenshots, not high-frame-rate recording. Display drivers, remote sessions, protected content, device loss, and multi-monitor configurations can affect capture behavior. Only the default Direct3D adapter is selected.

## License

This repository does not currently include a license file.
