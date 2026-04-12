#ifndef DIA_LOGGER_H
#define DIA_LOGGER_H

#include "DiaCore/Core/Logging/LogLevel.h"
#include "DiaCore/Core/Logging/LogNamespace.h"
#include "DiaCore/Core/Logging/LogChannel.h"
#include "DiaCore/Core/Logging/LogConfig.h"
#include "DiaCore/Threading/Mutex.h"
#include "DiaCore/Time/TimeAbsolute.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <memory>

namespace Dia
{
	namespace Core
	{
		namespace Logging
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Logger
			//
			// Main logging system that integrates level, namespace, channel, and configuration.
			//
			// USAGE:
			//   Logger& logger = Logger::GetInstance();
			//
			//   // Load config
			//   logger.LoadConfig("logging.json");
			//
			//   // Log messages
			//   logger.Log(LogLevel::Info, LogNamespaces::Physics(), "Velocity: %.2f", velocity);
			//   logger.Log(LogLevel::Error, LogNamespaces::Rendering(), "Failed to load texture");
			//
			//   // Macros (recommended)
			//   DIA_LOG_INFO("Physics", "Velocity: %.2f", velocity);
			//   DIA_LOG_ERROR("Rendering", "Failed to load texture");
			//
			// FEATURES:
			//   - Thread-safe logging
			//   - Configurable routing (console, file, server, database)
			//   - Namespace-based filtering
			//   - Runtime configuration changes
			//   - Formatted output with timestamps
			//   - File/line tracking
			//---------------------------------------------------------------------------------------------------------------------------------

			class Logger
			{
			public:
				~Logger()
				{
					Shutdown();
				}

				// Get singleton instance
				static Logger& GetInstance()
				{
					static Logger instance;
					return instance;
				}

				//---------------------------------------------------------
				// Initialization
				//---------------------------------------------------------

				void Initialize(const char* logFilePath = "game.log")
				{
					ScopedLock<Mutex> lock(mMutex);

					// Initialize default outputs
					mConsoleOutput = std::make_unique<ConsoleLogOutput>();
					mFileOutput = std::make_unique<FileLogOutput>(logFilePath);

					// Default config: INFO level, console + file
					mConfig.SetDefaultConfig(LogLevel::Info, LogChannel_Console | LogChannel_File);
				}

				void Shutdown()
				{
					ScopedLock<Mutex> lock(mMutex);
					FlushInternal();
					mConsoleOutput.reset();
					mFileOutput.reset();
					mServerOutput.reset();
					mDatabaseOutput.reset();
				}

				//---------------------------------------------------------
				// Configuration
				//---------------------------------------------------------

				bool LoadConfig(const char* filePath)
				{
					return mConfig.LoadFromFile(filePath);
				}

				bool SaveConfig(const char* filePath) const
				{
					return mConfig.SaveToFile(filePath);
				}

				void SetDefaultConfig(LogLevel minLevel, LogChannelMask channels)
				{
					mConfig.SetDefaultConfig(minLevel, channels);
				}

				void SetNamespaceConfig(const LogNamespace& ns, LogLevel minLevel, LogChannelMask channels)
				{
					mConfig.SetNamespaceConfig(ns, minLevel, channels);
				}

				void SuppressNamespace(const LogNamespace& ns, bool suppressed)
				{
					mConfig.SuppressNamespace(ns, suppressed);
				}

				void SuppressIndividual(const char* fileAndLine, bool suppressed)
				{
					mConfig.SuppressIndividual(fileAndLine, suppressed);
				}

				//---------------------------------------------------------
				// Logging
				//---------------------------------------------------------

				// Log formatted message
				void Log(LogLevel level, const LogNamespace& ns, const char* format, ...)
				{
					// Check if this level/namespace is enabled
					LogConfigEntry config = mConfig.GetConfig(ns);
					if (config.suppressed || !IsLogLevelEnabled(level, config.minLevel))
					{
						return;
					}

					// Format message
					char message[2048];
					va_list args;
					va_start(args, format);
					vsnprintf(message, sizeof(message), format, args);
					va_end(args);

					// Output to configured channels
					OutputToChannels(level, ns, message, config.channels);
				}

				// Log formatted message with file/line
				void LogWithLocation(LogLevel level, const LogNamespace& ns, const char* file, int line, const char* format, ...)
				{
					// Check individual suppression first
					if (mConfig.IsIndividualSuppressed(file, line))
					{
						return;
					}

					// Check if this level/namespace is enabled
					LogConfigEntry config = mConfig.GetConfig(ns);
					if (config.suppressed || !IsLogLevelEnabled(level, config.minLevel))
					{
						return;
					}

					// Format message
					char message[2048];
					va_list args;
					va_start(args, format);
					vsnprintf(message, sizeof(message), format, args);
					va_end(args);

					// Output to configured channels (with file/line)
					OutputToChannelsWithLocation(level, ns, file, line, message, config.channels);
				}

				// Flush all outputs
				void Flush()
				{
					ScopedLock<Mutex> lock(mMutex);
					FlushInternal();
				}

			private:
				Logger() {}

				// Prevent copying
				Logger(const Logger&) = delete;
				Logger& operator=(const Logger&) = delete;

				//---------------------------------------------------------
				// Internal Output
				//---------------------------------------------------------

