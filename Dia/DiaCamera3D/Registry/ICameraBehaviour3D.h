////////////////////////////////////////////////////////////////////////////////
// Filename: ICameraBehaviour3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Camera3D { struct Camera3D; } }

namespace Dia
{
	namespace Camera3D
	{
		////////////////////////////////////////////////////////////
		/// \brief Interface for composable 3D camera behaviours.
		///
		/// Each behaviour is attached to a named camera in CameraRegistry3D.
		/// Behaviours tick in attachment order each frame via Update(dt).
		/// Behaviours expose setters for application-driven input — they do
		/// not read input directly.
		////////////////////////////////////////////////////////////
		class ICameraBehaviour3D
		{
		public:
			virtual ~ICameraBehaviour3D() = default;

			virtual Dia::Core::StringCRC GetTypeId() const = 0;
			virtual void Update(Camera3D& camera, float dt) = 0;
		};

	} // namespace Camera3D
} // namespace Dia
