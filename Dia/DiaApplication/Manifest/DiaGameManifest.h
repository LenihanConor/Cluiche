#pragma once

#include <DiaApplication/Manifest/TypedImport.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String256.h>

namespace Dia
{
	namespace Application
	{
		struct DiaGameConfig
		{
			Dia::Core::Containers::String256 assetRoot;
		};

		struct DiaGameManifest
		{
			Dia::Core::Containers::String256 name;
			Dia::Core::Containers::String256 version;
			Dia::Core::Containers::DynamicArrayC<TypedImport, 16> imports;
			DiaGameConfig config;
		};

		struct DiaStageManifest
		{
			Dia::Core::Containers::String256 name;
			Dia::Core::Containers::String256 manifestPath;
		};
	}
}