				void OutputToChannels(LogLevel level, const LogNamespace& ns, const char* message, LogChannelMask channels)
				{
					ScopedLock<Mutex> lock(mMutex);

					// Build formatted message
					char formattedMessage[4096];
					FormatMessage(level, ns, nullptr, 0, message, formattedMessage, sizeof(formattedMessage));

					// Write to each enabled channel
					if (channels & LogChannel_Console && mConsoleOutput)
					{
						mConsoleOutput->Write(formattedMessage);
					}

					if (channels & LogChannel_File && mFileOutput)
					{
						mFileOutput->Write(formattedMessage);
					}

					if (channels & LogChannel_Server && mServerOutput)
					{
						mServerOutput->Write(formattedMessage);
					}

					if (channels & LogChannel_Database && mDatabaseOutput)
					{
						mDatabaseOutput->Write(formattedMessage);
					}
				}

				void OutputToChannelsWithLocation(LogLevel level, const LogNamespace& ns, const char* file, int line, const char* message, LogChannelMask channels)
				{
					ScopedLock<Mutex> lock(mMutex);

					// Build formatted message
					char formattedMessage[4096];
					FormatMessage(level, ns, file, line, message, formattedMessage, sizeof(formattedMessage));

					// Write to each enabled channel
					if (channels & LogChannel_Console && mConsoleOutput)
					{
						mConsoleOutput->Write(formattedMessage);
					}

					if (channels & LogChannel_File && mFileOutput)
					{
						mFileOutput->Write(formattedMessage);
					}

					if (channels & LogChannel_Server && mServerOutput)
					{
						mServerOutput->Write(formattedMessage);
					}

					if (channels & LogChannel_Database && mDatabaseOutput)
					{
						mDatabaseOutput->Write(formattedMessage);
					}
				}

				// Format message with timestamp, level, namespace, file/line
				void FormatMessage(LogLevel level, const LogNamespace& ns, const char* file, int line, const char* message, char* output, size_t outputSize)
				{
					// Get current timestamp
					std::time_t now = std::time(nullptr);
					char timeStr[64];

					#ifdef _MSC_VER
					std::tm localTime;
					if (localtime_s(&localTime, &now) == 0)
					{
						snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
							localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
							localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
					}
					#else
					std::tm* localTimePtr = std::localtime(&now);
					if (localTimePtr)
					{
						snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
							localTimePtr->tm_year + 1900, localTimePtr->tm_mon + 1, localTimePtr->tm_mday,
							localTimePtr->tm_hour, localTimePtr->tm_min, localTimePtr->tm_sec);
					}
					#endif
					else
					{
						snprintf(timeStr, sizeof(timeStr), "0000-00-00 00:00:00");
					}

					// Format: [TIMESTAMP] [LEVEL] [NAMESPACE] (file:line) message
					if (file && line > 0)
					{
						snprintf(output, outputSize, "[%s] [%-7s] [%s] (%s:%d) %s",
							timeStr, LogLevelToString(level), ns.GetName(), file, line, message);
					}
					else
					{
						snprintf(output, outputSize, "[%s] [%-7s] [%s] %s",
							timeStr, LogLevelToString(level), ns.GetName(), message);
					}
				}

				void FlushInternal()
				{
					if (mConsoleOutput) mConsoleOutput->Flush();
					if (mFileOutput) mFileOutput->Flush();
					if (mServerOutput) mServerOutput->Flush();
					if (mDatabaseOutput) mDatabaseOutput->Flush();
				}

				//---------------------------------------------------------
				// Data
				//---------------------------------------------------------

				LogConfig mConfig;

				std::unique_ptr<ConsoleLogOutput> mConsoleOutput;
				std::unique_ptr<FileLogOutput> mFileOutput;
				std::unique_ptr<ServerLogOutput> mServerOutput;
				std::unique_ptr<DatabaseLogOutput> mDatabaseOutput;

				Mutex mMutex;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// Logging Macros (recommended usage)
			//---------------------------------------------------------------------------------------------------------------------------------

			#define DIA_LOG_TRACE(namespace_str, ...) \
				::Dia::Core::Logging::Logger::GetInstance().LogWithLocation( \
					::Dia::Core::Logging::LogLevel::Trace, \
					::Dia::Core::Logging::LogNamespace(namespace_str), \
					__FILE__, __LINE__, __VA_ARGS__)

			#define DIA_LOG_DEBUG(namespace_str, ...) \
				::Dia::Core::Logging::Logger::GetInstance().LogWithLocation( \
					::Dia::Core::Logging::LogLevel::Debug, \
					::Dia::Core::Logging::LogNamespace(namespace_str), \
					__FILE__, __LINE__, __VA_ARGS__)

			#define DIA_LOG_INFO(namespace_str, ...) \
				::Dia::Core::Logging::Logger::GetInstance().LogWithLocation( \
					::Dia::Core::Logging::LogLevel::Info, \
					::Dia::Core::Logging::LogNamespace(namespace_str), \
					__FILE__, __LINE__, __VA_ARGS__)

			#define DIA_LOG_WARNING(namespace_str, ...) \
				::Dia::Core::Logging::Logger::GetInstance().LogWithLocation( \
					::Dia::Core::Logging::LogLevel::Warning, \
					::Dia::Core::Logging::LogNamespace(namespace_str), \
					__FILE__, __LINE__, __VA_ARGS__)

			#define DIA_LOG_ERROR(namespace_str, ...) \
				::Dia::Core::Logging::Logger::GetInstance().LogWithLocation( \
					::Dia::Core::Logging::LogLevel::Error, \
					::Dia::Core::Logging::LogNamespace(namespace_str), \
					__FILE__, __LINE__, __VA_ARGS__)

			#define DIA_LOG_FATAL(namespace_str, ...) \
				::Dia::Core::Logging::Logger::GetInstance().LogWithLocation( \
					::Dia::Core::Logging::LogLevel::Fatal, \
					::Dia::Core::Logging::LogNamespace(namespace_str), \
					__FILE__, __LINE__, __VA_ARGS__)
		}
	}
}

#endif // DIA_LOGGER_H
