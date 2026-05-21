#pragma once

#include "DiaAssetCatalogue/JsonDefinitionLoader.h"

#include "DiaCore/Reflect/JsonArchive.h"
#include "DiaCore/Reflect/SerializeResult.h"
#include "DiaCore/Json/external/json/json.h"
#include "DiaCore/Containers/Strings/StringReader.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String64.h"

#include <stdio.h>

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

			// Stack-allocate up to 64KB; consistent with DiaCore's avoidance of dynamic alloc.
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

			const char* jsonText = buffer.AsCStr();

			// Parse JSON
			Json::Value root;
			Json::Reader reader;
			if (!reader.parse(jsonText, root, false))
			{
				LoadError err;
				err.mKind = LoadErrorKind::JsonParseError;
				err.mFieldPath = Dia::Core::StringCRC("");
				err.mMessage = Dia::Core::Containers::String64("JSON parse error");
				result.mErrors.Add(err);
				return result;
			}

			// Deserialize via DiaReflect JsonReadArchive
			Dia::Reflect::JsonReadArchive ar(root);
			using ::serialize;
			serialize(ar, result.mValue, 0u);

			// Map DiaReflect errors to LoadError entries
			const Dia::Reflect::SerializeResult& serResult = ar.GetResult();
			if (serResult.HasErrors())
			{
				for (unsigned int i = 0u; i < serResult.ErrorCount(); ++i)
				{
					const Dia::Reflect::SerializeError& serErr = serResult.GetError(i);

					LoadError err;
					if (serErr.kind == Dia::Reflect::SerializeErrorKind::RequiredFieldMissing)
					{
						err.mKind = LoadErrorKind::MissingRequiredField;
					}
					else
					{
						err.mKind = LoadErrorKind::DeserializationError;
					}
					err.mFieldPath = serErr.fieldName;
					err.mMessage = Dia::Core::Containers::String64("Serialization error");
					if (!result.mErrors.IsFull())
					{
						result.mErrors.Add(err);
					}
				}
			}

			if (!result.HasErrors())
			{
				result.mSuccess = true;
			}

			return result;
		}

	} // namespace AssetCatalogue
} // namespace Dia
