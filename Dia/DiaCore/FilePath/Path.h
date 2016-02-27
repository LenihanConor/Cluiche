#pragma once

#include "DiaCore/Strings/String256.h"
#include "DiaCore/CRC/StringCRC.h"

namespace Dia
{
	namespace Core
	{
		class Path
		{
		public:
			typedef StringCRC Alias;
			typedef Containers::String256 String;

			static void AppendStrings(const String& str1, const String& str2, String& outString);
			static void CleanPathString(String& outString);

			Path();
			Path(const Alias& alias, const String& path);

			const Alias GetAlias()const;
			const String GetPath()const;

		private:
			static void ReplaceAllBackSlashWithForwardSlash(Path::String& outString);
			static void RemoveEndingForwardSlash(Path::String& outString);

			Alias mAlias;
			String mPath;
		}; 
	}
}