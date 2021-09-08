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

static constexpr bool kEnableOverlay = true;