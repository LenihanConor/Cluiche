#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/HashTables/HashTableC.h>
#include <DiaCore/Containers/HashTables/HashTableHashFunctionData.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			// Maps asset type IDs to editor plugin type IDs.
			// When open_asset is requested, this is checked first; if no editor is registered
			// the asset is opened via ShellExecuteExW.
			class AssetTypeEditorRegistry
			{
			public:
				AssetTypeEditorRegistry();

				// Register a type editor. Returns false if typeId is already registered.
				bool RegisterTypeEditor(const Dia::Core::StringCRC& assetTypeId,
				                        const Dia::Core::StringCRC& editorPluginTypeId);

				// Find the editor plugin type for a given asset type. Returns default StringCRC if not found.
				Dia::Core::StringCRC FindEditorForType(const Dia::Core::StringCRC& assetTypeId) const;

			private:
				class HashFunctor
				{
				public:
					typedef Dia::Core::StringCRC Key;
					typedef Dia::Core::Containers::HashTableHashFunctionData TableData;
					unsigned int GetHashIndex(Key key, const TableData* tableData) const;
				};

				static const unsigned int kMaxTypes     = 32;
				static const unsigned int kMaxTableSize = 48;

				typedef Dia::Core::Containers::HashTableC<
					Dia::Core::StringCRC,
					Dia::Core::StringCRC,
					HashFunctor,
					kMaxTypes,
					kMaxTableSize> TypeEditorMap;

				TypeEditorMap mMap;
				unsigned int  mCount;
			};

		} // namespace Editor
	} // namespace AssetCatalogue
} // namespace Dia
