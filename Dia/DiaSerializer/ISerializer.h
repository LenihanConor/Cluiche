#pragma once

namespace Dia
{
	namespace Serializer
	{
		class ISerializer
		{
		public:
			virtual ~ISerializer() = default;

			virtual const char* GetVersion() const = 0;

			virtual bool CanMigrate(const char* /*fromVersion*/) const { return false; }

		protected:
			static bool ReadFileToBuffer(const char* path, char* outBuffer, unsigned int bufferSize);
			static bool WriteBufferToFile(const char* path, const char* data, unsigned int dataSize);
		};
	}
}
