#include "DiaCore/FilePath/Path.h"

namespace Dia
{	
	namespace Core
	{
		void Path::AppendStrings(const Path::String& str1, const Path::String& str2, Path::String& outString)
		{
			Path::String temp1(str1);
			Path::String temp2(str2);

			// Format the string correctly to be forward slashs
			ReplaceAllBackSlashWithForwardSlash(temp1);
			ReplaceAllBackSlashWithForwardSlash(temp2);
			
			outString.Append(temp1);

			// If the first str does not end with '/' then add them
			Path::String::ConstReverseIterator iter(&outString.Back(), &outString.Front(), &outString.Back());
			char lastChar = *iter.Begin();
			if (lastChar != '/')
			{
				outString.Append("/");
			}
			
			outString.Append(temp2);
		}

		void Path::CleanPathString(String& outString)
		{
			ReplaceAllBackSlashWithForwardSlash(outString);
			RemoveEndingForwardSlash(outString);
		}

		Path::Path()
		{}

		Path::Path(const Alias& alias, const String& path)
			: mAlias(alias)
			, mPath(path)
		{
			CleanPathString(mPath);
		}

		const Path::Alias Path::GetAlias()const { return mAlias; }
		const Path::String Path::GetPath()const { return mPath; }

		void Path::ReplaceAllBackSlashWithForwardSlash(Path::String& outString)
		{
			// Convert all "\\" into "/"
			while (1)
			{
				int pos = outString.Find('\\');

				if (pos == -1)
				{
					break;
				}

				outString[pos] = '/';
			}
		}

		void Path::RemoveEndingForwardSlash(Path::String& outString)
		{
			// Remove all '/' at the end of the string
			Path::String::ConstReverseIterator iter(&outString.Back(), &outString.Front(), &outString.Back());
			char lastChar = *iter.Begin();
			if (lastChar == '/')
			{
				outString.Trim(outString.Size() - 1);
			}
		}
	}
}