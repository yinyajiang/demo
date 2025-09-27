#pragma once

#include "defexports.h"
#include <filesystem>
#include <string>

SONDKITS_API std::string fs2u8(const std::filesystem::path &path);
SONDKITS_API std::string ws2u8(const std::wstring &ws);
SONDKITS_API std::wstring u82ws(const std::string &u8);
SONDKITS_API std::string toLower(const std::string &str);
SONDKITS_API std::filesystem::path u82fs(const std::string &u8);
SONDKITS_API bool hasPrefix(const std::string &str, const std::string &prefix);
SONDKITS_API bool hasSuffix(const std::string &str, const std::string &suffix);
SONDKITS_API std::string cutPrefix(const std::string &str,
                                   const std::string &prefix);
