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

		// This class is input to the PathStore, to register paths from the data
		class PathStoreConfig
		{
		public:
			DIA_TYPE_DECLARATION;

			static const int kMaxAliasPathConfigTuples = 8;
			static const int kMaxAliasAppendPathConfig = 8;
			
			typedef Dia::Core::Containers::DynamicArrayC<AliasPathConfigTuple, kMaxAliasPathConfigTuples> AliasPathTupleArray;
			typedef Dia::Core::Containers::DynamicArrayC<AliasAppendPathConfig, kMaxAliasAppendPathConfig> AliasAppendPathArray;

			PathStoreConfig() {}

			const AliasPathTupleArray& GetAliasPathTupleArray()const { return mAliasPathTupleArray; }
			const AliasAppendPathArray& GetAliasAppendPathTupleArray()const { return mAliasAppendPathArray; }

		private:
			AliasPathTupleArray mAliasPathTupleArray;
			AliasAppendPathArray mAliasAppendPathArray;
		}; 
	}
}