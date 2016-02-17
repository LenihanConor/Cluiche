#pragma once

#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

#include "DiaCore/FilePath/Path.h"
#include "DiaCore/Strings/String256.h"
#include "DiaCore/Type/TypeDeclarationMacros.h"

namespace Dia
{
	namespace Core
	{
		class PathStoreConfigTuple
		{
		public:
			DIA_TYPE_DECLARATION;

			PathStoreConfigTuple() {}

		private:
			Containers::String256 mAlias;
			Path::String mPath;
		};

		class PathStoreConfig
		{
		public:
			DIA_TYPE_DECLARATION;

			static const int kMaxConfigTuples = 2;
			
			typedef Dia::Core::Containers::DynamicArrayC<PathStoreConfigTuple, kMaxConfigTuples> TupleArray;

			PathStoreConfig() {}

		private:
			TupleArray mTupleArray;
		}; 
	}
}