#ifndef DIA_WEBSOCKET_ERROR_H
#define DIA_WEBSOCKET_ERROR_H

#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace Dia
{
	namespace WebSocket
	{
		enum class ErrorCode
		{
			kNone,

			kConnectionFailed,
			kConnectionTimeout,
			kConnectionRejected,
			kConnectionCloseFailed,

			kMessageTooLarge,
			kMessageQueueFull,
			kSendFailed,

			kReconnectFailed,
			kReconnectExhausted,

			kInternalError
		};

		enum class ErrorSeverity
		{
			kWarning,
			kError,
			kFatal
		};

		struct Error
		{
			ErrorCode code;
			ErrorSeverity severity;
			int connectionId;
			char message[256];

			const char* GetMessage() const { return message; }

			const char* GetCodeName() const
			{
				switch (code)
				{
				case ErrorCode::kNone:                  return "kNone";
				case ErrorCode::kConnectionFailed:      return "kConnectionFailed";
				case ErrorCode::kConnectionTimeout:     return "kConnectionTimeout";
				case ErrorCode::kConnectionRejected:    return "kConnectionRejected";
				case ErrorCode::kConnectionCloseFailed: return "kConnectionCloseFailed";
				case ErrorCode::kMessageTooLarge:       return "kMessageTooLarge";
				case ErrorCode::kMessageQueueFull:      return "kMessageQueueFull";
				case ErrorCode::kSendFailed:            return "kSendFailed";
				case ErrorCode::kReconnectFailed:       return "kReconnectFailed";
				case ErrorCode::kReconnectExhausted:    return "kReconnectExhausted";
				case ErrorCode::kInternalError:         return "kInternalError";
				default:                                return "kUnknown";
				}
			}

			const char* GetSeverityName() const
			{
				switch (severity)
				{
				case ErrorSeverity::kWarning: return "WARNING";
				case ErrorSeverity::kError:   return "ERROR";
				case ErrorSeverity::kFatal:   return "FATAL";
				default:                      return "UNKNOWN";
				}
			}
		};

		namespace Internal
		{
			inline Error MakeError(ErrorCode code, ErrorSeverity severity, int connectionId, const char* fmt, ...)
			{
				Error err;
				err.code = code;
				err.severity = severity;
				err.connectionId = connectionId;

				va_list args;
				va_start(args, fmt);
				vsnprintf(err.message, sizeof(err.message), fmt, args);
				va_end(args);

				return err;
			}
		}
	}
}

#endif // DIA_WEBSOCKET_ERROR_H
