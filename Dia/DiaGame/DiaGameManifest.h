#pragma once

#include <DiaApplicationFlow/Manifest/TypedImport.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String256.h>

namespace Json { class Value; }

namespace Dia
{
	namespace Game
	{
		struct DiaGameConfig
		{
			Dia::Core::Containers::String256 assetRoot;
		};

		struct DiaGameManifest
		{
			Dia::Core::Containers::String256 name;
			Dia::Core::Containers::String256 version;
			Dia::Core::Containers::DynamicArrayC<Dia::Application::TypedImport, 16> imports;
			DiaGameConfig config;
			Json::Value* rawConfig;

			DiaGameManifest();
			~DiaGameManifest();

			DiaGameManifest(const DiaGameManifest&) = delete;
			DiaGameManifest& operator=(const DiaGameManifest&) = delete;
			DiaGameManifest(DiaGameManifest&& other) noexcept;
			DiaGameManifest& operator=(DiaGameManifest&& other) noexcept;
		};

		struct DiaStageManifest
		{
			Dia::Core::Containers::String256 name;
			Dia::Core::Containers::String256 manifestPath;
		};
	}
}
