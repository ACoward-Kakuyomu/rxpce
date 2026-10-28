#include "win32_support.h"
#include "Info.h"
#include "resource.h"
#include <mutex>
#include <vector>

namespace {
std::mutex mutex;
char message[256] = {};
unsigned message_frames = 120;
int fps = 0;
bool show_fps = false;
std::vector<unsigned char> glyphs;
int bitmap_width = 0, glyph_width = 0, glyph_height = 0;

void draw_text(std::uint32_t* rgba, int width, int height, int x, int y, const char* text) {
    if (glyphs.empty())
        return;
    const int origin = x;
    for (; *text; ++text) {
        const unsigned c = static_cast<unsigned char>(*text);
        if (c == '\n') {
            x = origin;
            y -= glyph_height + 4;
            continue;
        }
        if (c < 32 || c >= 128)
            continue;
        const int sx = ((c - 32) % 32) * glyph_width, sy = ((c - 32) / 32) * glyph_height;
        for (int gy = 0; gy < glyph_height; ++gy)
            for (int gx = 0; gx < glyph_width; ++gx) {
                if (x + gx < 0 || y + gy < 0 || x + gx >= width || y + gy >= height)
                    continue;
                const auto color = glyphs[(sy + gy) * bitmap_width + sx + gx];
                if (color <= 1)
                    rgba[(y + gy) * width + x + gx] = color ? 0xffffffffu : 0xff000000u;
            }
        x += glyph_width;
    }
}
} // namespace

void InitInfo() {
    std::lock_guard<std::mutex> lock(mutex);
    glyphs.clear();
    message_frames = 120;
    show_fps = false;
    const auto module = GetModuleHandle(nullptr);
    const auto resource = FindResource(module, MAKEINTRESOURCE(IDR_CHARS_BMP), "BITMAPFILE");
    if (!resource)
        return;
    const auto loaded = LoadResource(module, resource);
    if (!loaded)
        return;
    const auto data = static_cast<const unsigned char*>(LockResource(loaded));
    const auto size = SizeofResource(module, resource);
    if (!data || size < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER))
        return;
    BITMAPFILEHEADER file;
    BITMAPINFOHEADER info;
    memcpy(&file, data, sizeof(file));
    memcpy(&info, data + sizeof(file), sizeof(info));
    if (file.bfType != 0x4d42 || info.biBitCount != 8 || info.biCompression != BI_RGB ||
        info.biWidth <= 0 || info.biWidth > 4096 || info.biHeight <= 0 || info.biHeight > 4096 ||
        info.biWidth % 32 || info.biHeight % 3)
        return;
    const unsigned pitch = (info.biWidth + 3) & ~3;
    if (file.bfOffBits > size || pitch * info.biHeight > size - file.bfOffBits)
        return;
    bitmap_width = info.biWidth;
    glyph_width = info.biWidth / 32;
    glyph_height = info.biHeight / 3;
    glyphs.resize(info.biWidth * info.biHeight);
    for (int row = 0; row < info.biHeight; ++row)
        memcpy(glyphs.data() + row * info.biWidth,
               data + file.bfOffBits + (info.biHeight - row - 1) * pitch, info.biWidth);
}
void TrashInfo() {
    std::lock_guard<std::mutex> lock(mutex);
    glyphs.clear();
}
void SetInfoString(LPCTSTR text) {
    std::lock_guard<std::mutex> lock(mutex);
    strncpy_s(message, text, _TRUNCATE);
    message_frames = 0;
}
void SetInfoFps(int value) {
    std::lock_guard<std::mutex> lock(mutex);
    fps = value;
    show_fps = true;
}
void DisplayInfo(std::uint32_t* rgba, int width, int height) {
    std::lock_guard<std::mutex> lock(mutex);
    if (message_frames < 120) {
        ++message_frames;
        draw_text(rgba, width, height, 8, height - (glyph_height + 4) * 2 - 4, message);
    }
    if (show_fps) {
        char text[16];
        sprintf_s(text, "%3d", fps);
        draw_text(rgba, width, height, 8, 8, text);
        show_fps = false;
    }
}
