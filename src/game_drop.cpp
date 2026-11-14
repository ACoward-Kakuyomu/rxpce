#include "game_drop.h"
#include <filesystem>
#include <memory>
#include <type_traits>
#include <cwctype>

namespace rxpce_ui {
DropSelection read_game_drop(HDROP drop,bool available) {
    std::unique_ptr<std::remove_pointer_t<HDROP>,decltype(&DragFinish)> owner(drop,&DragFinish);
    if(!available) return {{},DropError::busy};
    if(!drop) return {{},DropError::path};
    if(DragQueryFileW(drop,0xffffffff,nullptr,0)!=1) return {{},DropError::count};
    const auto length=DragQueryFileW(drop,0,nullptr,0);
    if(!length || length>32767) return {{},DropError::path};
    std::wstring path(length+1,L'\0');
    if(DragQueryFileW(drop,0,path.data(),length+1)!=length) return {{},DropError::path};
    path.resize(length);
    const std::filesystem::path file(path);
    if(!file.is_absolute()) return {{},DropError::path};
    auto extension=file.extension().wstring();
    for(auto& c:extension)c=static_cast<wchar_t>(std::towlower(c));
    if(extension!=L".pce" && extension!=L".zip" && extension!=L".cue" && extension!=L".chd")
        return {{},DropError::unsupported};
    return {std::move(path),DropError::none};
}
}
