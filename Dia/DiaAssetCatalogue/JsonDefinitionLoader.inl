#pragma once

#include "DiaAssetCatalogue/JsonDefinitionLoader.h"

#include "DiaCore/Type/TypeDefinition.h"
#include "DiaCore/Type/TypeVariable.h"
#include "DiaCore/Type/TypeVariableAttributes.h"
#include "DiaCore/Type/TypeFacade.h"
#include "DiaCore/CRC/CRC.h"
#include "DiaCore/Json/external/json/json.h"

#include <stdio.h>
#include <string.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		//------------------------------------------------------------------------------------
		template<typename T>
		LoadResult<T> JsonDefinitionLoader::Load(const Dia::Core::FilePath& path) const
		{
			LoadResult<T> result;

			// Guard against an uninitialised FilePath (no alias set)
			if (path.GetPathAlias().Value() == 0 || path.GetFileName().Length() == 0)
			{
				LoadError err;
				err.mKind = LoadErrorKind::FileNotFound;
				err.mFieldPath = Dia::Core::StringCRC("");
				err.mMessage = Dia::Core::Containers::String64("File not found");
				result.mErrors.Add(err);
				return result;
			}

			Dia::Core::FilePath::ResoledFilePath resolvedPath;
			path.Resolve(resolvedPath);
			const char* filePath = resolvedPath.AsCStr();

			FILE* f = nullptr;
#if defined(_MSC_VER)
			fopen_s(&f, filePath, "rb");
#else
			f = fopen(filePath, "rb");
#endif
			if (!f)
			{
				LoadError err;
				err.mKind = LoadErrorKind::FileNotFound;
				err.mFieldPath = Dia::Core::StringCRC("");
				err.mMessage = Dia::Core::Containers::String64("File not found");
				result.mErrors.Add(err);
				return result;
			}

			fseek(f, 0, SEEK_END);
			long fileSize = ftell(f);
			fseek(f, 0, SEEK_SET);

			if (fileSize <= 0)
			{
				fclose(f);
				LoadError err;
				err.mKind = LoadErrorKind::JsonParseError;
				err.mFieldPath = Dia::Core::StringCRC("");
				err.mMessage = Dia::Core::Containers::String64("Empty file");
				result.mErrors.Add(err);
				return result;
			}

			// Stack-allocate up to 64KB; fall back to a simple heap buffer via std
			// Using a fixed-size static buffer is consistent with how DiaCore avoids dynamic alloc.
			static const unsigned int kMaxFileSize = 64 * 1024;
			static char sFileBuffer[kMaxFileSize];

			if (static_cast<unsigned long>(fileSize) >= kMaxFileSize)
			{
				fclose(f);
				LoadError err;
				err.mKind = LoadErrorKind::JsonParseError;
				err.mFieldPath = Dia::Core::StringCRC("");
				err.mMessage = Dia::Core::Containers::String64("File too large");
				result.mErrors.Add(err);
				return result;
			}

			size_t bytesRead = fread(sFileBuffer, 1, static_cast<size_t>(fileSize), f);
			fclose(f);

			sFileBuffer[bytesRead] = '\0';

			Dia::Core::Containers::StringReader bufferReader(sFileBuffer);
			return LoadFromBuffer<T>(bufferReader);
		}

		//------------------------------------------------------------------------------------
		template<typename T>
		LoadResult<T> JsonDefinitionLoader::LoadFromBuffer(const Dia::Core::Containers::StringReader& buffer) const
		{
			LoadResult<T> result;

			// Check type is registered
			Dia::Core::CRC typeCRC(T::GetTypeStatic()->GetUniqueCRC());
			if (!mRegistry.ContainsType(typeCRC))
			{
				LoadError err;
				err.mKind = LoadErrorKind::TypeNotRegistered;
				err.mFieldPath = Dia::Core::StringCRC("");
				err.mMessage = Dia::Core::Containers::String64("Type not registered");
				result.mErrors.Add(err);
				return result;
			}

			const char* jsonText = buffer.AsCStr();

			// Quick parse check before calling TypeJsonSerializer (which asserts on failure)
			{
				Json::Value parsedCheck;
				Json::Reader reader;
				if (!reader.parse(jsonText, parsedCheck, false))
				{
					LoadError err;
					err.mKind = LoadErrorKind::JsonParseError;
					err.mFieldPath = Dia::Core::StringCRC("");
					err.mMessage = Dia::Core::Containers::String64("JSON parse error");
					result.mErrors.Add(err);
					return result;
				}
			}

			// Deserialize using TypeJsonSerializer (requires an initialised serializer with registry)
			Dia::Core::Types::TypeJsonSerializer serializer;
			serializer.Initilize(&mRegistry);

			// We need a non-const StringReader for the Deserialize call
			Dia::Core::Containers::StringReader mutableBuffer(jsonText);
			serializer.Deserialize(result.mValue, mutableBuffer);

			// Validate required fields
			ValidateRequiredFields(result.mValue, jsonText, result);

			if (!result.HasErrors())
			{
				result.mSuccess = true;
			}

			return result;
		}

		//------------------------------------------------------------------------------------
		template<typename T>
		void JsonDefinitionLoader::ValidateRequiredFields(const T& value, const char* jsonText, LoadResult<T>& result) const
		{
			// Parse JSON again to check membership
			Json::Value parsedRoot;
			Json::Reader reader;
			if (!reader.parse(jsonText, parsedRoot, false))
			{
				return; // already failed parse check above
			}

			const Dia::Core::Types::TypeDefinition* typeDefinition = T::GetTypeStatic();
			if (!typeDefinition)
			{
				return;
			}

			const Dia::Core::Types::TypeDefinition::VariableLinkList& variables = typeDefinition->GetVariables();
			const Dia::Core::Types::TypeDefinition::VariableLinkListNode* currentNode = variables.HeadConst();

			while (currentNode != nullptr)
			{
				const Dia::Core::Types::TypeVariable* var = currentNode->GetPayloadConst();
				if (var && var->HasAttribute<Dia::Core::Types::TypeVariableAttributeRequired>())
				{
					const char* fieldName = var->GetName();
					if (!parsedRoot.isMember(fieldName))
					{
						LoadError err;
						err.mKind = LoadErrorKind::MissingRequiredField;
						err.mFieldPath = Dia::Core::StringCRC(fieldName);
						err.mMessage = Dia::Core::Containers::String64("Missing required field");
						result.mErrors.Add(err);
					}
				}
				currentNode = currentNode->GetNextConst();
			}
		}

	} // namespace AssetCatalogue
} // namespace Dia
