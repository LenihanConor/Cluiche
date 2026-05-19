#pragma once

#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/LogLevel.h>
#include <DiaCore/CRC/StringCRC.h>

#if defined(NDEBUG)
	#define DIA_LOG_TRACE(channel, fmt, ...) ((void)0)
	#define DIA_LOG_DEBUG(channel, fmt, ...) ((void)0)
#else
	#define DIA_LOG_TRACE(channel, fmt, ...) \
		Dia::Observation::Log::Logger::Instance().Log( \
			Dia::Observation::Log::LogLevel::kTrace, \
			Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)
	#define DIA_LOG_DEBUG(channel, fmt, ...) \
		Dia::Observation::Log::Logger::Instance().Log( \
			Dia::Observation::Log::LogLevel::kDebug, \
			Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)
#endif

#define DIA_LOG_INFO(channel, fmt, ...) \
	Dia::Observation::Log::Logger::Instance().Log( \
		Dia::Observation::Log::LogLevel::kInfo, \
		Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)

#define DIA_LOG_WARNING(channel, fmt, ...) \
	Dia::Observation::Log::Logger::Instance().Log( \
		Dia::Observation::Log::LogLevel::kWarning, \
		Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)

#define DIA_LOG_ERROR(channel, fmt, ...) \
	Dia::Observation::Log::Logger::Instance().Log( \
		Dia::Observation::Log::LogLevel::kError, \
		Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)

#define DIA_LOG(level, channel, fmt, ...) \
	Dia::Observation::Log::Logger::Instance().Log( \
		level, Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)
