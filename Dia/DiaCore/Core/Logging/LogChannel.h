#ifndef DIA_LOG_CHANNEL_H
#define DIA_LOG_CHANNEL_H

#include <cstdio>
#include <cstring>

namespace Dia
{
	namespace Core
	{
		namespace Logging
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Log Channel
			//
			// Defines where log output is routed.
			// Supports multiple simultaneous outputs (bitfield).
			//
			// USAGE:
			//   LogChannelMask mask = LogChannel::Console | LogChannel::File;
			//   if (mask & LogChannel::Console) { /* write to console */ }
			//
			// CHANNELS:
			//   Nothing  - Suppress output (no logging)
			//   Console  - Write to stdout/stderr
			//   File     - Write to log file
			//   Server   - Send to remote logging server
			//   Database - Write to database
			//
			// FEATURES:
			//   - Bitfield for multiple outputs
			//   - Per-namespace channel configuration
			//   - Runtime channel enable/disable
			//---------------------------------------------------------------------------------------------------------------------------------

			enum LogChannel
			{
				LogChannel_Nothing  = 0,
				LogChannel_Console  = 1 << 0,
				LogChannel_File     = 1 << 1,
				LogChannel_Server   = 1 << 2,
				LogChannel_Database = 1 << 3,
				LogChannel_All      = 0xFFFFFFFF
			};

			using LogChannelMask = unsigned int;

			//---------------------------------------------------------------------------------------------------------------------------------
			// Log Channel Utilities
			//---------------------------------------------------------------------------------------------------------------------------------

			// Convert channel mask to string (for debugging)
			inline void LogChannelToString(LogChannelMask mask, char* buffer, size_t bufferSize)
			{
				if (mask == LogChannel_Nothing)
				{
					strncpy(buffer, "Nothing", bufferSize);
					return;
				}

				buffer[0] = '\0';
				bool first = true;

				auto append = [&](const char* str) {
					if (!first) strncat(buffer, ", ", bufferSize - strlen(buffer) - 1);
					strncat(buffer, str, bufferSize - strlen(buffer) - 1);
					first = false;
				};

				if (mask & LogChannel_Console)  append("Console");
				if (mask & LogChannel_File)     append("File");
				if (mask & LogChannel_Server)   append("Server");
				if (mask & LogChannel_Database) append("Database");
			}

			// Parse channel string to mask
			// Supports: "Console", "Console|File", "All", etc.
			inline LogChannelMask StringToLogChannel(const char* str)
			{
				if (!str) return LogChannel_Nothing;

				if (strcmp(str, "Nothing") == 0) return LogChannel_Nothing;
				if (strcmp(str, "All") == 0) return LogChannel_All;

				LogChannelMask mask = LogChannel_Nothing;

				if (strstr(str, "Console"))  mask |= LogChannel_Console;
				if (strstr(str, "File"))     mask |= LogChannel_File;
				if (strstr(str, "Server"))   mask |= LogChannel_Server;
				if (strstr(str, "Database")) mask |= LogChannel_Database;

				return mask;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			// Log Output Interface
			//
			// Interface for implementing custom log outputs.
			// Logger will call these methods based on channel configuration.
			//---------------------------------------------------------------------------------------------------------------------------------

			class ILogOutput
			{
			public:
				virtual ~ILogOutput() {}

				// Write log message
				virtual void Write(const char* message) = 0;

				// Flush buffered output
				virtual void Flush() = 0;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// Console Output (stdout/stderr)
			//---------------------------------------------------------------------------------------------------------------------------------

			class ConsoleLogOutput : public ILogOutput
			{
			public:
				virtual void Write(const char* message) override
				{
					printf("%s\n", message);
				}

				virtual void Flush() override
				{
					fflush(stdout);
				}
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// File Output
			//---------------------------------------------------------------------------------------------------------------------------------

			class FileLogOutput : public ILogOutput
			{
			public:
				FileLogOutput(const char* filePath)
					: mFile(nullptr)
				{
					mFile = fopen(filePath, "a"); // Append mode
				}

				~FileLogOutput()
				{
					if (mFile)
					{
						fclose(mFile);
						mFile = nullptr;
					}
				}

				virtual void Write(const char* message) override
				{
					if (mFile)
					{
						fprintf(mFile, "%s\n", message);
					}
				}

				virtual void Flush() override
				{
					if (mFile)
					{
						fflush(mFile);
					}
				}

				bool IsOpen() const { return mFile != nullptr; }

			private:
				FILE* mFile;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// Server Output (placeholder - implement based on your server protocol)
			//---------------------------------------------------------------------------------------------------------------------------------

			class ServerLogOutput : public ILogOutput
			{
			public:
				ServerLogOutput(const char* serverUrl)
				{
					// TODO: Initialize connection to logging server
					(void)serverUrl;
				}

				virtual void Write(const char* message) override
				{
					// TODO: Send message to server
					// This would use HTTP POST, TCP socket, or other protocol
					(void)message;
				}

				virtual void Flush() override
				{
					// TODO: Flush server connection
				}
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// Database Output (placeholder - implement based on your database)
			//---------------------------------------------------------------------------------------------------------------------------------

			class DatabaseLogOutput : public ILogOutput
			{
			public:
				DatabaseLogOutput(const char* connectionString)
				{
					// TODO: Initialize database connection
					(void)connectionString;
				}

				virtual void Write(const char* message) override
				{
					// TODO: Insert message into database
					// This would use SQL INSERT or NoSQL write
					(void)message;
				}

				virtual void Flush() override
				{
					// TODO: Commit database transaction
				}
			};
		}
	}
}

#endif // DIA_LOG_CHANNEL_H
