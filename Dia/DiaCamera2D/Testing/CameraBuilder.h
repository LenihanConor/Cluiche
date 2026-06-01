////////////////////////////////////////////////////////////////////////////////
// Filename: CameraBuilder.h — Fluent builder for test camera setup
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCamera2D/Camera2D.h>
#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Camera2D
	{
		namespace Testing
		{
			/// Fluent builder for constructing CameraRegistry2D in tests.
			class CameraBuilder
			{
			public:
				CameraBuilder& WithCamera(const char* id,
				                         float posX = 0.0f, float posY = 0.0f,
				                         float zoom = 1.0f, float rotation = 0.0f)
				{
					Dia::Core::StringCRC crc(id);
					Camera2D cam(Dia::Maths::Vector2D(posX, posY), zoom, rotation);
					mRegistry.Register(crc, cam);
					mLastId = crc;
					return *this;
				}

				CameraBuilder& AsActive()
				{
					mRegistry.SetActive(mLastId);
					return *this;
				}

				CameraBuilder& WithBehaviour(ICameraBehaviour* behaviour)
				{
					mRegistry.AttachBehaviour(mLastId, behaviour);
					return *this;
				}

				CameraRegistry2D& Registry() { return mRegistry; }

			private:
				CameraRegistry2D     mRegistry;
				Dia::Core::StringCRC mLastId;
			};

		} // namespace Testing
	} // namespace Camera2D
} // namespace Dia
