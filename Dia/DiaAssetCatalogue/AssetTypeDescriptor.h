#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String64.h"
#include "DiaCore/Type/TypeDefinition.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		//---------------------------------------------------------------------------------------------------------
		// AssetTypeDescriptor
		//
		// Pure-metadata descriptor for a single registered asset type.
		// Describes identity (StringCRC type ID), human-readable name, file-extension pattern,
		// and an optional TypeDefinition pointer for schema reflection.
		//
		// FolderAsset, TextureAsset, and AudioAsset have mTypeDefinition == nullptr (directory/binary — no JSON schema).
		//---------------------------------------------------------------------------------------------------------
		struct AssetTypeDescriptor
		{
			Dia::Core::StringCRC                         mTypeId;          // e.g. "texture", "config", "entity"
			Dia::Core::Containers::String64              mName;            // e.g. "Texture", "Config", "Entity Definition"
			Dia::Core::Containers::String64              mFilePattern;     // e.g. "*.config.json"
			const Dia::Core::Types::TypeDefinition*      mTypeDefinition;  // schema via TypeSystem reflection; nullptr for binary/directory types

			AssetTypeDescriptor()
				: mTypeId()
				, mName()
				, mFilePattern()
				, mTypeDefinition(nullptr)
			{}
		};

	} // namespace AssetCatalogue
} // namespace Dia
