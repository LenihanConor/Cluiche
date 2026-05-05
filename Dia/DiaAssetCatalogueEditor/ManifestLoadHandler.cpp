#include "DiaAssetCatalogueEditor/ManifestLoadHandler.h"

#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaAssetCatalogue/CatalogueManifestSerializer.h>
#include <DiaEditor/Command/CommandHistory.h>
#include <DiaLogger/DiaLog.h>

#include <cstring>

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			bool ManifestLoadHandler::Load(
				const char* path,
				Dia::AssetCatalogue::AssetRegistry& registry,
				const Dia::AssetCatalogue::CatalogueManifestSerializer& serializer,
				Dia::Editor::CommandHistory& history,
				char* errorOut, unsigned int errorCapacity)
			{
				Dia::AssetCatalogue::LoadResult<AssetRegistry> result = serializer.LoadManifest(path);
				if (!result.mSuccess)
				{
					if (errorOut && errorCapacity > 0 && result.HasErrors())
						strncpy_s(errorOut, errorCapacity, result.GetFirstError().mMessage.AsCStr(), _TRUNCATE);
					DIA_LOG_ERROR("Editor", "ManifestLoadHandler: load failed");
					return false;
				}

				registry = result.mValue;
				history.Clear();
				history.MarkSavePoint();
				DIA_LOG_INFO("Editor", "ManifestLoadHandler: loaded manifest");
				return true;
			}

			bool ManifestLoadHandler::Save(
				const char* path,
				const Dia::AssetCatalogue::AssetRegistry& registry,
				const Dia::AssetCatalogue::CatalogueManifestSerializer& serializer,
				Dia::Editor::CommandHistory& history,
				char* errorOut, unsigned int errorCapacity)
			{
				if (!serializer.SaveManifest(registry, path))
				{
					if (errorOut && errorCapacity > 0)
						strncpy_s(errorOut, errorCapacity, "Failed to write manifest file", _TRUNCATE);
					DIA_LOG_ERROR("Editor", "ManifestLoadHandler: save failed");
					return false;
				}

				history.MarkSavePoint();
				DIA_LOG_INFO("Editor", "ManifestLoadHandler: saved manifest");
				return true;
			}

			void ManifestLoadHandler::NewManifest(
				Dia::AssetCatalogue::AssetRegistry& registry,
				Dia::Editor::CommandHistory& history)
			{
				registry = AssetRegistry();
				history.Clear();
				history.MarkSavePoint();
				DIA_LOG_INFO("Editor", "ManifestLoadHandler: new manifest");
			}

			bool ManifestLoadHandler::IsDirty(const Dia::Editor::CommandHistory& history) const
			{
				return !history.IsAtSavePoint();
			}
		}
	}
}
