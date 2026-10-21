#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaObservation/Log/DiaLog.h>

#include <memory>
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

		const unsigned int keyHash = StringCRC(info.mKey.AsCStr()).Value();
		if (info.mKind == ValueKind::Event)
		{
			auto ev = mImpl->mEvents.find(keyHash);
			if (ev != mImpl->mEvents.end())
			{
				DIA_LOG_ERROR("VSC", "Duplicate event key '%s': '%s' (%s) vs '%s' (%s); first wins",
					info.mKey.AsCStr(), ev->second->mFullKey.AsCStr(), ev->second->mSourceFile.AsCStr(), full.AsCStr(), info.mSourceFile.AsCStr());
				return RegisterResult::DuplicateEventKey;
			}
		}

		std::unique_ptr<ValueInfo> stored(new ValueInfo(info));
		stored->mFullKey = full.AsCStr();
		const ValueInfo* p = stored.get();
		mImpl->mStorage.push_back(std::move(stored));

		mImpl->mByFullKey[fullHash] = p;
		Impl::Format& fmt = mImpl->mFormats[StringCRC(info.mFor.AsCStr()).Value()];
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
