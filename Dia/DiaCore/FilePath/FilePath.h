#pragma once

#include "DiaCore/Strings/String512.h"
#include "DiaCore/Strings/String256.h"
#include "DiaCore/Strings/String32.h"
#include "DiaCore/Strings/String8.h"

namespace Dia
{
	namespace Core
	{
		// Used to pass into objects so that can be resoled later 
		class FilePath
		{
		public:
			typedef Containers::String32 PathAlias;
			typedef Containers::String256 Path;
			typedef Containers::String32 PathAmendment;
			typedef Containers::String32 FileName;
			typedef Containers::String8 FileType;	
			typedef Containers::String512 ResoledFilePath;

			FilePath();

			FilePath(const PathAlias& pathAlias, const FileName& filename);
			FilePath(const PathAlias& pathAlias, const PathAmendment& pathAmendment, const FileName& filename);
			
			void Create(const PathAlias& pathAlias, const FileName& filename);
			void Create(const PathAlias& pathAlias, const PathAmendment& pathAmendment, const FileName& filename);

			const ResoledFilePath& Resolve(ResoledFilePath& bufferToResovleInto)const;
			
			FilePath::FileType ResolveFileType()const;
			FilePath::FileName ResolveFileNameWithoutFileType()const;

			const PathAlias& GetPathAlias()const { return mPathAlias; }
			const FilePath::PathAmendment& GetPathAmendment()const { return mPathAmendment; }
			const FilePath::FileName& GetFileName()const { return mFileName; }
			
		private:
			PathAlias mPathAlias;
			PathAmendment mPathAmendment;
			FileName mFileName;
		}; 
	}
}