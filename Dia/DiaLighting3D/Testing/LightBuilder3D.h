////////////////////////////////////////////////////////////////////////////////
// Filename: LightBuilder3D.h — Fluent builder for test light setup
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaLighting3D/Registry/LightRegistry3D.h>
#include <DiaLighting3D/PointLight3D.h>
#include <DiaLighting3D/DirectionalLight3D.h>
#include <DiaLighting3D/SpotLight3D.h>
#include <DiaLighting3D/AmbientLight3D.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Lighting3D { namespace Testing {

/// Fluent builder for constructing LightRegistry3D in tests.
class LightBuilder3D
{
public:
	// --- Point lights ---
	LightBuilder3D& WithPoint(const char* id,
	                            float posX = 0.0f, float posY = 0.0f, float posZ = 0.0f,
	                            float radius = 10.0f, float intensity = 1.0f)
	{
		PointLight3D light;
		light.position  = Dia::Maths::Vector3D(posX, posY, posZ);
		light.radius    = radius;
		light.intensity = intensity;
		mLastPointId = Dia::Core::StringCRC(id);
		mRegistry.RegisterPoint(mLastPointId, light);
		return *this;
	}

	LightBuilder3D& PointDisabled()
	{
		mRegistry.GetPoint(mLastPointId).enabled = false;
		return *this;
	}

	// --- Directional lights ---
	LightBuilder3D& WithDirectional(const char* id,
	                                  float dirX = 0.0f, float dirY = -1.0f, float dirZ = 0.0f,
	                                  float intensity = 1.0f)
	{
		DirectionalLight3D light;
		light.direction = Dia::Maths::Vector3D(dirX, dirY, dirZ);
		light.intensity = intensity;
		mLastDirectionalId = Dia::Core::StringCRC(id);
		mRegistry.RegisterDirectional(mLastDirectionalId, light);
		return *this;
	}

	LightBuilder3D& DirectionalDisabled()
	{
		mRegistry.GetDirectional(mLastDirectionalId).enabled = false;
		return *this;
	}

	// --- Spot lights ---
	LightBuilder3D& WithSpot(const char* id,
	                           float posX = 0.0f, float posY = 0.0f, float posZ = 0.0f,
	                           float innerAngle = 15.0f, float outerAngle = 30.0f,
	                           float intensity = 1.0f)
	{
		SpotLight3D light;
		light.position   = Dia::Maths::Vector3D(posX, posY, posZ);
		light.innerAngle = innerAngle;
		light.outerAngle = outerAngle;
		light.intensity  = intensity;
		mLastSpotId = Dia::Core::StringCRC(id);
		mRegistry.RegisterSpot(mLastSpotId, light);
		return *this;
	}

	LightBuilder3D& SpotDisabled()
	{
		mRegistry.GetSpot(mLastSpotId).enabled = false;
		return *this;
	}

	// --- Ambient ---
	LightBuilder3D& WithAmbient(float intensity = 0.1f)
	{
		AmbientLight3D light;
		light.intensity = intensity;
		mRegistry.SetAmbient(light);
		return *this;
	}

	LightRegistry3D& Registry() { return mRegistry; }

private:
	LightRegistry3D      mRegistry;
	Dia::Core::StringCRC mLastPointId;
	Dia::Core::StringCRC mLastDirectionalId;
	Dia::Core::StringCRC mLastSpotId;
};

} } }
