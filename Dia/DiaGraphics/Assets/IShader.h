////////////////////////////////////////////////////////////////////////////////
// Filename: IShader.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
	namespace Graphics
	{
		////////////////////////////////////////////////////////////
		/// \brief Interface for shader objects
		///
		/// Shaders are programs written in GLSL (or equivalent) that
		/// run on the GPU to customize rendering. This interface provides
		/// backend-agnostic access to shader functionality.
		////////////////////////////////////////////////////////////
		class IShader
		{
		public:
			virtual ~IShader() {}

			////////////////////////////////////////////////////////////
			/// \brief Get the native backend shader handle
			///
			/// Returns a pointer to the underlying backend shader object
			/// (e.g., sf::Shader* for SFML backend). Use with caution.
			///
			/// \return Native shader handle (backend-specific)
			////////////////////////////////////////////////////////////
			virtual const void* GetNativeHandle() const = 0;

		protected:
			IShader() {}

		private:
			IShader(const IShader&);
			IShader& operator=(const IShader&);
		};
	}
}
