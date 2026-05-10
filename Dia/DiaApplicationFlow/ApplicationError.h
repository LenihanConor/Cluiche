////////////////////////////////////////////////////////////////////////////////
// Filename: ApplicationError.h
//
// Error handling types for DiaApplicationFlow framework.
// Provides graceful error recovery and reporting instead of assertion-only.
//
////////////////////////////////////////////////////////////////////////////////
#ifndef _APPLICATIONERROR_H_
#define _APPLICATIONERROR_H_

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Time/TimeAbsolute.h>

namespace Dia
{
	namespace Application
	{
		////////////////////////////////////////////////////////////////////////////////
		// Enum name: ErrorCode
		// Description: Error types that can occur during application lifecycle
		////////////////////////////////////////////////////////////////////////////////
		enum class ErrorCode
		{
			kSuccess = 0,              // No error
			kNullPointer,              // Null pointer passed where not allowed
			kInvalidState,             // Operation invalid in current state
			kTimeout,                  // Operation timed out
			kCircularDependency,       // Circular module dependency detected
			kStartupFailed,            // Module/Phase failed to start
			kModuleNotFound,           // Requested module does not exist
			kTransitionNotAllowed,     // Phase transition not in allowed list
			kAsyncTimeout,             // Async operation timed out
			kResourceLoadFailed,       // Resource loading failed
			kUnknown                   // Unknown/unspecified error
		};

		////////////////////////////////////////////////////////////////////////////////
		// Struct name: ErrorInfo
		// Description: Complete error information with context
		////////////////////////////////////////////////////////////////////////////////
		struct ErrorInfo
		{
			ErrorCode code;
			Dia::Core::StringCRC contextId;     // Module/Phase that generated error
			const char* message;                // Human-readable description
			Dia::Core::TimeAbsolute timestamp;  // When error occurred

			// Constructors
			ErrorInfo()
				: code(ErrorCode::kSuccess)
				, contextId("")
				, message(nullptr)
				, timestamp(Dia::Core::TimeAbsolute::Zero())
			{}

			ErrorInfo(ErrorCode c, const Dia::Core::StringCRC& id, const char* msg)
				: code(c)
				, contextId(id)
				, message(msg)
				, timestamp(Dia::Core::TimeAbsolute::GetSystemTime())
			{}

			// Helper methods
			bool IsSuccess() const { return code == ErrorCode::kSuccess; }
			bool IsFailure() const { return code != ErrorCode::kSuccess; }

			// Get error code as string for logging
			const char* GetErrorCodeString() const
			{
				switch (code)
				{
					case ErrorCode::kSuccess:              return "Success";
					case ErrorCode::kNullPointer:          return "NullPointer";
					case ErrorCode::kInvalidState:         return "InvalidState";
					case ErrorCode::kTimeout:              return "Timeout";
					case ErrorCode::kCircularDependency:   return "CircularDependency";
					case ErrorCode::kStartupFailed:        return "StartupFailed";
					case ErrorCode::kModuleNotFound:       return "ModuleNotFound";
					case ErrorCode::kTransitionNotAllowed: return "TransitionNotAllowed";
					case ErrorCode::kAsyncTimeout:         return "AsyncTimeout";
					case ErrorCode::kResourceLoadFailed:   return "ResourceLoadFailed";
					case ErrorCode::kUnknown:              return "Unknown";
					default:                               return "UnrecognizedErrorCode";
				}
			}
		};

		////////////////////////////////////////////////////////////////////////////////
		// Type alias: ErrorCallback
		// Description: Callback function for error notifications
		////////////////////////////////////////////////////////////////////////////////
		typedef void (*ErrorCallback)(const ErrorInfo& error);
	}
}

#endif // _APPLICATIONERROR_H_
