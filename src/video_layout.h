#pragma once

namespace rxpce_video {
enum class AspectMode { square_pixels, tv_4_3 };
struct LayoutSize {
    int width = 0, height = 0;
};
struct DisplayPlacement {
    int x = 0, y = 0, width = 0, height = 0;
};
using Placement = DisplayPlacement;
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

constexpr bool valid_aspect_mode(AspectMode mode) {
    return mode == AspectMode::square_pixels || mode == AspectMode::tv_4_3;
}
constexpr bool valid_source_size(int width, int height) {
    return width > 0 && width <= 512 && height > 0 && height <= 256;
}
inline LayoutSize window_size(int width, int height, int scale, AspectMode mode) {
    if (!valid_source_size(width, height) || scale < 1 || scale > 4 || !valid_aspect_mode(mode))
        return {};
    if (mode == AspectMode::tv_4_3)
        return {320 * scale, 240 * scale};
    return {(width < 320 ? 320 : width) * scale, (height < 240 ? 240 : height) * scale};
}
inline DisplayPlacement display_placement(int width, int height, int client_width,
                                          int client_height, int scale, AspectMode mode) {
    if (!valid_source_size(width, height) || client_width <= 0 || client_height <= 0 || scale < 0 ||
        scale > 4 || !valid_aspect_mode(mode))
        return {};
    if (mode == AspectMode::tv_4_3) {
        const auto horizontal = client_width / 4, vertical = client_height / 3;
        const auto k = horizontal < vertical ? horizontal : vertical;
        if (k == 0)
            return {};
        return {(client_width - 4 * k) / 2, (client_height - 3 * k) / 2, 4 * k, 3 * k};
    }
    if (scale == 0)
        scale = fit_scale(width, height, client_width, client_height);
    return placement(width, height, client_width, client_height, scale);
}

} // namespace rxpce_video
