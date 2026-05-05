#include "DiaAssetCatalogue/BuiltInAssetTypes.h"
#include "DiaAssetCatalogue/AssetTypeRegistry.h"
#include "DiaAssetCatalogue/AssetTypeDescriptor.h"

#include "DiaCore/Type/TypeDefinitionMacros.h"
#include "DiaCore/CRC/StringCRC.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		//---------------------------------------------------------------------------------------------------------
		// TypeDefinitions for built-in asset types
		//---------------------------------------------------------------------------------------------------------

		// TextureAsset — binary, no JSON fields
		DIA_TYPE_DEFINITION(TextureAsset)
		DIA_TYPE_DEFINITION_END()

		// SpriteAsset — references a texture
		DIA_TYPE_DEFINITION(SpriteAsset)
			DIA_TYPE_ADD_VARIABLE_ARRAY("mSourceTexture", mSourceTexture, 64)
		DIA_TYPE_DEFINITION_END()

		// AudioAsset — binary, no JSON fields
		DIA_TYPE_DEFINITION(AudioAsset)
		DIA_TYPE_DEFINITION_END()

		// ConfigAsset — open-ended JSON, placeholder field
		DIA_TYPE_DEFINITION(ConfigAsset)
			DIA_TYPE_ADD_VARIABLE_ARRAY("mVersion", mVersion, 8)
		DIA_TYPE_DEFINITION_END()

		// EntityAsset — component composition string
		DIA_TYPE_DEFINITION(EntityAsset)
			DIA_TYPE_ADD_VARIABLE_ARRAY("mComponents", mComponents, 512)
		DIA_TYPE_DEFINITION_END()

		// StageAsset — name and display name
		DIA_TYPE_DEFINITION(StageAsset)
			DIA_TYPE_ADD_VARIABLE_ARRAY("mName", mName, 64)
			DIA_TYPE_ADD_VARIABLE_ARRAY("mDisplayName", mDisplayName, 64)
		DIA_TYPE_DEFINITION_END()

		// UIAsset — layout reference
		DIA_TYPE_DEFINITION(UIAsset)
			DIA_TYPE_ADD_VARIABLE_ARRAY("mLayout", mLayout, 64)
		DIA_TYPE_DEFINITION_END()

		// FolderAsset — directory, no JSON fields
		DIA_TYPE_DEFINITION(FolderAsset)
		DIA_TYPE_DEFINITION_END()

		//---------------------------------------------------------------------------------------------------------
		// RegisterBuiltInAssetTypes
		//---------------------------------------------------------------------------------------------------------
		void RegisterBuiltInAssetTypes(AssetTypeRegistry& registry)
		{
			// --- Texture ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId          = Dia::Core::StringCRC("texture");
				desc.mName            = Dia::Core::Containers::String64("Texture");
				desc.mFilePattern     = Dia::Core::Containers::String64("*.texture.png");
				desc.mTypeDefinition  = nullptr; // binary — no JSON schema
				registry.Register(desc);
			}

			// --- Sprite ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId          = Dia::Core::StringCRC("sprite");
				desc.mName            = Dia::Core::Containers::String64("Mesh/Sprite");
				desc.mFilePattern     = Dia::Core::Containers::String64("*.sprite.json");
				desc.mTypeDefinition  = SpriteAsset::GetTypeStatic();
				registry.Register(desc);
			}

			// --- Audio ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId          = Dia::Core::StringCRC("audio");
				desc.mName            = Dia::Core::Containers::String64("Audio");
				desc.mFilePattern     = Dia::Core::Containers::String64("*.audio.wav");
				desc.mTypeDefinition  = nullptr; // binary — no JSON schema
				registry.Register(desc);
			}

			// --- Config ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId          = Dia::Core::StringCRC("config");
				desc.mName            = Dia::Core::Containers::String64("Config");
				desc.mFilePattern     = Dia::Core::Containers::String64("*.config.json");
				desc.mTypeDefinition  = ConfigAsset::GetTypeStatic();
				registry.Register(desc);
			}

			// --- Entity ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId          = Dia::Core::StringCRC("entity");
				desc.mName            = Dia::Core::Containers::String64("Entity Definition");
				desc.mFilePattern     = Dia::Core::Containers::String64("*.entity.json");
				desc.mTypeDefinition  = EntityAsset::GetTypeStatic();
				registry.Register(desc);
			}

			// --- Stage ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId          = Dia::Core::StringCRC("stage");
				desc.mName            = Dia::Core::Containers::String64("Stage");
				desc.mFilePattern     = Dia::Core::Containers::String64("*.stage.json");
				desc.mTypeDefinition  = StageAsset::GetTypeStatic();
				registry.Register(desc);
			}

			// --- UI ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId          = Dia::Core::StringCRC("ui");
				desc.mName            = Dia::Core::Containers::String64("UI Definition");
				desc.mFilePattern     = Dia::Core::Containers::String64("*.ui.json");
				desc.mTypeDefinition  = UIAsset::GetTypeStatic();
				registry.Register(desc);
			}

			// --- Folder ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId          = Dia::Core::StringCRC("folder");
				desc.mName            = Dia::Core::Containers::String64("Folder");
				desc.mFilePattern     = Dia::Core::Containers::String64("*.folder");
				desc.mTypeDefinition  = nullptr; // directory — no JSON schema
				registry.Register(desc);
			}
		}

	} // namespace AssetCatalogue
} // namespace Dia
