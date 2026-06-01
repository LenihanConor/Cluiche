////////////////////////////////////////////////////////////////////////////////
// Filename: LightRegistry2D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaLighting2D/Registry/LightRegistry2D.h"

#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Lighting2D
	{
		////////////////////////////////////////////////////////////
		LightRegistry2D::LightRegistry2D()
		{
		}

		////////////////////////////////////////////////////////////
		void LightRegistry2D::Register(Dia::Core::StringCRC id, PointLight2D light)
		{
			if (Has(id))
			{
				DIA_ASSERT(false, "LightRegistry2D::Register — duplicate light id");
				return;
			}
			DIA_ASSERT(mCount < kMaxLights, "LightRegistry2D::Register — capacity exceeded");

			mSlots[mCount].id    = id;
			mSlots[mCount].light = light;
			++mCount;
		}

		////////////////////////////////////////////////////////////
		void LightRegistry2D::Unregister(Dia::Core::StringCRC id)
		{
			const int idx = FindIndex(id);
			DIA_ASSERT(idx >= 0, "LightRegistry2D::Unregister — light not found");
			if (idx < 0) return;

			// Swap with last to fill gap
			const unsigned int last = mCount - 1;
			if (static_cast<unsigned int>(idx) != last)
				mSlots[idx] = mSlots[last];

			--mCount;
		}

		////////////////////////////////////////////////////////////
		bool LightRegistry2D::Has(Dia::Core::StringCRC id) const
		{
			return FindIndex(id) >= 0;
		}

		////////////////////////////////////////////////////////////
		PointLight2D& LightRegistry2D::Get(Dia::Core::StringCRC id)
		{
			const int idx = FindIndex(id);
			DIA_ASSERT(idx >= 0, "LightRegistry2D::Get — light not found");
			return mSlots[idx].light;
		}

		////////////////////////////////////////////////////////////
		const PointLight2D& LightRegistry2D::Get(Dia::Core::StringCRC id) const
		{
			const int idx = FindIndex(id);
			DIA_ASSERT(idx >= 0, "LightRegistry2D::Get — light not found");
			return mSlots[idx].light;
		}

		////////////////////////////////////////////////////////////
		PointLight2D& LightRegistry2D::GetByIndex(unsigned int index)
		{
			DIA_ASSERT(index < mCount, "LightRegistry2D::GetByIndex — index out of range");
			return mSlots[index].light;
		}

		////////////////////////////////////////////////////////////
		const PointLight2D& LightRegistry2D::GetByIndex(unsigned int index) const
		{
			DIA_ASSERT(index < mCount, "LightRegistry2D::GetByIndex — index out of range");
			return mSlots[index].light;
		}

		////////////////////////////////////////////////////////////
		void LightRegistry2D::GetLightsForLayer(
			unsigned int layerBitIndex,
			Dia::Core::Containers::DynamicArrayC<const PointLight2D*, 32>& outLights) const
		{
			DIA_ASSERT(layerBitIndex < 32, "LightRegistry2D::GetLightsForLayer — layerBitIndex >= 32");

			const uint32_t bit = (1u << layerBitIndex);
			for (unsigned int i = 0; i < mCount; ++i)
			{
				const PointLight2D& l = mSlots[i].light;
				if (l.enabled && (l.layerMask & bit))
					outLights.Add(&l);
			}
		}

		////////////////////////////////////////////////////////////
		int LightRegistry2D::FindIndex(Dia::Core::StringCRC id) const
		{
			for (unsigned int i = 0; i < mCount; ++i)
			{
				if (mSlots[i].id == id)
					return static_cast<int>(i);
			}
			return -1;
		}

	} // namespace Lighting2D
} // namespace Dia
