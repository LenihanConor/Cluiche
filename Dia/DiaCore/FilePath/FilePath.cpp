#include "DiaCore/FilePath/FilePath.h"

#include "DiaCore/FilePath/PathStore.h"

namespace Dia
{	
	namespace Core
	{
		FilePath::FilePath()
		{}

		FilePath::FilePath(const Path::Alias & pathAlias, const FileName & filename)
			: mPathAlias(pathAlias)
			, mFileName(filename)
		{
			DIA_ASSERT(PathStore::IsPathAliasRegistered(pathAlias), "%s is not a register alias", pathAlias.AsChar());
		}

		FilePath::FilePath(const Path::Alias & pathAlias, const PathAmendment & pathAmendment, const FileName & filename)
			: mPathAlias(pathAlias)
			, mPathAmendment(pathAmendment)
			, mFileName(filename)
		{
			DIA_ASSERT(PathStore::IsPathAliasRegistered(pathAlias), "%s is not a register alias", pathAlias.AsChar());
		}

		void FilePath::Create(const Path::Alias& pathAlias, const FileName& filename)
		{
			mPathAlias = pathAlias;
			mFileName = filename;
		
			DIA_ASSERT(PathStore::IsPathAliasRegistered(pathAlias), "%s is not a register alias", pathAlias.AsChar());
		}

		void FilePath::Create(const Path::Alias& pathAlias, const PathAmendment& pathAmendment, const FileName& filename)
		{
			mPathAlias = pathAlias;
			mPathAmendment = pathAmendment;
			mFileName = filename;
			
			DIA_ASSERT(PathStore::IsPathAliasRegistered(pathAlias), "%s is not a register alias", pathAlias.AsChar());
		}

		const FilePath::ResoledFilePath& FilePath::Resolve(ResoledFilePath& bufferToResovleInto) const
		{
			DIA_ASSERT(mPathAlias.Value() != 0, "Path alias has not been set");
			DIA_ASSERT(mFileName.Length() > 0, "File name has not been set");

			bufferToResovleInto.Append(PathStore::ResolvePathToString(mPathAlias).AsCStr());

			{
				char last = bufferToResovleInto[bufferToResovleInto.Length() - 1];
				if (last != '/')
				{
					bufferToResovleInto.Append("/");
				}
			}

			if (mPathAmendment.Length() > 0)
			{
				bufferToResovleInto.Append(mPathAmendment.AsCStr());
			}
			
			{
				char last = bufferToResovleInto[bufferToResovleInto.Length() - 1];
				if (last != '/')
				{
					bufferToResovleInto.Append("/");
				}
			}

			bufferToResovleInto.Append(mFileName.AsCStr());
		
			return bufferToResovleInto;
		}

		FilePath::FileType FilePath::ResolveFileType() const
		{
			DIA_ASSERT(mFileName.Length() > 0, "File name has not been set");
			
			return FilePath::FileType(mFileName.RightSubString(mFileName.Find('.') + 1));
		}

		FilePath::FileName FilePath::ResolveFileNameWithoutFileType() const
		{
			DIA_ASSERT(mFileName.Length() > 0, "Filename has not been set");

			return FilePath::FileName(mFileName.LeftSubString(mFileName.Find('.')));
		}
	}
}