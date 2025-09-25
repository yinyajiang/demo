#pragma once

#ifdef _WIN32
#ifdef SOUNDKITS_EXPORTS
#define SOUNDKITS_API __declspec(dllexport)
#else
#define SOUNDKITS_API __declspec(dllimport)
#endif
#else
#define SOUNDKITS_API
#endif