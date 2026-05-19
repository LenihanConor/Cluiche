#include "DiaObservation/Profile/ProfileFileSink.h"

#include "DiaCore/Json/external/json/json.h"

#include <cstring>
#include <cstdio>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		ProfileFileSink::ProfileFileSink(const char* jsonlPath, const char* finalPath,
		                                 const char* sessionId, int64_t epochOffsetNs)
			: mFrameHead(0)
			, mFrameCount(0)
			, mCurrentFrameSlot(-1)
			, mLastFrameNumber(UINT32_MAX)
			, mFile(nullptr)
			, mEpochOffsetNs(epochOffsetNs)
		{
			std::memset(mFinalPath, 0, sizeof(mFinalPath));
			std::memset(mSessionId, 0, sizeof(mSessionId));
			std::memset(mFrameRing,  0, sizeof(mFrameRing));

			if (finalPath)
				strncpy_s(mFinalPath, finalPath, sizeof(mFinalPath) - 1);
			if (sessionId)
				strncpy_s(mSessionId, sessionId, sizeof(mSessionId) - 1);

			if (jsonlPath)
				fopen_s(&mFile, jsonlPath, "ab");
		}

		ProfileFileSink::~ProfileFileSink()
		{
			if (mFile)
			{
				fclose(mFile);
				mFile = nullptr;
			}
		}

		void ProfileFileSink::OnRecord(const ScopeRecord& record)
		{
			// --- Write to .jsonl ---
			if (mFile)
			{
				int64_t startUnixNano = static_cast<int64_t>(record.startUnixNano) + mEpochOffsetNs;

				char line[512];
				int len = snprintf(line, sizeof(line),
					"{\"schema_version\":\"1.0\","
					"\"record_type\":\"profile_scope\","
					"\"session_id\":\"%s\","
					"\"frame_number\":%u,"
					"\"thread_id\":%u,"
					"\"scope_name\":%u,"
					"\"category\":%u,"
					"\"scope_id\":%llu,"
					"\"parent_scope_id\":%llu,"
					"\"start_unix_nano\":%lld,"
					"\"duration_ns\":%llu}\n",
					mSessionId,
					record.frameNumber,
					record.threadId,
					static_cast<unsigned int>(record.name.Value()),
					static_cast<unsigned int>(record.category),
					static_cast<unsigned long long>(record.scopeId),
					static_cast<unsigned long long>(record.parentScopeId),
					static_cast<long long>(startUnixNano),
					static_cast<unsigned long long>(record.durationNs));

				if (len > 0)
					fwrite(line, 1, static_cast<size_t>(len), mFile);
			}

			// --- Accumulate in frame ring ---
			if (record.frameNumber != mLastFrameNumber)
			{
				// Advance to a new frame slot
				if (mFrameCount < kMaxFrames)
				{
					// Ring not yet full — append at end
					unsigned int newSlot = (mFrameHead + mFrameCount) % kMaxFrames;
					mFrameRing[newSlot].frameNumber = record.frameNumber;
					mFrameRing[newSlot].scopeCount  = 0;
					mCurrentFrameSlot               = static_cast<int>(newSlot);
					++mFrameCount;
				}
				else
				{
					// Ring full — overwrite oldest slot
					mFrameRing[mFrameHead].frameNumber = record.frameNumber;
					mFrameRing[mFrameHead].scopeCount  = 0;
					mCurrentFrameSlot                  = static_cast<int>(mFrameHead);
					mFrameHead                         = (mFrameHead + 1) % kMaxFrames;
				}

				mLastFrameNumber = record.frameNumber;
			}

			// Append record to current frame slot (drop extras beyond kMaxScopesPerFrame)
			if (mCurrentFrameSlot >= 0)
			{
				FrameData& frame = mFrameRing[mCurrentFrameSlot];
				if (frame.scopeCount < kMaxScopesPerFrame)
				{
					frame.scopes[frame.scopeCount] = record;
					++frame.scopeCount;
				}
			}
		}

		void ProfileFileSink::WriteFinalJson()
		{
			if (mFinalPath[0] == '\0')
				return;

			FILE* finalFile = nullptr;
			fopen_s(&finalFile, mFinalPath, "wb");
			if (!finalFile)
				return;

			Json::Value root(Json::objectValue);
			root["schema_version"]  = Json::Value("1.0");
			root["session_id"]      = Json::Value(mSessionId);
			root["frames_captured"] = Json::Value(static_cast<Json::UInt>(mFrameCount));

			Json::Value framesArray(Json::arrayValue);

			for (unsigned int f = 0; f < mFrameCount; ++f)
			{
				unsigned int slot = (mFrameHead + f) % kMaxFrames;
				const FrameData& frame = mFrameRing[slot];

				Json::Value frameObj(Json::objectValue);
				frameObj["frame_number"] = Json::Value(static_cast<Json::UInt>(frame.frameNumber));

				Json::Value scopesArray(Json::arrayValue);
				for (unsigned int s = 0; s < frame.scopeCount; ++s)
				{
					const ScopeRecord& record = frame.scopes[s];
					int64_t startUnixNano = static_cast<int64_t>(record.startUnixNano) + mEpochOffsetNs;

					Json::Value scopeObj(Json::objectValue);
					scopeObj["scope_name"]     = Json::Value(static_cast<Json::UInt>(record.name.Value()));
					scopeObj["category"]       = Json::Value(static_cast<Json::UInt>(record.category));
					scopeObj["scope_id"]       = Json::Value(static_cast<Json::UInt64>(record.scopeId));
					scopeObj["parent_scope_id"]= Json::Value(static_cast<Json::UInt64>(record.parentScopeId));
					scopeObj["start_unix_nano"]= Json::Value(static_cast<Json::Int64>(startUnixNano));
					scopeObj["duration_ns"]    = Json::Value(static_cast<Json::UInt64>(record.durationNs));
					scopeObj["frame_number"]   = Json::Value(static_cast<Json::UInt>(record.frameNumber));
					scopeObj["thread_id"]      = Json::Value(static_cast<Json::UInt>(record.threadId));
					scopesArray.append(scopeObj);
				}

				frameObj["scopes"] = scopesArray;
				framesArray.append(frameObj);
			}

			root["frames"] = framesArray;

			Json::StreamWriterBuilder builder;
			builder["indentation"] = "  ";
			builder["commentStyle"] = "None";

			std::string output = Json::writeString(builder, root);

			// Replace any \r\n with \n to enforce LF-only line endings
			std::string lfOutput;
			lfOutput.reserve(output.size());
			for (size_t i = 0; i < output.size(); ++i)
			{
				if (output[i] == '\r' && i + 1 < output.size() && output[i + 1] == '\n')
					continue; // skip the \r
				lfOutput += output[i];
			}

			fwrite(lfOutput.c_str(), 1, lfOutput.size(), finalFile);
			fclose(finalFile);
		}
	}
} // namespace Observation
} // namespace Dia
