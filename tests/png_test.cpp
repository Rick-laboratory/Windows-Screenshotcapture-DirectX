#include "windows_helpers.hpp"
#include "png_writer.hpp"

#include <wincodec.h>
#include <array>
#include <iostream>
#include <limits>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
} // namespace

int main() {
    const std::filesystem::path file(u"synthetic-\u00E4-\u6587.png");
    try {
        const std::array<std::uint8_t, 24> source{
            0, 0, 255, 0, 0, 255, 0, 17, 99, 99, 99, 99,
            255, 0, 0, 42, 255, 255, 255, 128, 99, 99, 99, 99
        };
        save_png(2, 2, 12, source.data(), source.size(), file);
        {
            const ComApartment apartment;
            ComPtr<IWICImagingFactory> factory;
            check_hresult(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IWICImagingFactory, reinterpret_cast<void**>(factory.put())), "Factory");
            ComPtr<IWICBitmapDecoder> decoder;
            check_hresult(factory->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ,
                          WICDecodeMetadataCacheOnLoad, decoder.put()), "Decode PNG");
            ComPtr<IWICBitmapFrameDecode> frame;
            check_hresult(decoder->GetFrame(0, frame.put()), "Read frame");
            UINT width = 0, height = 0;
            check_hresult(frame->GetSize(&width, &height), "Read size");
            require(width == 2 && height == 2, "PNG dimensions changed.");
            ComPtr<IWICFormatConverter> converter;
            check_hresult(factory->CreateFormatConverter(converter.put()), "Create converter");
            check_hresult(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppBGRA,
                          WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom), "Convert pixels");
            std::array<std::uint8_t, 16> pixels{};
            check_hresult(converter->CopyPixels(nullptr, 8, static_cast<UINT>(pixels.size()),
                                                 pixels.data()), "Read pixels");
            const std::array<std::uint8_t, 16> expected{
                0, 0, 255, 255, 0, 255, 0, 255,
                255, 0, 0, 255, 255, 255, 255, 255
            };
            require(pixels == expected, "Stride, color channels or opaque alpha are incorrect.");
        }

        for (const std::size_t stride : {std::size_t{1}, std::size_t{100}}) {
            bool rejected = false;
            try { save_png(2, 2, stride, source.data(), source.size(), file); }
            catch (const std::invalid_argument&) { rejected = true; }
            require(rejected, "Invalid stride/buffer accepted.");
        }
        bool overflow_rejected = false;
        try {
            save_png(std::numeric_limits<std::uint32_t>::max(),
                     std::numeric_limits<std::uint32_t>::max(), 12,
                     source.data(), source.size(), file);
        } catch (const std::invalid_argument&) { overflow_rejected = true; }
        require(overflow_rejected, "Overflowing dimensions accepted.");
        bool file_error = false;
        try { save_png(2, 2, 12, source.data(), source.size(), file / "child.png"); }
        catch (const std::runtime_error&) { file_error = true; }
        require(file_error, "File initialization error was ignored.");
        std::filesystem::remove(file);
        std::cout << "PNG roundtrip, stride, colors, alpha, Unicode path and error checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
