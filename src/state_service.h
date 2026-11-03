#pragma once
#include "replay_codec.h"
#include <filesystem>
#include <string>
#include <vector>

std::vector<char> CaptureStateFile();
rxpce_replay::Digest CaptureReplayDigest();
bool RestoreStateFile(const std::vector<char>& file, bool& sound_ok, std::string& error);
void SaveState();
void LoadState();
std::filesystem::path StateFilePath();
