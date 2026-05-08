#pragma once

#include "DiaCore/Type/TypeDeclarationMacros.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		class AssetTypeRegistry;

		//---------------------------------------------------------------------------------------------------------
		// Built-in taxonomy type stubs
		//
		// These structs define the minimal structural schema for the 8 settled asset taxonomy types.
		// They are registered with DiaCore's TypeSystem so that AssetTypeDescriptor::mTypeDefinition
		// can point to them.
		//
		// Field definitions here are intentional stubs — downstream pipeline/loader specs may extend them.
		// FolderAsset, TextureAsset, and AudioAsset have no JSON schema (directory/binary); their
		// AssetTypeDescriptors will carry a nullptr TypeDefinition.
		//---------------------------------------------------------------------------------------------------------

		// Binary file — no JSON schema.
		struct TextureAsset
		{
			DIA_TYPE_DECLARATION;
		};

		// References a source texture by name.
		struct SpriteAsset
		{
			DIA_TYPE_DECLARATION;
			char mSourceTexture[64];

			SpriteAsset() { mSourceTexture[0] = '\0'; }
		};

		// Binary file — no JSON schema.
		struct AudioAsset
		{
			DIA_TYPE_DECLARATION;
		};

		// Open-ended JSON config — no fixed structural fields at this level.
		// Note: mVersion is a placeholder stub field; real config fields are defined by downstream specs.
		struct ConfigAsset
		{
			DIA_TYPE_DECLARATION;
			char mVersion[8];

			ConfigAsset() { mVersion[0] = '\0'; }
		};

		// Component composition string (simplified stub — full schema in downstream specs).
		struct EntityAsset
		{
			DIA_TYPE_DECLARATION;
			char mComponents[512];

			EntityAsset() { mComponents[0] = '\0'; }
		};

		// Stage identity fields. Membership via `contains` relationships (SD-CAT-012), not fields.
		struct StageAsset
		{
			DIA_TYPE_DECLARATION;
			char mName[64];
			char mDisplayName[64];

			StageAsset() { mName[0] = '\0'; mDisplayName[0] = '\0'; }
		};

		// UI layout reference.
		struct UIAsset
		{
			DIA_TYPE_DECLARATION;
			char mLayout[64];

			UIAsset() { mLayout[0] = '\0'; }
		};

		// Directory — no JSON schema.
		struct FolderAsset
		{
			DIA_TYPE_DECLARATION;
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
