////////////////////////////////////////////////////////////////////////////////
// Filename: LightRegistry2D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaLighting2D/PointLight2D.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace Lighting2D
	{
		////////////////////////////////////////////////////////////
		/// \brief Named registry for 2D point lights.
		///
		/// Fixed-slot storage — kMaxLights entries max.
		/// Caller is responsible for layer-name → bitmask resolution
		/// before registering (see DiaScene2D::LayerTable::ResolveMask).
		////////////////////////////////////////////////////////////
		class LightRegistry2D
		{
		public:
			static const unsigned int kMaxLights = 16;

			LightRegistry2D();

			// Registration
			void Register(Dia::Core::StringCRC id, PointLight2D light);
			void Unregister(Dia::Core::StringCRC id);
			bool Has(Dia::Core::StringCRC id) const;

			// Direct access
			PointLight2D&       Get(Dia::Core::StringCRC id);
			const PointLight2D& Get(Dia::Core::StringCRC id) const;

			// Indexed iteration (for renderer consumption)
			unsigned int        GetCount() const { return mCount; }
			PointLight2D&       GetByIndex(unsigned int index);
			const PointLight2D& GetByIndex(unsigned int index) const;

			// Query: lights whose layerMask has bit layerBitIndex set and are enabled
			void GetLightsForLayer(unsigned int layerBitIndex,
			                       Dia::Core::Containers::DynamicArrayC<const PointLight2D*, 32>& outLights) const;

		private:
			int FindIndex(Dia::Core::StringCRC id) const;

			struct LightSlot
			{
				Dia::Core::StringCRC id;
				PointLight2D         light;
			};

			LightSlot    mSlots[kMaxLights];
			unsigned int mCount = 0;
		};

	} // namespace Lighting2D
} // namespace Dia
