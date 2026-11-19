#pragma once

#include <DiaApplicationFlow/Manifest/TypedImport.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String256.h>
#include <DiaCore/Strings/String128.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String32.h>

namespace Json { class Value; }

namespace Dia
{
	namespace Game
	{
		struct DiaGameConfig
		{
			Dia::Core::Containers::String256 assetRoot;
			Dia::Core::Containers::String256 assetCatalogue;
		};

		struct DiaGameManifest
		{
			Dia::Core::Containers::String256 name;
			Dia::Core::Containers::String256 version;
			Dia::Core::Containers::DynamicArrayC<Dia::Application::TypedImport, 16> imports;
			DiaGameConfig config;
			Json::Value* rawConfig;
			// Editor manifests (relative to the .diagame) whose plugins exist only while this game is loaded.
			Dia::Core::Containers::DynamicArrayC<Dia::Core::Containers::String256, 8> editorManifests;

			DiaGameManifest();
			~DiaGameManifest();

			DiaGameManifest(const DiaGameManifest&) = delete;
			DiaGameManifest& operator=(const DiaGameManifest&) = delete;
			DiaGameManifest(DiaGameManifest&& other) noexcept;
			DiaGameManifest& operator=(DiaGameManifest&& other) noexcept;
		};

		// One module's override within a launch preset. overridesJson is compact JSON text of an object.
		struct DiaStagePresetModule
		{
			static const unsigned int kMaxOverrideBytes = 2048;
			Dia::Core::Containers::String64 instanceId;
			char overridesJson[kMaxOverrideBytes + 1];  // NUL-terminated; at most kMaxOverrideBytes chars

			DiaStagePresetModule() { overridesJson[0] = '\0'; }
		};

		// A named launch preset for a stage (config overrides per module instance).
		struct DiaStagePreset
		{
			static const unsigned int kMaxModules = 4;
			Dia::Core::Containers::String32 name;          // <= 31 chars
			Dia::Core::Containers::String128 description;  // optional; <= 120 chars
			Dia::Core::Containers::DynamicArrayC<DiaStagePresetModule, kMaxModules> modules;
		};

		struct DiaStageManifest
		{
			Dia::Core::Containers::String256 name;
			Dia::Core::Containers::String256 manifestPath;
			Dia::Core::Containers::String256 scenePath;  // optional; empty if absent
			Dia::Core::Containers::DynamicArrayC<Dia::Core::Containers::String64, 16> tags;  // optional; empty if absent
			Dia::Core::Containers::String128 description;                                       // optional; empty if absent
			Dia::Core::Containers::DynamicArrayC<Dia::Core::Containers::String256, 16> proves;  // optional spec paths (tooling metadata, runtime ignores); empty if absent
			Dia::Core::Containers::DynamicArrayC<DiaStagePreset, 16> presets;  // optional launch presets; empty if absent
		};
	}
}
