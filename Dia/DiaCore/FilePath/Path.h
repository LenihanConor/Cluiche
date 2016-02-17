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

			Path();
			Path(const Alias& alias, const String& path);

			const Alias GetAlias()const;
			const String GetPath()const;

		private:
			Alias mAlias;
			String mPath;
		}; 
	}
}