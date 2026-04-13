#pragma once

#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

#include "DiaCore/FilePath/Path.h"
#include "DiaCore/Strings/String32.h"
#include "DiaCore/Type/TypeDeclarationMacros.h"

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// AliasPathConfigTuple
		//
		// Represents a direct alias-to-path mapping for PathStore configuration.
		//
		// USAGE (via JSON):
		//   {
		//     "mAlias": "data",
		//     "mPath": "C:/Game/Data"
		//   }
		//---------------------------------------------------------------------------------------------------------------------------------
		class AliasPathConfigTuple
		{
		public:
			DIA_TYPE_DECLARATION;

			AliasPathConfigTuple() {}

			const Containers::String32& GetAlias()const { return mAlias; }
			const Path::String& GetPath()const { return mPath; }

		private:
			Containers::String32 mAlias;   // The path alias identifier
			Path::String mPath;            // The actual file system path
		};

		//---------------------------------------------------------------------------------------------------------------------------------
		// AliasAppendPathConfig
		//
		// Represents a compound alias built from an existing alias plus a sub-path.
		//
		// USAGE (via JSON):
		//   {
		//     "mAlias": "data_textures",
		//     "mBaseAlias": "data",
		//     "mPathAppend": "Textures"
		//   }
		//   Result: "data_textures" = resolve("data") + "/Textures"
		//---------------------------------------------------------------------------------------------------------------------------------
		class AliasAppendPathConfig
		{
		public:
			DIA_TYPE_DECLARATION;

			AliasAppendPathConfig() {}

			const Containers::String32& GetAlias()const { return mAlias; }
			const Containers::String32& GetBaseAlias()const { return mBaseAlias; }
			const Path::String& GetPathAppend()const { return mPathAppend; }

		private:
			Containers::String32 mAlias;        // The new alias to create
			Containers::String32 mBaseAlias;    // Existing alias to build from
			Path::String mPathAppend;           // Sub-path to append
		};

		//---------------------------------------------------------------------------------------------------------------------------------
		// PathStoreConfigFragment
		//
		// Points to an external PathStoreConfig JSON file to load and merge.
		// Allows splitting large path configurations across multiple files.
		//
		// USAGE (via JSON):
		//   {
		//     "mBaseAlias": "config",
		//     "mPathAppend": "",
		//     "mFileName": "paths_additional.json"
		//   }
		//---------------------------------------------------------------------------------------------------------------------------------
		class PathStoreConfigFragment
		{
		public:
			DIA_TYPE_DECLARATION;

			PathStoreConfigFragment() {}

			const Containers::String32& GetBaseAlias()const { return mBaseAlias; }
			const Containers::String32& GetFileName()const { return mFileName; }
			const Path::String& GetPathAppend()const { return mPathAppend; }

		private:
			Containers::String32 mBaseAlias;  // Base alias to resolve fragment location
			Containers::String32 mFileName;   // Config file name (JSON)
			Path::String mPathAppend;         // Optional subdirectory
		};

		//---------------------------------------------------------------------------------------------------------------------------------
		// PathStoreConfig
		//
		// Container for path store configuration data, typically loaded from JSON.
		//
		// Holds three types of configuration entries:
		//   1. AliasPathTupleArray - Direct alias->path mappings
		//   2. AliasAppendPathArray - Compound aliases built from other aliases
		//   3. PathStoreConfigFragmentArray - External config files to load
		//
		// SERIALIZATION:
		//   Configured via DIA_TYPE_DEFINITION macros for JSON deserialization.
		//   See PathStoreConfig.cpp for type definition.
		//
		// USAGE:
		//   PathStoreConfig config;
		//   SerializedFileLoad loader;
		//   loader.LoadNow("paths.json", config, 4096);
		//   PathStore::RegisterToStore(config);
		//---------------------------------------------------------------------------------------------------------------------------------
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