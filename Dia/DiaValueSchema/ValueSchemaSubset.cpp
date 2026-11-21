#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/ValueSchemaQuery.h>

#include <DiaObservation/Log/DiaLog.h>

#include <string>
#include <unordered_set>
#include <vector>

namespace Dia::ValueSchema
{
	using Dia::Core::StringCRC;

	namespace
	{
		// "<for>/<ref>" first (for-relative reference), then the reference as written (full key).
		const ValueInfo* ResolveReference(const ValueSchemaService& service, const ValueInfo& owner, const char* ref)
		{
			if (ref == nullptr || ref[0] == '\0') { return nullptr; }
			const std::string relative = std::string(owner.mFor.AsCStr()) + "/" + ref;
			if (const ValueInfo* found = service.Find(StringCRC(relative.c_str()))) { return found; }
			return service.Find(StringCRC(ref));
		}

		void NoteRamp(const ValueSchemaService& service, const ValueInfo& info, const char* field, std::unordered_set<unsigned int>& used)
		{
			if (!info.mUi.isObject() || !info.mUi.isMember(field) || !info.mUi[field].isString()) { return; }
			const StringCRC id(info.mUi[field].asCString());
			if (service.FindRamp(id) != nullptr) { used.insert(static_cast<unsigned int>(id)); }
		}
	}

	bool ValueSchemaService::ExportSubset(const StringCRC* keys, unsigned int keyCount, Json::Value& out, SubsetReport* report) const
	{
		if (keys == nullptr && keyCount > 0) { return false; }

		SubsetReport local;
		SubsetReport& rep = report ? *report : local;
		rep = SubsetReport();

		std::vector<const ValueInfo*> picked;
		std::unordered_set<const ValueInfo*> seen;
		std::unordered_set<unsigned int> unknownSeen;

		const unsigned int requestedEnd = keyCount;
		for (unsigned int i = 0; i < requestedEnd; ++i)
		{
			const ValueInfo* info = Find(keys[i]);
			if (info == nullptr)
			{
				if (unknownSeen.insert(static_cast<unsigned int>(keys[i])).second)
				{
					++rep.mSkippedUnknown;
					DIA_LOG_WARNING("VSC", "ExportSubset: unknown key '%s' (crc %u) skipped", keys[i].AsChar() ? keys[i].AsChar() : "", static_cast<unsigned int>(keys[i]));
				}
				continue;
			}
			if (seen.insert(info).second) { picked.push_back(info); }
		}

		// One level of references, in first-seen order.
		const size_t requestedCount = picked.size();
		for (size_t i = 0; i < requestedCount; ++i)
		{
			const ValueInfo& owner = *picked[i];
			for (int list = 0; list < 2; ++list)
			{
				const auto& refs = list == 0 ? owner.mAffects : owner.mRelated;
				for (unsigned int r = 0; r < refs.Size(); ++r)
				{
					const ValueInfo* target = ResolveReference(*this, owner, refs[r].AsCStr());
					if (target == nullptr) { ++rep.mReferencedUnresolved; continue; }
					if (seen.insert(target).second) { picked.push_back(target); }
				}
			}
		}

		Json::Value entries(Json::arrayValue);
		std::unordered_set<unsigned int> usedRamps;
		for (const ValueInfo* info : picked)
		{
			entries.append(ValueInfoToExportJson(*info));
			NoteRamp(*this, *info, "ramp", usedRamps);
			NoteRamp(*this, *info, "delta_ramp", usedRamps);
		}

		Json::Value ramps(Json::arrayValue);
		for (unsigned int i = 0; i < RampCount(); ++i)
		{
			const RampData* ramp = RampAt(i);
			if (ramp != nullptr && usedRamps.count(static_cast<unsigned int>(ramp->mId)) != 0) { ramps.append(RampToJson(*ramp)); }
		}

		out = Json::Value(Json::objectValue);
		out["entries"] = entries;
		out["ramps"] = ramps;
		return true;
	}
}
