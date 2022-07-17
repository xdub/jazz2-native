﻿#pragma once

#define ENABLE_LOG

#if !defined(_MSC_VER) && defined(__has_include)
#   if __has_include("../Shared/Common.h")
#       define __HAS_LOCAL_COMMON
#   endif
#endif
#ifdef __HAS_LOCAL_COMMON
#   include "../Shared/Common.h"
#else
#   include <Common.h>
#endif

// These attributes are not defined outside of MSVC
#if !defined(__in)
#	define __in
#endif
#if !defined(__in_opt)
#	define __in_opt
#endif
#if !defined(__out)
#	define __out
#endif
#if !defined(__out_opt)
#	define __out_opt
#endif
#if !defined(__success)
#	define __success(expr)
#endif

#if defined(ENABLE_LOG)

enum class LogLevel {
	Unknown,
	Verbose,
	Debug,
	Info,
	Warn,
	Error,
	Fatal,
	Off
};

void __WriteLog(LogLevel logLevel, const char* fmt, ...);

#ifdef __GNUC__
#	define FUNCTION __PRETTY_FUNCTION__
#elif _MSC_VER
#	define FUNCTION __FUNCTION__
#else
#	define FUNCTION __func__
#endif

#define LOGV_X(fmt, ...) __WriteLog(LogLevel::Verbose, static_cast<const char *>("%s -> " fmt), FUNCTION, ##__VA_ARGS__)
#define LOGD_X(fmt, ...) __WriteLog(LogLevel::Debug, static_cast<const char *>("%s -> " fmt), FUNCTION, ##__VA_ARGS__)
#define LOGI_X(fmt, ...) __WriteLog(LogLevel::Info, static_cast<const char *>("%s, -> " fmt), FUNCTION, ##__VA_ARGS__)
#define LOGW_X(fmt, ...) __WriteLog(LogLevel::Warn, static_cast<const char *>("%s -> " fmt), FUNCTION, ##__VA_ARGS__)
#define LOGE_X(fmt, ...) __WriteLog(LogLevel::Error, static_cast<const char *>("%s -> " fmt), FUNCTION, ##__VA_ARGS__)
#define LOGF_X(fmt, ...) __WriteLog(LogLevel::Fatal, static_cast<const char *>("%s -> " fmt), FUNCTION, ##__VA_ARGS__)

#define LOGV(fmt) __WriteLog(LogLevel::Verbose, static_cast<const char *>("%s -> " fmt), FUNCTION)
#define LOGD(fmt) __WriteLog(LogLevel::Debug, static_cast<const char *>("%s -> " fmt), FUNCTION)
#define LOGI(fmt) __WriteLog(LogLevel::Info, static_cast<const char *>("%s, -> " fmt), FUNCTION)
#define LOGW(fmt) __WriteLog(LogLevel::Warn, static_cast<const char *>("%s -> " fmt), FUNCTION)
#define LOGE(fmt) __WriteLog(LogLevel::Error, static_cast<const char *>("%s -> " fmt), FUNCTION)
#define LOGF(fmt) __WriteLog(LogLevel::Fatal, static_cast<const char *>("%s -> " fmt), FUNCTION)

#else

#define LOGV_X(fmt, ...)
#define LOGD_X(fmt, ...)
#define LOGI_X(fmt, ...)
#define LOGW_X(fmt, ...)
#define LOGE_X(fmt, ...)
#define LOGF_X(fmt, ...)

#define LOGV(fmt)
#define LOGD(fmt)
#define LOGI(fmt)
#define LOGW(fmt)
#define LOGE(fmt)
#define LOGF(fmt)

#endif

// Return assert macros
#define RETURN_ASSERT_MSG_X(x, fmt, ...) do { if (!(x)) { LOGE_X(fmt, ##__VA_ARGS__); return; } } while (false)
#define RETURN_ASSERT_MSG(x, fmt) do { if (!(x)) { LOGE(fmt); return; } } while (false)
#define RETURN_ASSERT(x) do { if (!(x)) { LOGE("RETURN_ASSERT(" #x ")"); return; } } while (false)

// Return macros
#define RETURN_MSG_X(fmt, ...) do { LOGE_X(fmt, ##__VA_ARGS__); return; } while (false)
#define RETURN_MSG(fmt) do { LOGE(fmt); return; } while (false)

// Return false assert macros
#define RETURNF_ASSERT_MSG_X(x, fmt, ...) do { if (!(x)) { LOGE_X(fmt, ##__VA_ARGS__); return false; } } while (false)
#define RETURNF_ASSERT_MSG(x, fmt) do { if (!(x)) { LOGE(fmt); return false; } } while (false)
#define RETURNF_ASSERT(x) do { if (!(x)) { LOGE("RETURNF_ASSERT(" #x ")"); return false; } } while (false)

// Return false macros
#define RETURNF_MSG_X(fmt, ...) do { LOGE_X(fmt, ##__VA_ARGS__); return false; } while (false)
#define RETURNF_MSG(fmt) do { LOGE(fmt); return false; } while (false)