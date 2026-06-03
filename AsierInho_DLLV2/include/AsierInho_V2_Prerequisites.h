#pragma once

// #define _TIME_PROFILING
//  Define ASIERINHO version
#define ASIERINHO_V2_VERSION_MAJOR 2
#define ASIERINHO_V2_VERSION_MINOR 0
#define ASIERINHO_V2_VERSION_PATCH 0
#define ASIERINHO_V2_VERSION_SUFFIX ""
#define ASIERINHO_V2_VERSION_NAME "Zoe"

#define ASIERINHO_V2_VERSION                                                   \
  ((ASIERINHO_VERSION_MAJOR << 16) | (ASIERINHO_VERSION_MINOR << 8) |          \
   ASIERINHO_VERSION_PATCH)

#if defined(ASIERINHO_V2_NONCLIENT_BUILD)
#if defined _WIN32
#define _AsierInho_Export_V2 __declspec(dllexport)
#else
#define _AsierInho_Export_V2 __attribute__((visibility("default")))
#endif
#else
#if defined(__MINGW32__)
#define _AsierInho_Export_V2
#else
#define _AsierInho_Export_V2 __declspec(dllimport)
#endif
#endif
#define AsierInho_V2_Handler long long