#include <thread>
#include <filesystem>
#include <vector>
#include <cwchar>
#include <cstdlib>
#if _WIN32
#include <Windows.h>
#endif

std::string fs2u8(const std::filesystem::path& path) {
  #if _WIN32
    std::wstring wide_path = path.wstring();
    return ws2u8(wide_path);
  #else
    return path.string();
  #endif
}

std::string ws2u8(const std::wstring &ws) {
  #if _WIN32
    int buffer_size =
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (buffer_size == 0) {
        return std::string();
    }
    std::vector<char> buffer(buffer_size);
    WideCharToMultiByte(
        CP_UTF8, 0, ws.c_str(), -1, buffer.data(), buffer_size, nullptr, nullptr
    );
    return std::string(buffer.data());
  #else
    // 在非Windows平台，使用locale转换宽字符到UTF-8
    std::mbstate_t state = std::mbstate_t();
    const wchar_t* src = ws.c_str();
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
