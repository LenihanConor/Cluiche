#include "DiaAssetCatalogueEditor/Handlers/AssetTypeEditorRegistry.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/CRC/CRC.h>
#include <math.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			unsigned int AssetTypeEditorRegistry::HashFunctor::GetHashIndex(Key key, const TableData* tableData) const
			{
				DIA_ASSERT(key.Value() != 0, "Cannot hash a zero CRC key");

				static const unsigned int sTranslationToTableSpace =
					Dia::Core::CRC::MaxCRC() / AssetTypeEditorRegistry::kMaxTableSize;
				unsigned int index = static_cast<unsigned int>(
					floorf(static_cast<float>(key.Value()) / static_cast<float>(sTranslationToTableSpace)));
				return index;
			}

			AssetTypeEditorRegistry::AssetTypeEditorRegistry()
				: mMap()
				, mCount(0)
			{}

			bool AssetTypeEditorRegistry::RegisterTypeEditor(
				const Dia::Core::StringCRC& assetTypeId,
				const Dia::Core::StringCRC& editorPluginTypeId)
			{
				if (mMap.ContainsKey(assetTypeId))
					return false;
				mMap.Add(assetTypeId, editorPluginTypeId);
				++mCount;
				return true;
			}

			Dia::Core::StringCRC AssetTypeEditorRegistry::FindEditorForType(
				const Dia::Core::StringCRC& assetTypeId) const
			{
				const Dia::Core::StringCRC* found = mMap.TryGetItemConst(assetTypeId);
				return found ? *found : Dia::Core::StringCRC();
			}

		} // namespace Editor
	} // namespace AssetCatalogue
} // namespace Dia
