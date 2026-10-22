#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaObservation/Log/DiaLog.h>

#include <memory>
#include <string>
#include <string.h>
#include <unordered_map>
#include <vector>

namespace Dia::ValueSchema
{
	using Dia::Core::StringCRC;

	struct ValueSchemaRegistry::Impl
	{
		struct Format
		{
			std::string											mFor;		// the format string behind the CRC key
			std::unordered_map<unsigned int, const ValueInfo*>	mByKey;
			std::vector<const ValueInfo*>						mOrdered;
		};

		std::vector<std::unique_ptr<ValueInfo>>					mStorage;
		std::unordered_map<unsigned int, const ValueInfo*>		mByFullKey;
		std::unordered_map<unsigned int, Format>				mFormats;
		std::unordered_map<unsigned int, const ValueInfo*>		mEvents;
		bool													mPublished = false;
	};

	ValueSchemaRegistry::ValueSchemaRegistry() : mImpl(new Impl()) {}
	ValueSchemaRegistry::~ValueSchemaRegistry() { delete mImpl; }

	RegisterResult ValueSchemaRegistry::Register(const ValueInfo& info)
	{
		return RegisterInternal(info, nullptr);
	}

	RegisterResult ValueSchemaRegistry::Register(const ValueInfo& info, StringCRC fullKeyHashOverride)
	{
		return RegisterInternal(info, &fullKeyHashOverride);
	}

	RegisterResult ValueSchemaRegistry::RegisterInternal(const ValueInfo& info, const StringCRC* hashOverride)
	{
		if (mImpl->mPublished)
		{
			DIA_LOG_ERROR("VSC", "Register of '%s/%s' rejected: registry already published", info.mFor.AsCStr(), info.mKey.AsCStr());
			return RegisterResult::AlreadyPublished;
		}

		Dia::Core::Containers::String256 full;
		full.Append(info.mFor.AsCStr());
		full.Append("/");
		full.Append(info.mKey.AsCStr());

		const unsigned int fullHash = hashOverride ? hashOverride->Value() : StringCRC(full.AsCStr()).Value();

		auto existing = mImpl->mByFullKey.find(fullHash);
		if (existing != mImpl->mByFullKey.end())
		{
			if (strcmp(existing->second->mFullKey.AsCStr(), full.AsCStr()) == 0)
			{
				DIA_LOG_ERROR("VSC", "Duplicate key '%s': first defined in '%s', again in '%s' (first wins)",
					full.AsCStr(), existing->second->mSourceFile.AsCStr(), info.mSourceFile.AsCStr());
				return RegisterResult::DuplicateKey;
			}
			DIA_LOG_ERROR("VSC", "Hash collision: '%s' (%s) vs '%s' (%s); second rejected",
				existing->second->mFullKey.AsCStr(), existing->second->mSourceFile.AsCStr(), full.AsCStr(), info.mSourceFile.AsCStr());
			return RegisterResult::HashCollision;
		}

		// Everything below compares the stored STRINGS, not just hashes: two different strings with equal CRCs are
		// rejected as HashCollision instead of silently overwriting an index slot (which would make
		// Find(fullKey) and Find(for, key) disagree).
		const unsigned int keyHash = StringCRC(info.mKey.AsCStr()).Value();
		const unsigned int forHash = StringCRC(info.mFor.AsCStr()).Value();

		auto format = mImpl->mFormats.find(forHash);
		if (format != mImpl->mFormats.end())
		{
			if (format->second.mFor != info.mFor.AsCStr())
			{
				DIA_LOG_ERROR("VSC", "Hash collision between formats '%s' and '%s' (key '%s' from %s); second rejected",
					format->second.mFor.c_str(), info.mFor.AsCStr(), info.mKey.AsCStr(), info.mSourceFile.AsCStr());
				return RegisterResult::HashCollision;
			}
			auto sameKeyHash = format->second.mByKey.find(keyHash);
			if (sameKeyHash != format->second.mByKey.end())
			{
				if (strcmp(sameKeyHash->second->mKey.AsCStr(), info.mKey.AsCStr()) == 0)
				{
					DIA_LOG_ERROR("VSC", "Duplicate key '%s': first defined in '%s', again in '%s' (first wins)",
						full.AsCStr(), sameKeyHash->second->mSourceFile.AsCStr(), info.mSourceFile.AsCStr());
					return RegisterResult::DuplicateKey;
				}
				DIA_LOG_ERROR("VSC", "Hash collision: key '%s' vs '%s' in format '%s' (%s); second rejected",
					sameKeyHash->second->mKey.AsCStr(), info.mKey.AsCStr(), info.mFor.AsCStr(), info.mSourceFile.AsCStr());
				return RegisterResult::HashCollision;
			}
		}

		if (info.mKind == ValueKind::Event)
		{
			auto ev = mImpl->mEvents.find(keyHash);
			if (ev != mImpl->mEvents.end())
			{
				if (strcmp(ev->second->mKey.AsCStr(), info.mKey.AsCStr()) != 0)
				{
					DIA_LOG_ERROR("VSC", "Hash collision between event keys '%s' ('%s') and '%s' ('%s'); second rejected",
						ev->second->mKey.AsCStr(), ev->second->mFullKey.AsCStr(), info.mKey.AsCStr(), full.AsCStr());
					return RegisterResult::HashCollision;
				}
				DIA_LOG_ERROR("VSC", "Duplicate event key '%s': '%s' (%s) vs '%s' (%s); first wins",
					info.mKey.AsCStr(), ev->second->mFullKey.AsCStr(), ev->second->mSourceFile.AsCStr(), full.AsCStr(), info.mSourceFile.AsCStr());
				return RegisterResult::DuplicateEventKey;
			}
		}

		std::unique_ptr<ValueInfo> stored(new ValueInfo(info));
		SetText(stored->mFullKey, full.AsCStr());	// verbatim: a '%' in a key is not a format specifier
		const ValueInfo* p = stored.get();
		mImpl->mStorage.push_back(std::move(stored));

		mImpl->mByFullKey[fullHash] = p;
		Impl::Format& fmt = mImpl->mFormats[forHash];
		if (fmt.mOrdered.empty()) { fmt.mFor = info.mFor.AsCStr(); }
		fmt.mByKey[keyHash] = p;
		fmt.mOrdered.push_back(p);
		if (info.mKind == ValueKind::Event)
		{
			mImpl->mEvents[keyHash] = p;
		}
		return RegisterResult::Ok;
	}

