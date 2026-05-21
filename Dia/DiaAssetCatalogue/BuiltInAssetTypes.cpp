#include "DiaAssetCatalogue/BuiltInAssetTypes.h"
#include "DiaAssetCatalogue/AssetTypeRegistry.h"
#include "DiaAssetCatalogue/AssetTypeDescriptor.h"

#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaCore/Reflect/JsonArchive.h"
#include "DiaCore/CRC/StringCRC.h"

//---------------------------------------------------------------------------------------------------------
// DIA_SERIALIZE blocks for built-in asset types
//
// These must be at file scope (outside any namespace) or inside the same namespace as the type.
// Placed inside Dia::AssetCatalogue so that ADL finds serialize() when called on these types.
//---------------------------------------------------------------------------------------------------------

namespace Dia
{
	namespace AssetCatalogue
	{
		// TextureAsset — binary, no JSON fields (empty serialize)
		DIA_SERIALIZE(TextureAsset, 1)
		DIA_SERIALIZE_END

		// SpriteAsset — references a source texture
		DIA_SERIALIZE(SpriteAsset, 1)
			DIA_FIELD(mSourceTexture)
		DIA_SERIALIZE_END

		// AudioAsset — binary, no JSON fields (empty serialize)
		DIA_SERIALIZE(AudioAsset, 1)
		DIA_SERIALIZE_END

		// ConfigAsset — open-ended JSON, placeholder version field
		DIA_SERIALIZE(ConfigAsset, 1)
			DIA_FIELD(mVersion)
		DIA_SERIALIZE_END

		// EntityAsset — component composition string
		DIA_SERIALIZE(EntityAsset, 1)
			DIA_FIELD(mComponents)
		DIA_SERIALIZE_END

		// StageAsset — name and display name
		DIA_SERIALIZE(StageAsset, 1)
			DIA_FIELD(mName)
			DIA_FIELD(mDisplayName)
		DIA_SERIALIZE_END

		// UIAsset — layout reference
		DIA_SERIALIZE(UIAsset, 1)
			DIA_FIELD(mLayout)
		DIA_SERIALIZE_END

		// FolderAsset — directory, no JSON fields (empty serialize)
		DIA_SERIALIZE(FolderAsset, 1)
		DIA_SERIALIZE_END

		//---------------------------------------------------------------------------------------------------------
		// RegisterBuiltInAssetTypes
		//---------------------------------------------------------------------------------------------------------
		void RegisterBuiltInAssetTypes(AssetTypeRegistry& registry)
		{
			// --- Texture ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId         = Dia::Core::StringCRC("texture");
				desc.mName           = Dia::Core::Containers::String64("Texture");
				desc.mFilePattern    = Dia::Core::Containers::String64("*.texture.png");
				desc.mDeserializeFn  = nullptr; // binary — no JSON schema
				registry.Register(desc);
			}

			// --- Sprite ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId         = Dia::Core::StringCRC("sprite");
				desc.mName           = Dia::Core::Containers::String64("Mesh/Sprite");
				desc.mFilePattern    = Dia::Core::Containers::String64("*.sprite.json");
				desc.mDeserializeFn  = [](const Json::Value& root, void* obj) {
					Dia::Reflect::JsonReadArchive ar(root);
					serialize(ar, *static_cast<SpriteAsset*>(obj), 1u);
				};
				registry.Register(desc);
			}

			// --- Audio ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId         = Dia::Core::StringCRC("audio");
				desc.mName           = Dia::Core::Containers::String64("Audio");
				desc.mFilePattern    = Dia::Core::Containers::String64("*.audio.wav");
				desc.mDeserializeFn  = nullptr; // binary — no JSON schema
				registry.Register(desc);
			}

			// --- Config ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId         = Dia::Core::StringCRC("config");
				desc.mName           = Dia::Core::Containers::String64("Config");
				desc.mFilePattern    = Dia::Core::Containers::String64("*.config.json");
				desc.mDeserializeFn  = [](const Json::Value& root, void* obj) {
					Dia::Reflect::JsonReadArchive ar(root);
					serialize(ar, *static_cast<ConfigAsset*>(obj), 1u);
				};
				registry.Register(desc);
			}

			// --- Entity ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId         = Dia::Core::StringCRC("entity");
				desc.mName           = Dia::Core::Containers::String64("Entity Definition");
				desc.mFilePattern    = Dia::Core::Containers::String64("*.entity.json");
				desc.mDeserializeFn  = [](const Json::Value& root, void* obj) {
					Dia::Reflect::JsonReadArchive ar(root);
					serialize(ar, *static_cast<EntityAsset*>(obj), 1u);
				};
				registry.Register(desc);
			}

			// --- Stage ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId         = Dia::Core::StringCRC("stage");
				desc.mName           = Dia::Core::Containers::String64("Stage");
				desc.mFilePattern    = Dia::Core::Containers::String64("*.stage.json");
				desc.mDeserializeFn  = [](const Json::Value& root, void* obj) {
					Dia::Reflect::JsonReadArchive ar(root);
					serialize(ar, *static_cast<StageAsset*>(obj), 1u);
				};
				registry.Register(desc);
			}

			// --- UI ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId         = Dia::Core::StringCRC("ui");
				desc.mName           = Dia::Core::Containers::String64("UI Definition");
				desc.mFilePattern    = Dia::Core::Containers::String64("*.ui.json");
				desc.mDeserializeFn  = [](const Json::Value& root, void* obj) {
					Dia::Reflect::JsonReadArchive ar(root);
					serialize(ar, *static_cast<UIAsset*>(obj), 1u);
				};
				registry.Register(desc);
			}

			// --- Folder ---
			{
				AssetTypeDescriptor desc;
				desc.mTypeId         = Dia::Core::StringCRC("folder");
				desc.mName           = Dia::Core::Containers::String64("Folder");
				desc.mFilePattern    = Dia::Core::Containers::String64("*.folder");
				desc.mDeserializeFn  = nullptr; // directory — no JSON schema
				registry.Register(desc);
			}
		}

	} // namespace AssetCatalogue
} // namespace Dia
