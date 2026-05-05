#include "DiaAssetCatalogue/CatalogueManifestSerializer.h"
#include "DiaAssetCatalogue/RelationshipTypes.h"

#include "DiaCore/Json/external/json/json.h"
#include "DiaCore/Strings/String64.h"
#include "DiaCore/Strings/String256.h"
#include "DiaCore/Strings/String512.h"

#include <stdio.h>
#include <string.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace
		{
			//------------------------------------------------------------------------------------
			// ReadFileToBuffer
			// Reads an entire file into a static buffer. Returns false on failure.
			//------------------------------------------------------------------------------------
			static const unsigned int kMaxManifestSize = 64 * 1024;
			static char sManifestBuffer[kMaxManifestSize];
			static char sSubManifestBuffer[kMaxManifestSize];

			bool ReadFileToBuffer(const char* path, char* buffer, unsigned int bufferSize)
			{
				FILE* f = nullptr;
#if defined(_MSC_VER)
				fopen_s(&f, path, "rb");
#else
				f = fopen(path, "rb");
#endif
				if (!f)
				{
					return false;
				}

				fseek(f, 0, SEEK_END);
				long fileSize = ftell(f);
				fseek(f, 0, SEEK_SET);

				if (fileSize <= 0 || static_cast<unsigned long>(fileSize) >= bufferSize)
				{
					fclose(f);
					return false;
				}

				size_t bytesRead = fread(buffer, 1, static_cast<size_t>(fileSize), f);
				fclose(f);
				buffer[bytesRead] = '\0';
				return true;
			}

			//------------------------------------------------------------------------------------
			// GetDirectoryFromPath
			// Returns the directory portion of a path (including trailing slash), or "" for
			// paths with no directory component.
			//------------------------------------------------------------------------------------
			void GetDirectoryFromPath(const char* path, char* dirOut, unsigned int dirOutSize)
			{
				if (path == nullptr || path[0] == '\0')
				{
					dirOut[0] = '\0';
					return;
				}

				// Find the last '/' or '\\'
				int len = static_cast<int>(strlen(path));
				int lastSlash = -1;
				for (int i = len - 1; i >= 0; --i)
				{
					if (path[i] == '/' || path[i] == '\\')
					{
						lastSlash = i;
						break;
					}
				}

				if (lastSlash < 0)
				{
					dirOut[0] = '\0';
					return;
				}

				int copyLen = lastSlash + 1; // include the slash
				if (copyLen >= static_cast<int>(dirOutSize))
				{
					copyLen = static_cast<int>(dirOutSize) - 1;
				}
#if defined(_MSC_VER)
				strncpy_s(dirOut, dirOutSize, path, static_cast<size_t>(copyLen));
#else
				strncpy(dirOut, path, static_cast<size_t>(copyLen));
				dirOut[copyLen] = '\0';
#endif
			}

			//------------------------------------------------------------------------------------
			// ParseStatusFromString
			//------------------------------------------------------------------------------------
			AssetStatus ParseStatus(const char* str)
			{
				if (str == nullptr) return AssetStatus::Active;
				if (strcmp(str, "draft") == 0) return AssetStatus::Draft;
				if (strcmp(str, "deprecated") == 0) return AssetStatus::Deprecated;
				return AssetStatus::Active; // default
			}

			//------------------------------------------------------------------------------------
			// StatusToString
			//------------------------------------------------------------------------------------
			const char* StatusToString(AssetStatus status)
			{
				switch (status)
				{
				case AssetStatus::Draft:       return "draft";
				case AssetStatus::Deprecated:  return "deprecated";
				case AssetStatus::Active:
				default:                       return "active";
				}
			}

			//------------------------------------------------------------------------------------
			// ScopeToString
			//------------------------------------------------------------------------------------
			const char* ScopeToString(AssetScope scope)
			{
				if (scope == AssetScope::kStage) return "stage";
				return "global";
			}

			//------------------------------------------------------------------------------------
			// ParseScopeFromString
			//------------------------------------------------------------------------------------
			AssetScope ParseScope(const char* str)
			{
				if (str != nullptr && strcmp(str, "stage") == 0) return AssetScope::kStage;
				return AssetScope::kGlobal;
			}

			//------------------------------------------------------------------------------------
			// AddError — helper to add a LoadError to a result
			//------------------------------------------------------------------------------------
			void AddError(LoadResult<AssetRegistry>& result, LoadErrorKind kind, const char* fieldPath, const char* message)
			{
				LoadError err;
				err.mKind = kind;
				err.mFieldPath = Dia::Core::StringCRC(fieldPath != nullptr ? fieldPath : "");
				// Truncate message to String64
				if (message != nullptr)
				{
					// String64 max is 64 chars; copy safely
					char buf[64];
#if defined(_MSC_VER)
					strncpy_s(buf, sizeof(buf), message, _TRUNCATE);
#else
					strncpy(buf, message, 63);
					buf[63] = '\0';
#endif
					err.mMessage = Dia::Core::Containers::String64(buf);
				}
				if (!result.mErrors.IsFull())
				{
					result.mErrors.Add(err);
				}
			}

			//------------------------------------------------------------------------------------
			// ParseAssetRecord — fills an AssetRecord from a Json::Value object
			//------------------------------------------------------------------------------------
			bool ParseAssetRecord(const Json::Value& obj, AssetRecord& recordOut)
			{
				if (!obj.isObject()) return false;

				if (!obj.isMember("id") || !obj["id"].isString()) return false;
				if (!obj.isMember("type") || !obj["type"].isString()) return false;

				recordOut.mId          = Dia::Core::StringCRC(obj["id"].asCString());
				recordOut.mAssetTypeId = Dia::Core::StringCRC(obj["type"].asCString());

				if (obj.isMember("source_path") && obj["source_path"].isString())
				{
					recordOut.mSourcePath = Dia::Core::Containers::String256(obj["source_path"].asCString());
				}

				if (obj.isMember("content_hash") && obj["content_hash"].isNumeric())
				{
					recordOut.mContentHash = static_cast<unsigned int>(obj["content_hash"].asUInt());
				}

				if (obj.isMember("status") && obj["status"].isString())
				{
					recordOut.mStatus = ParseStatus(obj["status"].asCString());
				}

				if (obj.isMember("scope") && obj["scope"].isString())
				{
					recordOut.mScope = ParseScope(obj["scope"].asCString());
				}

				if (obj.isMember("stage_name") && obj["stage_name"].isString())
				{
					const char* stageName = obj["stage_name"].asCString();
					if (stageName != nullptr && stageName[0] != '\0')
					{
						recordOut.mScopeStageName = Dia::Core::StringCRC(stageName);
					}
				}

				if (obj.isMember("tags") && obj["tags"].isArray())
				{
					const Json::Value& tags = obj["tags"];
					for (unsigned int t = 0; t < tags.size() && !recordOut.mTags.IsFull(); ++t)
					{
						if (tags[t].isString())
						{
							recordOut.mTags.Add(Dia::Core::StringCRC(tags[t].asCString()));
						}
					}
				}

				if (obj.isMember("references") && obj["references"].isArray())
				{
					const Json::Value& refs = obj["references"];
					for (unsigned int r = 0; r < refs.size() && !recordOut.mReferences.IsFull(); ++r)
					{
						const Json::Value& refObj = refs[r];
						if (refObj.isObject() && refObj.isMember("type") && refObj.isMember("target"))
						{
							RelationshipEdge edge;
							edge.mRelationshipType = Dia::Core::StringCRC(refObj["type"].asCString());
							edge.mTargetAssetId    = Dia::Core::StringCRC(refObj["target"].asCString());
							recordOut.mReferences.Add(edge);
						}
					}
				}

				return true;
			}

		} // anonymous namespace

		//------------------------------------------------------------------------------------
		// CatalogueManifestSerializer
		//------------------------------------------------------------------------------------
		CatalogueManifestSerializer::CatalogueManifestSerializer()
		{}

		void CatalogueManifestSerializer::ParseAssetsFromJson(
			const char* jsonText,
			const char* sourcePath,
			bool isSubManifest,
			LoadResult<AssetRegistry>& result) const
		{
			Json::Value root;
			Json::Reader reader;
			if (!reader.parse(jsonText, root, false))
			{
				AddError(result, LoadErrorKind::JsonParseError, sourcePath, "JSON parse error");
				return;
			}

			if (!root.isObject())
			{
				AddError(result, LoadErrorKind::JsonParseError, sourcePath, "Root must be object");
				return;
			}

			// Sub-manifests must not have includes
			if (isSubManifest && root.isMember("includes"))
			{
				AddError(result, LoadErrorKind::DeserializationError, sourcePath, "Sub-manifest includes not allowed");
				return;
			}

			if (!root.isMember("assets") || !root["assets"].isArray())
			{
				return; // no assets is fine
			}

			const Json::Value& assets = root["assets"];
			for (unsigned int i = 0; i < assets.size(); ++i)
			{
				AssetRecord record;
				if (!ParseAssetRecord(assets[i], record))
				{
					AddError(result, LoadErrorKind::DeserializationError, sourcePath, "Invalid asset record");
					continue;
				}

				if (!result.mValue.Register(record))
				{
					// Could be duplicate ID or invalid format
					AddError(result, LoadErrorKind::DeserializationError, record.mId.AsChar(), "Duplicate or invalid asset ID");
				}
			}
		}

		LoadResult<AssetRegistry> CatalogueManifestSerializer::LoadManifest(const char* path) const
		{
			LoadResult<AssetRegistry> result;

			if (path == nullptr || path[0] == '\0')
			{
				AddError(result, LoadErrorKind::FileNotFound, "", "Null or empty path");
				return result;
			}

			// Read master manifest
			if (!ReadFileToBuffer(path, sManifestBuffer, kMaxManifestSize))
			{
				AddError(result, LoadErrorKind::FileNotFound, path, "File not found or too large");
				return result;
			}

			// Parse master manifest JSON to get includes list before processing assets
			Json::Value root;
			Json::Reader reader;
			if (!reader.parse(sManifestBuffer, root, false))
			{
				AddError(result, LoadErrorKind::JsonParseError, path, "JSON parse error");
				return result;
			}

			// Get directory for resolving relative include paths
			char dirBuf[512];
#if defined(_MSC_VER)
			GetDirectoryFromPath(path, dirBuf, sizeof(dirBuf));
#else
			GetDirectoryFromPath(path, dirBuf, sizeof(dirBuf));
#endif

			// Process includes first
			if (root.isMember("includes") && root["includes"].isArray())
			{
				const Json::Value& includes = root["includes"];
				for (unsigned int i = 0; i < includes.size(); ++i)
				{
					if (!includes[i].isString()) continue;

					// Build absolute path for sub-manifest
					char subPath[512];
#if defined(_MSC_VER)
					snprintf(subPath, sizeof(subPath), "%s%s", dirBuf, includes[i].asCString());
#else
					snprintf(subPath, sizeof(subPath), "%s%s", dirBuf, includes[i].asCString());
#endif

					if (!ReadFileToBuffer(subPath, sSubManifestBuffer, kMaxManifestSize))
					{
						AddError(result, LoadErrorKind::FileNotFound, subPath, "Include file not found");
						continue;
					}

					ParseAssetsFromJson(sSubManifestBuffer, subPath, /*isSubManifest=*/true, result);
				}
			}

			// Process master manifest assets
			ParseAssetsFromJson(sManifestBuffer, path, /*isSubManifest=*/false, result);

			if (!result.HasErrors())
			{
				result.mSuccess = true;
			}

			return result;
		}

		bool CatalogueManifestSerializer::SaveManifest(const AssetRegistry& registry, const char* path) const
		{
			if (path == nullptr || path[0] == '\0')
			{
				return false;
			}

			Json::Value root(Json::objectValue);
			Json::Value assetsArray(Json::arrayValue);

			unsigned int count = registry.GetCount();
			for (unsigned int i = 0; i < count; ++i)
			{
				const AssetRecord& record = registry.GetRecordByIndex(i);

				Json::Value obj(Json::objectValue);
				obj["id"]           = record.mId.AsChar();
				obj["type"]         = record.mAssetTypeId.AsChar();
				obj["source_path"]  = record.mSourcePath.AsCStr();
				obj["content_hash"] = record.mContentHash;
				obj["status"]       = StatusToString(record.mStatus);
				obj["scope"]        = ScopeToString(record.mScope);
				obj["stage_name"]   = record.mScopeStageName.AsChar();

				Json::Value tagsArray(Json::arrayValue);
				for (unsigned int t = 0; t < record.mTags.Size(); ++t)
				{
					tagsArray.append(record.mTags[t].AsChar());
				}
				obj["tags"] = tagsArray;

				Json::Value refsArray(Json::arrayValue);
				for (unsigned int r = 0; r < record.mReferences.Size(); ++r)
				{
					Json::Value refObj(Json::objectValue);
					refObj["type"]   = record.mReferences[r].mRelationshipType.AsChar();
					refObj["target"] = record.mReferences[r].mTargetAssetId.AsChar();
					refsArray.append(refObj);
				}
				obj["references"] = refsArray;

				assetsArray.append(obj);
			}

			root["assets"] = assetsArray;

			Json::StyledWriter writer;
			std::string jsonOutput = writer.write(root);

			FILE* f = nullptr;
#if defined(_MSC_VER)
			fopen_s(&f, path, "wb");
#else
			f = fopen(path, "wb");
#endif
			if (!f)
			{
				return false;
			}

			fwrite(jsonOutput.c_str(), 1, jsonOutput.size(), f);
			fclose(f);
			return true;
		}

	} // namespace AssetCatalogue
} // namespace Dia
