#ifndef DIA_STREAM_READER_H
#define DIA_STREAM_READER_H

#include "DiaCore/Core/Assert.h"
#include <cstdio>
#include <cstring>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Stream Reader
		//
		// Buffered file reading for efficient sequential access.
		//
		// USAGE:
		//   StreamReader reader("data.txt");
		//   if (reader.IsOpen()) {
		//       int value;
		//       reader.Read(value);
		//       const char* line = reader.ReadLine();
		//   }
		//
		// FEATURES:
		//   - Buffered reads (reduce system calls)
		//   - Line-by-line reading
		//   - Binary and text modes
		//   - Type-safe read operations
		//---------------------------------------------------------------------------------------------------------------------------------

		class StreamReader
		{
		public:
			static const size_t DefaultBufferSize = 4096;

			// Constructors
			StreamReader()
				: mFile(nullptr)
				, mBuffer(nullptr)
				, mBufferSize(0)
				, mBufferPos(0)
				, mBufferEnd(0)
				, mEOF(false)
			{}

			explicit StreamReader(const char* filePath, size_t bufferSize = DefaultBufferSize)
				: mFile(nullptr)
				, mBuffer(nullptr)
				, mBufferSize(0)
				, mBufferPos(0)
				, mBufferEnd(0)
				, mEOF(false)
			{
				Open(filePath, bufferSize);
			}

			~StreamReader()
			{
				Close();
			}

			// Open file
			bool Open(const char* filePath, size_t bufferSize = DefaultBufferSize)
			{
				Close();

				mFile = fopen(filePath, "rb");
				if (!mFile)
				{
					return false;
				}

				mBufferSize = bufferSize;
				mBuffer = new char[mBufferSize];
				mBufferPos = 0;
				mBufferEnd = 0;
				mEOF = false;

				return true;
			}

			// Close file
			void Close()
			{
				if (mFile)
				{
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
				mBufferEnd = 0;
				mEOF = false;
			}

			// Check if file is open
			bool IsOpen() const
			{
				return mFile != nullptr;
			}

			// Check if end of file
			bool IsEOF() const
			{
				return mEOF && mBufferPos >= mBufferEnd;
			}

			// Read raw bytes
			size_t Read(void* data, size_t size)
			{
				DIA_ASSERT(IsOpen(), "File not open");

				size_t totalRead = 0;
				char* dest = static_cast<char*>(data);

				while (size > 0 && !IsEOF())
				{
					// Fill buffer if empty
					if (mBufferPos >= mBufferEnd)
					{
						FillBuffer();
						if (mBufferPos >= mBufferEnd)
						{
							break; // No more data
						}
					}

					// Copy from buffer
					size_t available = mBufferEnd - mBufferPos;
					size_t toCopy = (size < available) ? size : available;

					memcpy(dest, mBuffer + mBufferPos, toCopy);

					dest += toCopy;
					mBufferPos += toCopy;
					totalRead += toCopy;
					size -= toCopy;
				}

				return totalRead;
			}

			// Read a single value
			template <typename T>
			bool Read(T& value)
			{
				return Read(&value, sizeof(T)) == sizeof(T);
			}

			// Read a line (up to newline or buffer size)
			const char* ReadLine(char* buffer, size_t bufferSize)
			{
				DIA_ASSERT(IsOpen(), "File not open");
				DIA_ASSERT(buffer != nullptr, "Buffer is null");
				DIA_ASSERT(bufferSize > 0, "Buffer size is zero");

				size_t pos = 0;

				while (pos < bufferSize - 1 && !IsEOF())
				{
					// Fill buffer if empty
					if (mBufferPos >= mBufferEnd)
					{
						FillBuffer();
						if (mBufferPos >= mBufferEnd)
						{
							break;
						}
					}

					// Read character
					char c = mBuffer[mBufferPos++];

					// Check for newline
					if (c == '\n')
					{
						break;
					}

					// Skip carriage return
					if (c == '\r')
					{
						continue;
					}

					buffer[pos++] = c;
				}

				buffer[pos] = '\0';
				return buffer;
			}

			// Get current position in file
			long long Tell() const
			{
				if (!mFile) return -1;

				long long filePos = ftell(mFile);
				return filePos - (mBufferEnd - mBufferPos);
			}

			// Seek to position
			bool Seek(long long offset, int origin = SEEK_SET)
			{
				if (!mFile) return false;

				// Clear buffer
				mBufferPos = 0;
				mBufferEnd = 0;
				mEOF = false;

				return fseek(mFile, offset, origin) == 0;
			}

			// Get file size
			long long GetSize() const
			{
				if (!mFile) return -1;

				long long currentPos = ftell(mFile);
				fseek(mFile, 0, SEEK_END);
				long long size = ftell(mFile);
				fseek(mFile, currentPos, SEEK_SET);

				return size;
			}

		private:
			void FillBuffer()
			{
				if (!mFile || mEOF) return;

				mBufferEnd = fread(mBuffer, 1, mBufferSize, mFile);
				mBufferPos = 0;

				if (mBufferEnd == 0 || feof(mFile))
				{
					mEOF = true;
				}
			}

			FILE* mFile;
			char* mBuffer;
			size_t mBufferSize;
			size_t mBufferPos;
			size_t mBufferEnd;
			bool mEOF;
		};
	}
}

#endif // DIA_STREAM_READER_H
