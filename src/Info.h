// Info.h

#ifndef HDEF_INFO_H
#define HDEF_INFO_H

#include <windows.h>

#include <cstdint>

void InitInfo(void);
void TrashInfo(void);
void SetInfoString(LPCTSTR szInfo);
void SetInfoFps(int fps);
void DisplayInfo(std::uint32_t* rgba, int width, int height);

#endif // HDEF_INFO_H