	void ValueSchemaRegistry::Publish() { mImpl->mPublished = true; }
	bool ValueSchemaRegistry::IsPublished() const { return mImpl->mPublished; }

	const ValueInfo* ValueSchemaRegistry::Find(StringCRC fullKey) const
	{
		auto it = mImpl->mByFullKey.find(fullKey.Value());
		return it == mImpl->mByFullKey.end() ? nullptr : it->second;
	}

	const ValueInfo* ValueSchemaRegistry::Find(StringCRC forFormat, StringCRC key) const
	{
		auto f = mImpl->mFormats.find(forFormat.Value());
		if (f == mImpl->mFormats.end()) return nullptr;
		auto it = f->second.mByKey.find(key.Value());
		return it == f->second.mByKey.end() ? nullptr : it->second;
	}

	ValueInfoSpan ValueSchemaRegistry::ForFormat(StringCRC formatId) const
	{
		ValueInfoSpan span;
		auto f = mImpl->mFormats.find(formatId.Value());
		if (f != mImpl->mFormats.end() && !f->second.mOrdered.empty())
		{
			span.mItems = f->second.mOrdered.data();
			span.mCount = static_cast<unsigned int>(f->second.mOrdered.size());
		}
		return span;
	}

	const ValueInfo* ValueSchemaRegistry::FindEvent(StringCRC eventKey) const
	{
		auto it = mImpl->mEvents.find(eventKey.Value());
		return it == mImpl->mEvents.end() ? nullptr : it->second;
	}

	unsigned int ValueSchemaRegistry::Count() const
	{
		return static_cast<unsigned int>(mImpl->mStorage.size());
	}
}
