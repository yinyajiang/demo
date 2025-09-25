#include <algorithm>
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
