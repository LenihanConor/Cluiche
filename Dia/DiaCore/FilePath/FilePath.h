#pragma once

#include "DiaCore/Strings/String512.h"
#include "DiaCore/Strings/String256.h"
#include "DiaCore/Strings/String32.h"
#include "DiaCore/Strings/String8.h"

#include "DiaCore/FilePath/Path.h"

namespace Dia
{
	namespace Core
	{
		// Used to pass into objects so that can be resoled later 
		class FilePath
		{
		public:	
			typedef Containers::String32 PathAmendment;
			typedef Containers::String32 FileName;
			typedef Containers::String8 FileType;	
			typedef Containers::String512 ResoledFilePath;

			FilePath();

			FilePath(const Path::Alias& pathAlias, const FileName& filename);
			FilePath(const Path::Alias& pathAlias, const PathAmendment& pathAmendment, const FileName& filename);
			
			void Create(const Path::Alias& pathAlias, const FileName& filename);
			void Create(const Path::Alias& pathAlias, const PathAmendment& pathAmendment, const FileName& filename);

			const ResoledFilePath& Resolve(ResoledFilePath& bufferToResovleInto)const;
			
			FilePath::FileType ResolveFileType()const;
			FilePath::FileName ResolveFileNameWithoutFileType()const;

			const Path::Alias& GetPathAlias()const { return mPathAlias; }
			const FilePath::PathAmendment& GetPathAmendment()const { return mPathAmendment; }
			const FilePath::FileName& GetFileName()const { return mFileName; }
			
		private:
			Path::Alias mPathAlias;
			PathAmendment mPathAmendment;
			FileName mFileName;
		}; 
	}
}