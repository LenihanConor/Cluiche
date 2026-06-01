////////////////////////////////////////////////////////////////////////////////
// Filename: LightBuilder.h — Fluent builder for test light setup
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaLighting2D/PointLight2D.h>
#include <DiaLighting2D/Registry/LightRegistry2D.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Lighting2D
	{
		namespace Testing
		{
			/// Fluent builder for constructing LightRegistry2D in tests.
			class LightBuilder
			{
			public:
				LightBuilder& WithLight(const char* id,
				                        float posX = 0.0f, float posY = 0.0f,
				                        float radius = 100.0f, float intensity = 1.0f)
				{
					PointLight2D light;
					light.position  = Dia::Maths::Vector2D(posX, posY);
					light.radius    = radius;
					light.intensity = intensity;
					mLastId = Dia::Core::StringCRC(id);
					mRegistry.Register(mLastId, light);
					return *this;
				}

				LightBuilder& OnLayers(uint32_t mask)
				{
					mRegistry.Get(mLastId).layerMask = mask;
					return *this;
				}

				LightBuilder& Disabled()
				{
					mRegistry.Get(mLastId).enabled = false;
					return *this;
				}

				LightRegistry2D& Registry() { return mRegistry; }

			private:
				LightRegistry2D      mRegistry;
				Dia::Core::StringCRC mLastId;
			};

		} // namespace Testing
	} // namespace Lighting2D
} // namespace Dia
