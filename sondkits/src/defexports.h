#pragma once

#ifdef _WIN32
#ifdef SONDKITS_EXPORTS
#define SONDKITS_API __declspec(dllexport)
#else
#define SONDKITS_API __declspec(dllimport)
#endif
#else
#define SONDKITS_API
#endif