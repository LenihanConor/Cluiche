#pragma once

namespace Dia
{
	namespace AssetCatalogue
	{
		class AssetTypeRegistry;

		//---------------------------------------------------------------------------------------------------------
		// Built-in taxonomy type stubs
		//
		// These structs define the minimal structural schema for the 8 settled asset taxonomy types.
		// serialize() free functions are defined in BuiltInAssetTypes.cpp via DIA_SERIALIZE macros.
		//
		// Field definitions here are intentional stubs — downstream pipeline/loader specs may extend them.
		// FolderAsset, TextureAsset, and AudioAsset have no JSON schema (directory/binary); their
		// AssetTypeDescriptors will carry a nullptr mDeserializeFn.
		//---------------------------------------------------------------------------------------------------------

		// Binary file — no JSON schema.
		struct TextureAsset
		{
		};

		// References a source texture by name.
		struct SpriteAsset
		{
			char mSourceTexture[64];

			SpriteAsset() { mSourceTexture[0] = '\0'; }
		};

		// Binary file — no JSON schema.
		struct AudioAsset
		{
		};

		// Open-ended JSON config — no fixed structural fields at this level.
		// Note: mVersion is a placeholder stub field; real config fields are defined by downstream specs.
		struct ConfigAsset
		{
			char mVersion[8];

			ConfigAsset() { mVersion[0] = '\0'; }
		};

		// Component composition string (simplified stub — full schema in downstream specs).
		struct EntityAsset
		{
			char mComponents[512];

			EntityAsset() { mComponents[0] = '\0'; }
		};

		// Stage identity fields. Membership via `contains` relationships (SD-CAT-012), not fields.
		struct StageAsset
		{
			char mName[64];
			char mDisplayName[64];

			StageAsset() { mName[0] = '\0'; mDisplayName[0] = '\0'; }
		};

		// UI layout reference.
		struct UIAsset
		{
			char mLayout[64];

			UIAsset() { mLayout[0] = '\0'; }
		};

		// Directory — no JSON schema.
		struct FolderAsset
		{
		};

		//---------------------------------------------------------------------------------------------------------
		// RegisterBuiltInAssetTypes
		//
		// Registers all 8 built-in taxonomy descriptors into the given registry.
		// Call once at startup before any asset-type lookups.
		//---------------------------------------------------------------------------------------------------------
		void RegisterBuiltInAssetTypes(AssetTypeRegistry& registry);

	} // namespace AssetCatalogue
} // namespace Dia
