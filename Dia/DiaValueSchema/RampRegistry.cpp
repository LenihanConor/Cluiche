#include <DiaValueSchema/RampRegistry.h>
#include <DiaObservation/Log/DiaLog.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace Dia::ValueSchema
{
	using Dia::Core::StringCRC;

	struct RampRegistry::Impl
	{
		struct Entry
		{
			std::unique_ptr<RampData>	mData;
			bool						mIsDia = false;
			std::string					mFile;
		};
		std::unordered_map<unsigned int, Entry>	mById;
		bool									mPublished = false;
	};

	RampRegistry::RampRegistry() : mImpl(new Impl()) {}
	RampRegistry::~RampRegistry() { delete mImpl; }

	RampRegisterResult RampRegistry::Register(const RampData& ramp, bool isDia, const char* sourceFile)
	{
		const char* file = sourceFile ? sourceFile : "";
		if (mImpl->mPublished)
		{
			DIA_LOG_ERROR("VSC", "Register of ramp '%s' (%s) rejected: registry already published", ramp.mName.AsCStr(), file);
			return RampRegisterResult::AlreadyPublished;
		}

		auto it = mImpl->mById.find(ramp.mId.Value());
		if (it != mImpl->mById.end())
		{
			const Impl::Entry& existing = it->second;
			if (existing.mIsDia && !isDia)
			{
				DIA_LOG_ERROR("VSC", "Ramp '%s' in '%s' rejected: id clashes with Dia ramp from '%s' (Dia wins)",
					ramp.mName.AsCStr(), file, existing.mFile.c_str());
				return RampRegisterResult::ClashesWithDia;
			}
			DIA_LOG_ERROR("VSC", "Ramp '%s' in '%s' rejected: duplicate id, first defined in '%s'",
				ramp.mName.AsCStr(), file, existing.mFile.c_str());
			return RampRegisterResult::DuplicateId;
		}

		Impl::Entry entry;
		entry.mData = std::make_unique<RampData>(ramp);
		entry.mIsDia = isDia;
		entry.mFile = file;
		mImpl->mById.emplace(ramp.mId.Value(), std::move(entry));
		return RampRegisterResult::Ok;
	}

	void RampRegistry::Publish() { mImpl->mPublished = true; }
	bool RampRegistry::IsPublished() const { return mImpl->mPublished; }

	const RampData* RampRegistry::Find(StringCRC rampId) const
	{
		auto it = mImpl->mById.find(rampId.Value());
		return it == mImpl->mById.end() ? nullptr : it->second.mData.get();
	}

	unsigned int RampRegistry::Count() const { return static_cast<unsigned int>(mImpl->mById.size()); }
}
