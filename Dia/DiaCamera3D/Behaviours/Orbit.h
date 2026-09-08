////////////////////////////////////////////////////////////////////////////////
// Filename: Orbit.h
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "DiaCamera3D/Registry/ICameraBehaviour3D.h"
#include <DiaMaths/Vector/Vector3D.h>

namespace Dia
{
    namespace Camera3D
    {
        ////////////////////////////////////////////////////////////
        /// \brief Orbits the camera around a target point using spherical coordinates.
        ///        Call SetTarget() to set the pivot.
        ///        Call SetInput() each frame with delta yaw/pitch/radius.
        ///        Never reads DiaInput directly — application drives this via setters.
        ////////////////////////////////////////////////////////////
        class Orbit : public ICameraBehaviour3D
        {
        public:
            Orbit(float initialRadius = 10.0f, float initialYaw = 0.0f, float initialPitch = 0.3f);

            Dia::Core::StringCRC GetTypeId() const override;
            void Update(Camera3D& camera, float dt) override;

            void SetTarget(const Dia::Maths::Vector3D& target) { mTarget = target; }
            // Accumulate input deltas each frame. Applied then zeroed in Update.
            void SetInput(float deltaYaw, float deltaPitch, float deltaRadius);

            static constexpr const char* kTypeIdStr = "Orbit";

        private:
            Dia::Maths::Vector3D mTarget;
            float mYaw    = 0.0f;   // radians, wraps
            float mPitch  = 0.3f;   // radians, clamped [−π/2+ε, π/2−ε]
            float mRadius = 10.0f;  // must stay > 0
            float mDeltaYaw    = 0.0f;
            float mDeltaPitch  = 0.0f;
            float mDeltaRadius = 0.0f;
        };

    } // namespace Camera3D
} // namespace Dia
