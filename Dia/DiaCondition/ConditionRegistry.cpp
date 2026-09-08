#include "ConditionRegistry.h"

#include <DiaCore/Core/Assert.h>

#include <cstdint>
#include <unordered_map>

namespace Dia
{
	namespace Condition
	{
		// -----------------------------------------------------------------------------------------
		// Impl — pimpl struct holding the two std::unordered_map tables.
		// Kept in the .cpp so no STL leaks into the public header (PD-004).
		// -----------------------------------------------------------------------------------------
		struct ConditionRegistry::Impl
		{
			std::unordered_map<uint64_t, FloatAccessorFn> floatMap;
			std::unordered_map<uint64_t, BoolAccessorFn>  boolMap;
		};

		// -----------------------------------------------------------------------------------------
		// Internal helper: combine slot + field CRCs into a single 64-bit lookup key (PD-001).
		// -----------------------------------------------------------------------------------------
		static inline uint64_t MakeKey(Dia::Core::StringCRC slot, Dia::Core::StringCRC field)
		{
			return (static_cast<uint64_t>(slot.Value()) << 32) | static_cast<uint32_t>(field.Value());
		}

		// -----------------------------------------------------------------------------------------
		// ConditionRegistry
		// -----------------------------------------------------------------------------------------

		ConditionRegistry::ConditionRegistry(void* data)
			: mData(data)
			, mImpl(new Impl())
		{
		}

		ConditionRegistry::~ConditionRegistry()
		{
			delete mImpl;
		}

		void ConditionRegistry::RegisterFloat(Dia::Core::StringCRC slot, Dia::Core::StringCRC field, FloatAccessorFn accessor)
		{
			DIA_ASSERT(accessor != nullptr, "ConditionRegistry::RegisterFloat — accessor must not be null");
			mImpl->floatMap[MakeKey(slot, field)] = accessor;
		}

		void ConditionRegistry::RegisterBool(Dia::Core::StringCRC slot, Dia::Core::StringCRC field, BoolAccessorFn accessor)
		{
			DIA_ASSERT(accessor != nullptr, "ConditionRegistry::RegisterBool — accessor must not be null");
			mImpl->boolMap[MakeKey(slot, field)] = accessor;
		}

		bool ConditionRegistry::HasFloat(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const
		{
			return mImpl->floatMap.count(MakeKey(slot, field)) != 0;
		}

		bool ConditionRegistry::HasBool(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const
		{
			return mImpl->boolMap.count(MakeKey(slot, field)) != 0;
		}

		float ConditionRegistry::GetFloat(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const
		{
			const uint64_t key = MakeKey(slot, field);
			const auto it = mImpl->floatMap.find(key);

			DIA_ASSERT(it != mImpl->floatMap.end(), "ConditionRegistry::GetFloat — unregistered slot/field");

			if (it == mImpl->floatMap.end())
			{
				return 0.0f;
			}

			return it->second(mData);
		}

		bool ConditionRegistry::GetBool(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const
		{
			const uint64_t key = MakeKey(slot, field);
			const auto it = mImpl->boolMap.find(key);

			DIA_ASSERT(it != mImpl->boolMap.end(), "ConditionRegistry::GetBool — unregistered slot/field");

			if (it == mImpl->boolMap.end())
			{
				return false;
			}

			return it->second(mData);
		}

	} // namespace Condition
} // namespace Dia
