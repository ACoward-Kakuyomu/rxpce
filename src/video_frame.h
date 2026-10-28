#pragma once
#include <array>
#include <cstdint>
#include <istream>
#include <vector>

namespace rxpce_video {
using Pixel = std::uint16_t;
using Palette = std::array<std::uint32_t, 512>;

inline Palette linear_palette() {
    Palette result;
    for (unsigned i = 0; i < result.size(); ++i) {
        const auto red = (((i >> 3) & 7) * 255 + 3) / 7;
        const auto green = ((i >> 6) * 255 + 3) / 7;
        const auto blue = ((i & 7) * 255 + 3) / 7;
        result[i] = 0xff000000u | red | (green << 8) | (blue << 16);
    }
    return result;
}

inline bool load_palette(std::istream& input, Palette& result) {
    result = linear_palette();
    unsigned char bytes[1536];
    if (!input.read(reinterpret_cast<char*>(bytes), sizeof(bytes)))
        return false;
    char extra;
    if (input.get(extra) || !input.eof())
        return false;
    for (unsigned i = 0; i < result.size(); ++i)
        result[i] = 0xff000000u | bytes[i * 3] | (bytes[i * 3 + 1] << 8) | (bytes[i * 3 + 2] << 16);
    return true;
}

inline void set_palette_color(Pixel* palette, unsigned index, unsigned raw) {
    const auto color = static_cast<Pixel>(raw & 511);
    if (index == 0) {
        for (unsigned i = 0; i < 256; i += 16)
            palette[i] = color;
    } else if (index < 512 && (index & 15))
        palette[index] = color;
}

inline bool convert_frame(const Pixel* source, int stride, int rows, int x, int y, int width,
                          int height, const Palette& palette, std::vector<std::uint32_t>& result) {
    if (!source || x < 0 || y < 0 || width <= 0 || height <= 0 || width > stride || height > rows ||
        x > stride - width || y > rows - height)
        return false;
    result.resize(static_cast<std::size_t>(width) * height);
    for (int row = 0; row < height; ++row)
        for (int col = 0; col < width; ++col)
            result[row * width + col] = palette[source[(y + row) * stride + x + col] & 511];
    return true;
}

struct Placement {
    int x, y, width, height;
};
inline int fit_scale(int width, int height, int client_width, int client_height) {
    if (width <= 0 || height <= 0)
        return 1;
    const int horizontal = client_width / width, vertical = client_height / height;
    const int scale = horizontal < vertical ? horizontal : vertical;
    return scale < 1 ? 1 : scale;
}
inline Placement placement(int width, int height, int client_width, int client_height, int scale) {
    return {(client_width - width * scale) / 2, (client_height - height * scale) / 2, width * scale,
            height * scale};
}
} // namespace rxpce_video
