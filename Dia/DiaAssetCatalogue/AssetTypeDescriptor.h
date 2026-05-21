#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String64.h"
#include "DiaCore/Json/external/json/json.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		//---------------------------------------------------------------------------------------------------------
		// AssetTypeDescriptor
		//
		// Pure-metadata descriptor for a single registered asset type.
		// Describes identity (StringCRC type ID), human-readable name, file-extension pattern,
		// and an optional deserialize function pointer that reads a Json::Value into a raw void* object.
		//
		// FolderAsset, TextureAsset, and AudioAsset have mDeserializeFn == nullptr (directory/binary — no JSON schema).
		//
		// DeserializeFn contract: the void* must point to an object of the correct concrete type.
		// The caller is responsible for allocation and type safety.
		//---------------------------------------------------------------------------------------------------------
		using DeserializeFn = void(*)(const Json::Value& root, void* obj);

		struct AssetTypeDescriptor
		{
			Dia::Core::StringCRC         mTypeId;          // e.g. "texture", "config", "entity"
			Dia::Core::Containers::String64 mName;         // e.g. "Texture", "Config", "Entity Definition"
			Dia::Core::Containers::String64 mFilePattern;  // e.g. "*.config.json"
			DeserializeFn                mDeserializeFn;   // DiaReflect-based deserializer; nullptr for binary/directory types

			AssetTypeDescriptor()
				: mTypeId()
				, mName()
				, mFilePattern()
				, mDeserializeFn(nullptr)
			{}
		};

	} // namespace AssetCatalogue
} // namespace Dia
