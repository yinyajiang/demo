#include <algorithm>
#include "utils.h"
#include <cctype>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <vector>
#if _WIN32
#include <Windows.h>
#endif

std::string ws2u8(const std::wstring &ws) {
#if _WIN32
  int buffer_size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0,
                                        nullptr, nullptr);
  if (buffer_size == 0) {
    return std::string();
  }
  std::vector<char> buffer(buffer_size);
  WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, buffer.data(), buffer_size,
                      nullptr, nullptr);
  return std::string(buffer.data());
#else
  std::mbstate_t state = std::mbstate_t();
  const wchar_t *src = ws.c_str();
  size_t len = std::wcsrtombs(nullptr, &src, 0, &state);
  if (len == static_cast<size_t>(-1)) {
    return std::string();
  }
  std::vector<char> buffer(len + 1);
  src = ws.c_str();
  std::wcsrtombs(buffer.data(), &src, len + 1, &state);
  return std::string(buffer.data());
#endif
}

std::wstring u82ws(const std::string &u8){
  if (u8.empty()) {
    return std::wstring();
  }
#if _WIN32
  int buffer_size = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0);
  if (buffer_size == 0) {
    return std::wstring();
  }
  std::vector<wchar_t> buffer(buffer_size);
  MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, buffer.data(), buffer_size);
  return std::wstring(buffer.data());
#else
  std::mbstate_t state = std::mbstate_t();
  const char *src = u8.c_str();
  size_t len = std::mbsrtowcs(nullptr, &src, 0, &state);
  if (len == static_cast<size_t>(-1)) {
    return std::wstring();
  }
  std::vector<wchar_t> buffer(len + 1);
  src = u8.c_str();
  std::mbsrtowcs(buffer.data(), &src, len + 1, &state);
  return std::wstring(buffer.data());
#endif
}

std::string fs2u8(const std::filesystem::path &path) {
#if _WIN32
  std::wstring wide_path = path.wstring();
  return ws2u8(wide_path);
#else
  return path.string();
#endif
}

std::string toLower(const std::string &str) {
  std::string lower_str = str;
  std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
                 ::tolower);
  return lower_str;
}


std::filesystem::path u82fs(const std::string &u8){
#if _WIN32
  std::wstring wide_path = u82ws(u8);
  return std::filesystem::path(wide_path);
#else
  return std::filesystem::path(u8);
#endif
}
