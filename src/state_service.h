#pragma once
#include <filesystem>

void SaveState();
void LoadState();
std::filesystem::path StateFilePath();
