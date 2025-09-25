#pragma once

#include "defexports.h"
#include <filesystem>
#include <string>

SOUNDKITS_API std::string fs2u8(const std::filesystem::path &path);
SOUNDKITS_API std::string ws2u8(const std::wstring &ws);
SOUNDKITS_API std::string toLower(const std::string &str);