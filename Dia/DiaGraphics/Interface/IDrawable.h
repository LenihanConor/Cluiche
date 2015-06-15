////////////////////////////////////////////////////////////////////////////////
// Filename: IDrawable.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
	namespace Graphics
	{
		class IRenderTarget;

		////////////////////////////////////////////////////////////
		// Abstract base class for objects that can be draw to a render target
		////////////////////////////////////////////////////////////
		class IDrawable
		{
		public:
			virtual ~IDrawable() {}

		protected:

			friend class IRenderTarget;

			////////////////////////////////////////////////////////////
			/// \brief Draw the object to a render target
			///
			/// This is a pure virtual function that has to be implemented
			/// by the derived class to define how the drawable should be
			/// drawn.
			///
			/// \param target Render target to draw to
			/// \param states Current render states
			///
			////////////////////////////////////////////////////////////
			virtual void Draw(IRenderTarget& target, RenderStates states) const = 0;
		};
	}
}