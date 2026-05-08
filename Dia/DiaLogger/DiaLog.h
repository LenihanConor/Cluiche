#pragma once

#include <DiaLogger/Logger.h>
#include <DiaLogger/LogLevel.h>
#include <DiaCore/CRC/StringCRC.h>

#if defined(NDEBUG)
	#define DIA_LOG_TRACE(channel, fmt, ...) ((void)0)
	#define DIA_LOG_DEBUG(channel, fmt, ...) ((void)0)
#else
	#define DIA_LOG_TRACE(channel, fmt, ...) \
		Dia::Logger::Logger::Instance().Log( \
			Dia::Logger::LogLevel::kTrace, \
			Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)
	#define DIA_LOG_DEBUG(channel, fmt, ...) \
		Dia::Logger::Logger::Instance().Log( \
			Dia::Logger::LogLevel::kDebug, \
			Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)
#endif

#define DIA_LOG_INFO(channel, fmt, ...) \
	Dia::Logger::Logger::Instance().Log( \
		Dia::Logger::LogLevel::kInfo, \
		Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)

#define DIA_LOG_WARNING(channel, fmt, ...) \
	Dia::Logger::Logger::Instance().Log( \
		Dia::Logger::LogLevel::kWarning, \
		Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)

#define DIA_LOG_ERROR(channel, fmt, ...) \
	Dia::Logger::Logger::Instance().Log( \
		Dia::Logger::LogLevel::kError, \
		Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)

#define DIA_LOG(level, channel, fmt, ...) \
	Dia::Logger::Logger::Instance().Log( \
		level, Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)
