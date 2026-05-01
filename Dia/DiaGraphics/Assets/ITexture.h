////////////////////////////////////////////////////////////////////////////////
// Filename: ITexture.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Graphics
	{
		////////////////////////////////////////////////////////////
		/// \brief Interface for texture objects
		///
		/// A texture represents an image that can be mapped onto shapes.
		/// This interface provides backend-agnostic access to texture
		/// properties and allows retrieval of the native handle for
		/// direct backend usage.
		////////////////////////////////////////////////////////////
		class ITexture
		{
		public:
			virtual ~ITexture() {}

			////////////////////////////////////////////////////////////
			/// \brief Return the size of the texture
			///
			/// \return Size in pixels
			////////////////////////////////////////////////////////////
			virtual Maths::Vector2D GetSize() const = 0;

			////////////////////////////////////////////////////////////
			/// \brief Get the native backend texture handle
			///
			/// Returns a pointer to the underlying backend texture object
			/// (e.g., sf::Texture* for SFML backend). Use with caution.
			///
			/// \return Native texture handle (backend-specific)
			////////////////////////////////////////////////////////////
			virtual const void* GetNativeHandle() const = 0;

		protected:
			ITexture() {}

		private:
			ITexture(const ITexture&);
			ITexture& operator=(const ITexture&);
		};
	}
}
