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

			virtual const void* GetNativeHandle() const = 0;

		protected:
			IShader() {}

		private:
			IShader(const IShader&);
			IShader& operator=(const IShader&);
		};
	}
}
