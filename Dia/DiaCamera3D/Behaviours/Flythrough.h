////////////////////////////////////////////////////////////////////////////////
// Filename: Flythrough.h
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "DiaCamera3D/Registry/ICameraBehaviour3D.h"
#include <DiaMaths/Vector/Vector3D.h>

namespace Dia
{
    namespace Camera3D
    {
        ////////////////////////////////////////////////////////////
        /// \brief Free-look fly camera. SetLookInput(yaw, pitch) drives orientation.
        ///        SetMoveInput(Vector3D) drives position in camera-local space.
        ///        Never reads DiaInput directly — application drives this via setters.
        ////////////////////////////////////////////////////////////
        class Flythrough : public ICameraBehaviour3D
        {
        public:
            explicit Flythrough(float moveSpeed = 5.0f, float lookSensitivity = 1.0f);

            Dia::Core::StringCRC GetTypeId() const override;
            void Update(Camera3D& camera, float dt) override;

            // Accumulated yaw/pitch input (radians per frame). Applied then zeroed.
            void SetLookInput(float deltaYaw, float deltaPitch);
            // Movement input in camera-local space (forward/right/up). Applied scaled by dt*moveSpeed.
            void SetMoveInput(Dia::Maths::Vector3D localMove);

            static constexpr const char* kTypeIdStr = "Flythrough";

        private:
            float mMoveSpeed       = 5.0f;
            float mLookSensitivity = 1.0f;
            float mYaw             = 0.0f;  // radians
            float mPitch           = 0.0f;  // radians, clamped
            Dia::Maths::Vector3D mMoveInput;
            float mDeltaYaw   = 0.0f;
            float mDeltaPitch = 0.0f;
        };

    } // namespace Camera3D
} // namespace Dia
