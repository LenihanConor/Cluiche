// ===================================================================
// FilePath.cpp
// Implementation of abstract file path resolution system
// ===================================================================

#include "DiaCore/FilePath/FilePath.h"

#include "DiaCore/FilePath/PathStore.h"

namespace Dia
{
	namespace Core
	{
		// Default constructor
		FilePath::FilePath()
		{}

		// Construct with path alias and filename
		// Verifies the alias is registered in PathStore
		FilePath::FilePath(const Path::Alias & pathAlias, const FileName & filename)
			: mPathAlias(pathAlias)
			, mFileName(filename)
		{
			DIA_ASSERT(PathStore::IsPathAliasRegistered(pathAlias), "%s is not a register alias", pathAlias.AsChar());
		}

		// Construct with path alias, subdirectory amendment, and filename
		// Verifies the alias is registered in PathStore
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

		// Resolve the file path to a full absolute path string
		// Combines: [PathStore alias] / [amendment] / [filename]
		// Ensures forward slashes are used as separators
		const FilePath::ResoledFilePath& FilePath::Resolve(ResoledFilePath& bufferToResovleInto) const
		{
			DIA_ASSERT(mPathAlias.Value() != 0, "Path alias has not been set");
			DIA_ASSERT(mFileName.Length() > 0, "File name has not been set");

			// Start with the base path from PathStore
			bufferToResovleInto.Append(PathStore::ResolvePathToString(mPathAlias).AsCStr());

			// Ensure trailing slash after base path
			{
				char last = bufferToResovleInto[bufferToResovleInto.Length() - 1];
				if (last != '/')
				{
					bufferToResovleInto.Append("/");
				}
			}

			// Add path amendment if present
			if (mPathAmendment.Length() > 0)
			{
				bufferToResovleInto.Append(mPathAmendment.AsCStr());
			}

			// Ensure trailing slash before filename
			{
				char last = bufferToResovleInto[bufferToResovleInto.Length() - 1];
				if (last != '/')
				{
					bufferToResovleInto.Append("/");
				}
			}

			// Append the filename
			bufferToResovleInto.Append(mFileName.AsCStr());

			return bufferToResovleInto;
		}

		// Extract the file extension from the filename
		// Example: "texture.png" -> "png"
		FilePath::FileType FilePath::ResolveFileType() const
		{
			DIA_ASSERT(mFileName.Length() > 0, "File name has not been set");

			return FilePath::FileType(mFileName.RightSubString(mFileName.Find('.') + 1));
		}

		// Get the filename without its extension
		// Example: "texture.png" -> "texture"
		FilePath::FileName FilePath::ResolveFileNameWithoutFileType() const
		{
			DIA_ASSERT(mFileName.Length() > 0, "Filename has not been set");

			return FilePath::FileName(mFileName.LeftSubString(mFileName.Find('.')));
		}
	}
}