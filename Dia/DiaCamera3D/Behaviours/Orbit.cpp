////////////////////////////////////////////////////////////////////////////////
// Filename: Orbit.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera3D/Behaviours/Orbit.h"
#include "DiaCamera3D/Registry/CameraBehaviourRegistry3D.h"
#include "DiaCamera3D/Camera3D.h"
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Quaternion/Quaternion.h>
#include <cmath>

namespace Dia
{
    namespace Camera3D
    {
        static bool sRegistered = [] {
            CameraBehaviourRegistry3D::Get().Register(
                Dia::Core::StringCRC(Orbit::kTypeIdStr),
                [](const void*) -> ICameraBehaviour3D* { return new Orbit(); });
            return true;
        }();

        ////////////////////////////////////////////////////////////
        Orbit::Orbit(float initialRadius, float initialYaw, float initialPitch)
            : mRadius(initialRadius)
            , mYaw(initialYaw)
            , mPitch(initialPitch)
        {
        }

        ////////////////////////////////////////////////////////////
        Dia::Core::StringCRC Orbit::GetTypeId() const
        {
            return Dia::Core::StringCRC(kTypeIdStr);
        }

        ////////////////////////////////////////////////////////////
        void Orbit::SetInput(float deltaYaw, float deltaPitch, float deltaRadius)
        {
            mDeltaYaw    = deltaYaw;
            mDeltaPitch  = deltaPitch;
            mDeltaRadius = deltaRadius;
        }

        ////////////////////////////////////////////////////////////
        void Orbit::Update(Camera3D& camera, float /*dt*/)
        {
            // 1. Accumulate deltas
            mYaw    += mDeltaYaw;
            mPitch  += mDeltaPitch;
            mRadius += mDeltaRadius;

            // 2. Wrap yaw into (-π, π]
            const float kPi     = 3.14159265358979323846f;
            const float kTwoPi  = 2.0f * kPi;
            while (mYaw >  kPi)  mYaw -= kTwoPi;
            while (mYaw < -kPi)  mYaw += kTwoPi;

            // 3. Clamp pitch to avoid gimbal lock at poles
            const float kEps       = 0.01f;
            const float kPitchMax  =  kPi * 0.5f - kEps;
            const float kPitchMin  = -kPi * 0.5f + kEps;
            if (mPitch >  kPitchMax) mPitch =  kPitchMax;
            if (mPitch <  kPitchMin) mPitch =  kPitchMin;

            // 4. Clamp radius
            if (mRadius < 0.1f) mRadius = 0.1f;

            // 5. Zero deltas
            mDeltaYaw    = 0.0f;
            mDeltaPitch  = 0.0f;
            mDeltaRadius = 0.0f;

            // 6. Compute position from spherical coords (right-handed, Y-up)
            const float cosPitch = cosf(mPitch);
            const float sinPitch = sinf(mPitch);
            const float cosYaw   = cosf(mYaw);
            const float sinYaw   = sinf(mYaw);

            camera.position = Dia::Maths::Vector3D(
                mTarget.x + mRadius * cosPitch * sinYaw,
                mTarget.y + mRadius * sinPitch,
                mTarget.z + mRadius * cosPitch * cosYaw);

            // 7. Aim camera toward target
            const Dia::Maths::Vector3D forward = (mTarget - camera.position).AsNormal();
            const Dia::Maths::Vector3D up(0.0f, 1.0f, 0.0f);
            camera.orientation = Dia::Maths::Quaternion::LookRotation(forward, up);
        }

    } // namespace Camera3D
} // namespace Dia
