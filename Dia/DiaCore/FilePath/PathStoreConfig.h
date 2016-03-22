#pragma once

#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

#include "DiaCore/FilePath/Path.h"
#include "DiaCore/Strings/String32.h"
#include "DiaCore/Type/TypeDeclarationMacros.h"

namespace Dia
{
	namespace Core
	{
		// This class is input to the PathStore of alias's and there orresponding path
		class AliasPathConfigTuple
		{
		public:
			DIA_TYPE_DECLARATION;

			AliasPathConfigTuple() {}

			const Containers::String32& GetAlias()const { return mAlias; }
			const Path::String& GetPath()const { return mPath; }

		private:
			Containers::String32 mAlias;
			Path::String mPath;
		};

		// This class is input to the PathStore that will create a new alias that is an appendment to another.
		class AliasAppendPathConfig
		{
		public:
			DIA_TYPE_DECLARATION;

			AliasAppendPathConfig() {}

			const Containers::String32& GetAlias()const { return mAlias; }
			const Containers::String32& GetBaseAlias()const { return mBaseAlias; }
			const Path::String& GetPathAppend()const { return mPathAppend; }

		private:
			Containers::String32 mAlias;
			Containers::String32 mBaseAlias;
			Path::String mPathAppend;
		};

		// This class is to point the system at other path store files to open/parse and add there contents to the path store
		class PathStoreConfigFragment
		{
		public:
			DIA_TYPE_DECLARATION;

			PathStoreConfigFragment() {}

			const Containers::String32& GetBaseAlias()const { return mBaseAlias; }
			const Containers::String32& GetFileName()const { return mFileName; }
			const Path::String& GetPathAppend()const { return mPathAppend; }

		private:
			Containers::String32 mBaseAlias;
			Containers::String32 mFileName;
			Path::String mPathAppend;
		};

		// This class is input to the PathStore, to register paths from the data
		class PathStoreConfig
		{
		public:
			DIA_TYPE_DECLARATION;

			static const int kMaxAliasPathConfigTuples = 8;
			static const int kMaxAliasAppendPathConfig = 8;
			static const int kMaxPathConfigFragment = 8;
			
			typedef Dia::Core::Containers::DynamicArrayC<AliasPathConfigTuple, kMaxAliasPathConfigTuples> AliasPathTupleArray;
			typedef Dia::Core::Containers::DynamicArrayC<AliasAppendPathConfig, kMaxAliasAppendPathConfig> AliasAppendPathArray;
			typedef Dia::Core::Containers::DynamicArrayC<PathStoreConfigFragment, kMaxPathConfigFragment> PathStoreConfigFragmentArray;

			PathStoreConfig() {}

			const AliasPathTupleArray& GetAliasPathTupleArray()const { return mAliasPathTupleArray; }
			const AliasAppendPathArray& GetAliasAppendPathTupleArray()const { return mAliasAppendPathArray; }
			const PathStoreConfigFragmentArray& GetPathStoreConfigFragmentArray()const { return mPathStoreConfigFragmentArray; }

		private:
			AliasPathTupleArray mAliasPathTupleArray;
			AliasAppendPathArray mAliasAppendPathArray;
			PathStoreConfigFragmentArray mPathStoreConfigFragmentArray;
		}; 
	}
}