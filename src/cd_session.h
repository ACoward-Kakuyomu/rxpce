#pragma once
#include "system_card.h"
#include "disc_image.h"

namespace rxpce_cd {
struct Session {
    SystemCard bios;
    DiscImage disc;
    std::wstring source;
};
std::wstring bios_path(const std::wstring& path);
bool prepare(const std::wstring& image, const std::wstring& bios, std::unique_ptr<Session>& output,
             std::string& error, const ImageProgress& progress = {});
void queue(std::unique_ptr<Session> session);
std::unique_ptr<Session> take();
} // namespace rxpce_cd
