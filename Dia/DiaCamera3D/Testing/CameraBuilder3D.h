////////////////////////////////////////////////////////////////////////////////
// Filename: CameraBuilder3D.h - Fluent builder for test camera setup
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCamera3D/Camera3D.h>
#include <DiaCamera3D/Registry/CameraRegistry3D.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Camera3D::Testing
{
    /// Fluent builder for constructing CameraRegistry3D in tests.
    class CameraBuilder3D
    {
    public:
        CameraBuilder3D& WithCamera(const char* id,
                                    float posX = 0.0f, float posY = 0.0f, float posZ = 0.0f)
        {
            Dia::Core::StringCRC crc(id);
            Camera3D cam;
            cam.position = Dia::Maths::Vector3D(posX, posY, posZ);
            mRegistry.Register(crc, cam);
            mLastId = crc;
            return *this;
        }

        CameraBuilder3D& AsActive()
        {
            mRegistry.SetActive(mLastId);
            return *this;
        }

        CameraBuilder3D& WithBehaviour(ICameraBehaviour3D* behaviour)
        {
            mRegistry.AttachBehaviour(mLastId, behaviour);
            return *this;
        }

        /// Set perspective projection on the last registered camera
        CameraBuilder3D& Perspective(float fovY = 60.0f, float nearZ = 0.1f, float farZ = 1000.0f)
        {
            Camera3D& cam = mRegistry.Get(mLastId);
            cam.projectionType = ProjectionType::Perspective;
            cam.perspective.fovY  = fovY;
            cam.perspective.nearZ = nearZ;
            cam.perspective.farZ  = farZ;
            return *this;
        }

        /// Set orthographic projection on the last registered camera
        CameraBuilder3D& Orthographic(float width = 10.0f, float height = 10.0f,
                                       float nearZ = -100.0f, float farZ = 100.0f)
        {
            Camera3D& cam = mRegistry.Get(mLastId);
            cam.projectionType = ProjectionType::Orthographic;
            cam.ortho.width  = width;
            cam.ortho.height = height;
            cam.ortho.nearZ  = nearZ;
            cam.ortho.farZ   = farZ;
            return *this;
        }

        CameraRegistry3D& Registry() { return mRegistry; }

    private:
        CameraRegistry3D     mRegistry;
        Dia::Core::StringCRC mLastId;
    };

} // namespace Dia::Camera3D::Testing
