#include "DiaCore/FilePath/Path.h"

namespace Dia
{	
	namespace Core
	{
		Path::Path()
		{}

		Path::Path(const Alias& alias, const String& path)
			: mAlias(alias)
			, mPath(path)
		{}

		const Path::Alias Path::GetAlias()const { return mAlias; }
		const Path::String Path::GetPath()const { return mPath; }
	}
}