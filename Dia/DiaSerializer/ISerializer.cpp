#include "DiaSerializer/ISerializer.h"
#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>
#include <cstdio>
#include <cstring>

namespace Dia
{
	namespace Serializer
	{
		bool ISerializer::ReadFileToBuffer(const char* path, char* outBuffer, unsigned int bufferSize)
		{
			FILE* f = nullptr;
			fopen_s(&f, path, "rb");
			if (!f)
			{
				DIA_LOG_WARNING("Serializer", "ISerializer::ReadFileToBuffer: failed to open '%s'", path);
				return false;
			}

			fseek(f, 0, SEEK_END);
			long fileSize = ftell(f);
			fseek(f, 0, SEEK_SET);

			if (fileSize < 0 || static_cast<unsigned int>(fileSize) + 1 > bufferSize)
			{
				DIA_LOG_WARNING("Serializer", "ISerializer::ReadFileToBuffer: file '%s' too large for buffer (%ld bytes, %u available)", path, fileSize, bufferSize);
				fclose(f);
				return false;
			}

			size_t bytesRead = fread(outBuffer, 1, static_cast<size_t>(fileSize), f);
			fclose(f);

			if (bytesRead != static_cast<size_t>(fileSize))
			{
				DIA_LOG_WARNING("Serializer", "ISerializer::ReadFileToBuffer: read error on '%s'", path);
				return false;
			}

			outBuffer[fileSize] = '\0';
			return true;
		}

		bool ISerializer::WriteBufferToFile(const char* path, const char* data, unsigned int dataSize)
		{
			FILE* f = nullptr;
			fopen_s(&f, path, "wb");
			if (!f)
			{
				DIA_LOG_WARNING("Serializer", "ISerializer::WriteBufferToFile: failed to open '%s' for writing", path);
				return false;
			}

			size_t written = fwrite(data, 1, dataSize, f);
			fclose(f);

			if (written != dataSize)
			{
				DIA_LOG_WARNING("Serializer", "ISerializer::WriteBufferToFile: write error on '%s'", path);
				return false;
			}

			return true;
		}
	}
}
