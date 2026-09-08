////////////////////////////////////////////////////////////////////////////////
// Filename: ICameraBehaviour.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Camera2D { class Camera2D; } }

namespace Dia
{
	namespace Camera2D
	{
		////////////////////////////////////////////////////////////
		/// \brief Interface for composable camera behaviours.
		///
		/// Each behaviour is attached to a named camera in CameraRegistry2D.
		/// Behaviours tick in attachment order each frame via UpdateAll(dt).
		/// Behaviours expose setters for application-driven input — they do
		/// not read input directly.
		////////////////////////////////////////////////////////////
		class ICameraBehaviour
		{
		public:
			virtual ~ICameraBehaviour() = default;

			virtual Dia::Core::StringCRC GetTypeId() const = 0;
			virtual void Update(Camera2D& camera, float dt) = 0;
		};

	} // namespace Camera2D
} // namespace Dia
