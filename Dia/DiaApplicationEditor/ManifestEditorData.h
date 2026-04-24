#pragma once

#include <DiaApplication/Manifest/ApplicationManifest.h>
#include <DiaApplication/Manifest/ManifestValidator.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Application
	{
		namespace Editor
		{
			struct ValidationResult
			{
				bool isValid;
				Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32> errors;

				ValidationResult() : isValid(false) {}
			};

			struct TypeInfo
			{
				Dia::Core::StringCRC typeId;
				char description[128];

				TypeInfo()
				{
					description[0] = '\0';
				}
			};

			struct TypeCache
			{
				Dia::Core::Containers::DynamicArrayC<TypeInfo, 16> processingUnitTypes;
				Dia::Core::Containers::DynamicArrayC<TypeInfo, 32> phaseTypes;
				Dia::Core::Containers::DynamicArrayC<TypeInfo, 64> moduleTypes;

				bool isFromLiveGame;   // true = queried from connected game, false = static fallback

				TypeCache() : isFromLiveGame(false) {}

				void Clear()
				{
					processingUnitTypes.RemoveAll();
					phaseTypes.RemoveAll();
					moduleTypes.RemoveAll();
					isFromLiveGame = false;
				}
			};

			struct ManifestEditorData
			{
				ApplicationManifest manifest;
				char filePath[512];
				char selectedNodeId[256];
				bool isDirty;
				bool hasManifest;

				ValidationResult validationResult;
				TypeCache typeCache;

				ManifestEditorData()
					: isDirty(false)
					, hasManifest(false)
				{
					filePath[0] = '\0';
					selectedNodeId[0] = '\0';
				}
			};
		}
	}
}
