#pragma once

#ifdef NDEBUG
static constexpr bool kEnableDebugMessengerExtension = false;
#else
static constexpr bool kEnableDebugMessengerExtension = true;
#endif

#ifdef NDEBUG
static constexpr bool kEnableDebugUtilsExtension = false;
#else
static constexpr bool kEnableDebugUtilsExtension = true;
#endif

#ifdef NDEBUG
static constexpr bool kEnableValidation = false;
#else
static constexpr bool kEnableValidation = true;
#endif

#if defined(WIN32) || defined(_WIN32) || \
    defined(__WIN32) && !defined(__CYGWIN__)
static constexpr bool kEnableLunarGMonitor = true;
static constexpr bool kEnableOverlay = false;
#else
static constexpr bool kEnableLunarGMonitor = false;
static constexpr bool kEnableOverlay = true;
#endif