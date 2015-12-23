#include "DiaCore/FilePath/FilePath.h"

#include "DiaCore/FilePath/FilePathStore.h"


namespace Dia
{	
	namespace Core
	{
		FilePath::FilePath()
		{}

		FilePath::FilePath(const PathAlias & pathAlias, const FileName & filename)
			: mPathAlias(pathAlias)
			, mFileName(filename)
		{
			DIA_ASSERT(FilePathStore::IsPathAliasRegistered(pathAlias), "%s is not a register alias", pathAlias.AsCStr());
		}

		FilePath::FilePath(const PathAlias & pathAlias, const PathAmendment & pathAmendment, const FileName & filename)
			: mPathAlias(pathAlias)
			, mPathAmendment(pathAmendment)
			, mFileName(filename)
		{
			DIA_ASSERT(FilePathStore::IsPathAliasRegistered(pathAlias), "%s is not a register alias", pathAlias.AsCStr());
		}

		void FilePath::Create(const PathAlias& pathAlias, const FileName& filename)
		{
			mPathAlias = pathAlias;
			mFileName = filename;
		
			DIA_ASSERT(FilePathStore::IsPathAliasRegistered(pathAlias), "%s is not a register alias", pathAlias.AsCStr());
		}

		void FilePath::Create(const PathAlias& pathAlias, const PathAmendment& pathAmendment, const FileName& filename)
		{
			mPathAlias = pathAlias;
			mPathAmendment = pathAmendment;
			mFileName = filename;
			
			DIA_ASSERT(FilePathStore::IsPathAliasRegistered(pathAlias), "%s is not a register alias", pathAlias.AsCStr());
		}

		const FilePath::ResoledFilePath& FilePath::Resolve(ResoledFilePath& bufferToResovleInto) const
		{
			DIA_ASSERT(mPathAlias.Length() > 0, "Path alias has not been set");
			DIA_ASSERT(mFileName.Length() > 0, "File name has not been set");

			bufferToResovleInto.Append(FilePathStore::ResolvePathToString(mPathAlias).AsCStr());
			if (mPathAmendment.Length() > 0)
			{
				bufferToResovleInto.Append(mPathAmendment.AsCStr());
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