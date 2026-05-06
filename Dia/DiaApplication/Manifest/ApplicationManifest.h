#pragma once

#include <DiaApplication/Manifest/TypedImport.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String256.h>

namespace Json { class Value; }

namespace Dia
{
	namespace Application
	{
		// Intermediate representation of application manifest
		// Loaded from .diaapp JSON files
		struct ApplicationManifest
		{
			// Module entry: describes a module instance
			struct ModuleEntry
			{
				Dia::Core::StringCRC typeId;        // Type of module (e.g., "RenderModule")
				Dia::Core::StringCRC instanceId;    // Unique instance ID
				Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> phaseIds;  // Phases this module belongs to
				Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> dependencies; // Other module instance IDs
				Json::Value* config;                // Configuration (owned by this entry)
				Dia::Core::Containers::String256 sourceManifestPath;  // Which .diaapp file this entry came from

				ModuleEntry();
				ModuleEntry(const ModuleEntry& other);
				ModuleEntry& operator=(const ModuleEntry& other);
				~ModuleEntry();
			};

			// Phase entry: describes a phase instance
			struct PhaseEntry
			{
				Dia::Core::StringCRC typeId;        // Type of phase (e.g., "UpdatePhase")
				Dia::Core::StringCRC instanceId;    // Unique instance ID
				Json::Value* config;                // Configuration (owned by this entry)
				Dia::Core::Containers::String256 sourceManifestPath;  // Which .diaapp file this entry came from

				PhaseEntry();
				PhaseEntry(const PhaseEntry& other);
				PhaseEntry& operator=(const PhaseEntry& other);
				~PhaseEntry();
			};

			// Phase transition: defines allowed phase transitions
			struct PhaseTransition
			{
				Dia::Core::StringCRC fromPhase;     // Source phase instance ID
				Dia::Core::StringCRC toPhase;       // Target phase instance ID
			};

			// ProcessingUnit entry: describes a processing unit instance
			struct ProcessingUnitEntry
			{
				Dia::Core::StringCRC typeId;        // Type of processing unit
				Dia::Core::StringCRC instanceId;    // Unique instance ID
				float frequencyHz;                  // Update frequency (-1 = unlimited)
				bool dedicatedThread;               // Run on dedicated thread?
				bool root;                          // True for the single tree root PU
				Json::Value* config;                // Configuration (owned by this entry)
				Dia::Core::Containers::String256 sourceManifestPath;  // Which .diaapp file this entry came from

				Dia::Core::Containers::DynamicArrayC<PhaseEntry, 16> phases;
				Dia::Core::Containers::DynamicArrayC<PhaseTransition, 32> transitions;
				Dia::Core::StringCRC initialPhase;
				Dia::Core::Containers::DynamicArrayC<ModuleEntry, 32> modules;

				ProcessingUnitEntry();
				ProcessingUnitEntry(const ProcessingUnitEntry& other);
				ProcessingUnitEntry& operator=(const ProcessingUnitEntry& other);
				~ProcessingUnitEntry();
			};

			// Manifest data
			unsigned int version;  // Schema version (e.g., 1)
			Dia::Core::Containers::DynamicArrayC<ProcessingUnitEntry, 4> processingUnits;
			Dia::Core::Containers::DynamicArrayC<TypedImport, 16> imports;
			Json::Value* metadata;  // Editor metadata (optional, owned by manifest loader)

			ApplicationManifest();
			~ApplicationManifest();
		};
	}
}
