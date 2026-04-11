#ifndef DIA_CORE_ASSERT_H
#define DIA_CORE_ASSERT_H

#include "DiaCore/Core/Logging/Logger.h"
#include <cstdlib>

namespace Dia
{
	namespace Core
	{
		namespace Logging
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Assert System
			//
			// Configurable assert system integrated with logging.
			// Supports multiple assert levels with different behaviors.
			//
			// USAGE:
			//   DIA_ASSERT_TRACE("Physics", velocity >= 0.0f, "Velocity must be positive");
			//   DIA_ASSERT_DEBUG("Core", ptr != nullptr, "Pointer is null");
			//   DIA_ASSERT_WARNING("Audio", channelCount <= MAX_CHANNELS, "Too many channels");
			//   DIA_ASSERT_ERROR("Network", connected, "Not connected to server");
			//   DIA_ASSERT_FATAL("Memory", allocation != nullptr, "Out of memory");
			//
			// ASSERT LEVELS:
			//   TRACE   - Logs trace message, never breaks (even in debug)
			//   DEBUG   - Breaks in debug builds only, no-op in release
			//   WARNING - Logs warning and continues (no break)
			//   ERROR   - Logs error and continues (no break)
			//   FATAL   - Logs fatal error and crashes/terminates always
			//
			// FEATURES:
			//   - Configurable via JSON (same as logging)
			//   - Can suppress individual asserts by file:line
			//   - Can suppress entire namespaces
			//   - Integration with logging system
			//   - Custom assert handlers
			//---------------------------------------------------------------------------------------------------------------------------------

			//---------------------------------------------------------
			// Assert Handler Type
			//---------------------------------------------------------

			enum class AssertAction
			{
				Continue,  // Log and continue
				Break,     // Break into debugger
				Crash      // Terminate application
			};

			using AssertHandlerFunc = AssertAction(*)(LogLevel level, const LogNamespace& ns, const char* file, int line, const char* condition, const char* message);

			//---------------------------------------------------------
			// Assert System
			//---------------------------------------------------------

			class AssertSystem
			{
			public:
				static AssertSystem& GetInstance()
				{
					static AssertSystem instance;
					return instance;
				}

				// Set custom assert handler
				void SetAssertHandler(AssertHandlerFunc handler)
				{
					mCustomHandler = handler;
				}

				// Handle assert
				void HandleAssert(LogLevel level, const LogNamespace& ns, const char* file, int line, const char* condition, const char* message)
				{
					// Check if suppressed
					LogConfigEntry config = Logger::GetInstance().mConfig.GetConfig(ns);
					if (config.suppressed || Logger::GetInstance().mConfig.IsIndividualSuppressed(file, line))
					{
						return;
					}

					// Build assert message
					char assertMessage[1024];
					snprintf(assertMessage, sizeof(assertMessage), "Assertion failed: %s - %s", condition, message ? message : "");

					// Log the assert
					Logger::GetInstance().LogWithLocation(level, ns, file, line, "%s", assertMessage);

					// Determine action
					AssertAction action = AssertAction::Continue;

					if (mCustomHandler)
					{
						action = mCustomHandler(level, ns, file, line, condition, message);
					}
					else
					{
						action = DefaultAssertBehavior(level);
					}

					// Execute action
					switch (action)
					{
						case AssertAction::Continue:
							// Just log, continue execution
							break;

						case AssertAction::Break:
							// Break into debugger
							#if defined(_MSC_VER)
								__debugbreak();
							#elif defined(__GNUC__) || defined(__clang__)
								__builtin_trap();
							#else
								abort();
							#endif
							break;

						case AssertAction::Crash:
							// Terminate application
							Logger::GetInstance().Flush();
							abort();
							break;
					}
				}

			private:
				AssertSystem()
					: mCustomHandler(nullptr)
				{}

				// Default behavior based on level
				AssertAction DefaultAssertBehavior(LogLevel level)
				{
					switch (level)
					{
						case LogLevel::Trace:
							// Trace: just log, never break
							return AssertAction::Continue;

						case LogLevel::Debug:
							// Debug: break in debug builds, continue in release
							#ifdef DEBUG
								return AssertAction::Break;
							#else
								return AssertAction::Continue;
							#endif

						case LogLevel::Warning:
							// Warning: log and continue
							return AssertAction::Continue;

						case LogLevel::Error:
							// Error: log and continue (could break in debug if desired)
							return AssertAction::Continue;

						case LogLevel::Fatal:
							// Fatal: always crash
							return AssertAction::Crash;

						default:
							return AssertAction::Continue;
					}
				}

				AssertHandlerFunc mCustomHandler;

				// Allow Logger to access mConfig
				friend class Logger;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// Assert Macros
			//---------------------------------------------------------------------------------------------------------------------------------

			// Internal assert implementation
			#define DIA_ASSERT_IMPL(level, namespace_str, condition, message) \
				do { \
					if (!(condition)) { \
						::Dia::Core::Logging::AssertSystem::GetInstance().HandleAssert( \
							level, \
							::Dia::Core::Logging::LogNamespace(namespace_str), \
							__FILE__, __LINE__, #condition, message); \
					} \
				} while (0)

			// Trace assert - logs but never breaks
			#define DIA_ASSERT_TRACE(namespace_str, condition, message) \
				DIA_ASSERT_IMPL(::Dia::Core::Logging::LogLevel::Trace, namespace_str, condition, message)

			// Debug assert - breaks in debug builds only
			#ifdef DEBUG
				#define DIA_ASSERT_DEBUG(namespace_str, condition, message) \
					DIA_ASSERT_IMPL(::Dia::Core::Logging::LogLevel::Debug, namespace_str, condition, message)
			#else
				#define DIA_ASSERT_DEBUG(namespace_str, condition, message) ((void)0)
			#endif

			// Warning assert - logs warning but continues
			#define DIA_ASSERT_WARNING(namespace_str, condition, message) \
				DIA_ASSERT_IMPL(::Dia::Core::Logging::LogLevel::Warning, namespace_str, condition, message)

			// Error assert - logs error but continues
			#define DIA_ASSERT_ERROR(namespace_str, condition, message) \
				DIA_ASSERT_IMPL(::Dia::Core::Logging::LogLevel::Error, namespace_str, condition, message)

			// Fatal assert - always crashes
			#define DIA_ASSERT_FATAL(namespace_str, condition, message) \
				DIA_ASSERT_IMPL(::Dia::Core::Logging::LogLevel::Fatal, namespace_str, condition, message)

			// Simple assert (maps to DEBUG level)
			#define DIA_ASSERT(namespace_str, condition) \
				DIA_ASSERT_DEBUG(namespace_str, condition, "")

			//---------------------------------------------------------------------------------------------------------------------------------
			// Convenience Macros (for common namespaces)
			//---------------------------------------------------------------------------------------------------------------------------------

			#define DIA_CORE_ASSERT(condition, message) \
				DIA_ASSERT_DEBUG("Core", condition, message)

			#define DIA_CORE_ASSERT_FATAL(condition, message) \
				DIA_ASSERT_FATAL("Core", condition, message)
		}
	}
}

#endif // DIA_CORE_ASSERT_H
