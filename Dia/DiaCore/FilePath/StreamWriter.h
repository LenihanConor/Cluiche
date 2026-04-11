#ifndef DIA_STREAM_WRITER_H
#define DIA_STREAM_WRITER_H

#include "DiaCore/Core/Assert.h"
#include <cstdio>
#include <cstring>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Stream Writer
		//
		// Buffered file writing for efficient sequential writes.
		//
		// USAGE:
		//   StreamWriter writer("output.dat");
		//   if (writer.IsOpen()) {
		//       writer.Write(42);
		//       writer.WriteLine("Hello World");
		//       writer.Flush();
		//   }
		//
		// FEATURES:
		//   - Buffered writes (reduce system calls)
		//   - Line writing with newlines
		//   - Auto-flush on close
		//   - Type-safe write operations
		//---------------------------------------------------------------------------------------------------------------------------------

		class StreamWriter
		{
		public:
			static const size_t DefaultBufferSize = 4096;

			// Constructors
			StreamWriter()
				: mFile(nullptr)
				, mBuffer(nullptr)
				, mBufferSize(0)
				, mBufferPos(0)
			{}

			explicit StreamWriter(const char* filePath, bool append = false,
				size_t bufferSize = DefaultBufferSize)
				: mFile(nullptr)
				, mBuffer(nullptr)
				, mBufferSize(0)
				, mBufferPos(0)
			{
				Open(filePath, append, bufferSize);
			}

			~StreamWriter()
			{
				Close();
			}

			// Open file for writing
			bool Open(const char* filePath, bool append = false,
				size_t bufferSize = DefaultBufferSize)
			{
				Close();

				const char* mode = append ? "ab" : "wb";
				mFile = fopen(filePath, mode);
				if (!mFile)
				{
					return false;
				}

				mBufferSize = bufferSize;
				mBuffer = new char[mBufferSize];
				mBufferPos = 0;

				return true;
			}

			// Close file (auto-flushes)
			void Close()
			{
				if (mFile)
				{
					Flush();
					fclose(mFile);
					mFile = nullptr;
				}

				if (mBuffer)
				{
					delete[] mBuffer;
					mBuffer = nullptr;
				}

				mBufferSize = 0;
				mBufferPos = 0;
			}

			// Check if file is open
			bool IsOpen() const
			{
				return mFile != nullptr;
			}

			// Write raw bytes
			size_t Write(const void* data, size_t size)
			{
				DIA_ASSERT(IsOpen(), "File not open");

				size_t totalWritten = 0;
				const char* src = static_cast<const char*>(data);

				while (size > 0)
				{
					// Flush if buffer is full
					if (mBufferPos >= mBufferSize)
					{
						if (!Flush())
						{
							break;
						}
					}

					// Copy to buffer
					size_t available = mBufferSize - mBufferPos;
					size_t toCopy = (size < available) ? size : available;

					memcpy(mBuffer + mBufferPos, src, toCopy);

					src += toCopy;
					mBufferPos += toCopy;
					totalWritten += toCopy;
					size -= toCopy;
				}

				return totalWritten;
			}

			// Write a single value
			template <typename T>
			bool Write(const T& value)
			{
				return Write(&value, sizeof(T)) == sizeof(T);
			}

			// Write a C string
			bool WriteString(const char* str)
			{
				if (!str) return false;
				return Write(str, strlen(str)) == strlen(str);
			}

			// Write a line (adds newline)
			bool WriteLine(const char* str)
			{
				if (!WriteString(str)) return false;
				return WriteString("\n");
			}

			// Write formatted text
			bool WriteFormat(const char* format, ...)
			{
				DIA_ASSERT(IsOpen(), "File not open");

				char buffer[1024];
				va_list args;
				va_start(args, format);
				int written = vsnprintf(buffer, sizeof(buffer), format, args);
				va_end(args);

				if (written > 0)
				{
					return Write(buffer, written) == static_cast<size_t>(written);
				}

				return false;
			}

			// Flush buffer to disk
			bool Flush()
			{
				if (!mFile || mBufferPos == 0)
				{
					return true;
				}

				size_t written = fwrite(mBuffer, 1, mBufferPos, mFile);
				bool success = (written == mBufferPos);

				mBufferPos = 0;

				if (success)
				{
					fflush(mFile);
				}

				return success;
			}

			// Get current position in file
			long long Tell() const
			{
				if (!mFile) return -1;
				return ftell(mFile) + mBufferPos;
			}

			// Seek to position (flushes buffer first)
			bool Seek(long long offset, int origin = SEEK_SET)
			{
				if (!mFile) return false;

				if (!Flush())
				{
					return false;
				}

				return fseek(mFile, offset, origin) == 0;
			}

		private:
			// Prevent copying
			StreamWriter(const StreamWriter&) = delete;
			StreamWriter& operator=(const StreamWriter&) = delete;

			FILE* mFile;
			char* mBuffer;
			size_t mBufferSize;
			size_t mBufferPos;
		};
	}
}

#endif // DIA_STREAM_WRITER_H
