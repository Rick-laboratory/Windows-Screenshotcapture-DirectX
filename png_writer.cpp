#include "windows_helpers.hpp"
#include "png_writer.hpp"

#include <wincodec.h>
#include <cstring>
#include <limits>
#include <vector>

void save_png(std::uint32_t width, std::uint32_t height, std::size_t stride,
              const std::uint8_t* pixels, std::size_t buffer_size,
              const std::filesystem::path& file) {
    const std::uint64_t row_bytes = static_cast<std::uint64_t>(width) * 4;
    if (pixels == nullptr || width == 0 || height == 0 ||
        row_bytes > std::numeric_limits<UINT>::max() / height || stride < row_bytes ||
        stride > std::numeric_limits<std::size_t>::max() / height ||
        buffer_size < stride * (height - 1) + row_bytes) {
        throw std::invalid_argument("Invalid image dimensions, stride or buffer length.");
    }

    const auto total_bytes = static_cast<std::size_t>(row_bytes * height);
    std::vector<std::uint8_t> opaque(total_bytes);
    for (std::uint32_t row = 0; row < height; ++row) {
        auto* destination = opaque.data() + static_cast<std::size_t>(row * row_bytes);
        std::memcpy(destination, pixels + row * stride, static_cast<std::size_t>(row_bytes));
        for (std::uint32_t column = 0; column < width; ++column) destination[column * 4 + 3] = 255;
    }

    const ComApartment apartment;
    ComPtr<IWICImagingFactory> factory;
    check_hresult(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_IWICImagingFactory,
                                    reinterpret_cast<void**>(factory.put())), "Create WIC factory");
    ComPtr<IWICStream> stream;
    check_hresult(factory->CreateStream(stream.put()), "Create WIC stream");
    check_hresult(stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE), "Open PNG file");
    ComPtr<IWICBitmapEncoder> encoder;
    check_hresult(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put()),
                  "Create PNG encoder");
    check_hresult(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache), "Initialize encoder");
    ComPtr<IWICBitmapFrameEncode> frame;
    check_hresult(encoder->CreateNewFrame(frame.put(), nullptr), "Create PNG frame");
    check_hresult(frame->Initialize(nullptr), "Initialize frame");
    check_hresult(frame->SetSize(width, height), "Set image dimensions");
    GUID format = GUID_WICPixelFormat32bppBGRA;
    check_hresult(frame->SetPixelFormat(&format), "Set pixel format");
    if (!IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA)) {
        throw std::runtime_error("The PNG encoder did not accept 32-bit BGRA pixels.");
    }
    check_hresult(frame->WritePixels(height, static_cast<UINT>(row_bytes),
                                      static_cast<UINT>(opaque.size()), opaque.data()), "Write pixels");
    check_hresult(frame->Commit(), "Commit frame");
    check_hresult(encoder->Commit(), "Commit PNG");
}
