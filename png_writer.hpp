#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>

// Input is top-down BGRA with the supplied row stride. Desktop alpha is ignored;
// output PNG pixels are opaque. The input remains owned by the caller.
void save_png(std::uint32_t width, std::uint32_t height, std::size_t stride,
              const std::uint8_t* pixels, std::size_t buffer_size,
              const std::filesystem::path& file);
