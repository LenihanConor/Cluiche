////////////////////////////////////////////////////////////////////////////////
// Filename: Flythrough.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera3D/Behaviours/Flythrough.h"
#include "DiaCamera3D/Registry/CameraBehaviourRegistry3D.h"
#include "DiaCamera3D/Camera3D.h"
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Quaternion/Quaternion.h>
#include <DiaMaths/Core/Angle.h>

namespace Dia
{
    namespace Camera3D
    {
        static bool sRegistered = [] {
            CameraBehaviourRegistry3D::Get().Register(
                Dia::Core::StringCRC(Flythrough::kTypeIdStr),
                [](const void*) -> ICameraBehaviour3D* { return new Flythrough(); });
            return true;
        }();

        ////////////////////////////////////////////////////////////
        Flythrough::Flythrough(float moveSpeed, float lookSensitivity)
            : mMoveSpeed(moveSpeed)
            , mLookSensitivity(lookSensitivity)
        {
        }

        ////////////////////////////////////////////////////////////
        Dia::Core::StringCRC Flythrough::GetTypeId() const
        {
            return Dia::Core::StringCRC(kTypeIdStr);
        }

        ////////////////////////////////////////////////////////////
        void Flythrough::SetLookInput(float deltaYaw, float deltaPitch)
        {
            mDeltaYaw   = deltaYaw;
            mDeltaPitch = deltaPitch;
        }

        ////////////////////////////////////////////////////////////
        void Flythrough::SetMoveInput(Dia::Maths::Vector3D localMove)
        {
            mMoveInput = localMove;
        }

        ////////////////////////////////////////////////////////////
        void Flythrough::Update(Camera3D& camera, float dt)
        {
            // 1. Accumulate look deltas scaled by sensitivity
            mYaw   += mDeltaYaw   * mLookSensitivity;
            mPitch += mDeltaPitch * mLookSensitivity;

            // 2. Clamp pitch to avoid gimbal lock at poles
            const float kPi      = 3.14159265358979323846f;
            const float kEps     = 0.01f;
            const float kPitchMax =  kPi * 0.5f - kEps;
            const float kPitchMin = -kPi * 0.5f + kEps;
            if (mPitch >  kPitchMax) mPitch =  kPitchMax;
            if (mPitch <  kPitchMin) mPitch =  kPitchMin;

            // 3. Rebuild orientation from yaw/pitch (YXZ intrinsic)
            camera.orientation = Dia::Maths::Quaternion::FromEuler(
                Dia::Maths::Angle::FromRadians(mYaw),
                Dia::Maths::Angle::FromRadians(mPitch),
                Dia::Maths::Angle::FromRadians(0.0f));

            // 4. Translate in camera-local space
            if (dt > 0.0f)
            {
                const Dia::Maths::Vector3D forward = camera.orientation.Rotate(Dia::Maths::Vector3D(0.0f,  0.0f, -1.0f));
                const Dia::Maths::Vector3D right   = camera.orientation.Rotate(Dia::Maths::Vector3D(1.0f,  0.0f,  0.0f));
                const Dia::Maths::Vector3D up      = camera.orientation.Rotate(Dia::Maths::Vector3D(0.0f,  1.0f,  0.0f));

                const float scale = dt * mMoveSpeed;
                camera.position = camera.position
                    + (forward * (mMoveInput.z * scale))
                    + (right   * (mMoveInput.x * scale))
                    + (up      * (mMoveInput.y * scale));
            }

            // 5. Zero inputs
            mDeltaYaw   = 0.0f;
            mDeltaPitch = 0.0f;
            mMoveInput  = Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f);
        }

    } // namespace Camera3D
} // namespace Dia
