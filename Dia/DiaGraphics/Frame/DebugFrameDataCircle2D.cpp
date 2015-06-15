////////////////////////////////////////////////////////////////////////////////
// Filename: Frame.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Frame/DebugFrameDataCircle2D.h"

#include "DiaGraphics/Frame/DebugFrameDataVisitor.h"

namespace Dia
{
	namespace Graphics
	{
		//------------------------------------------------------------------------------
		DIA_TYPE_DEFINITION(DebugFrameDataCircle2D)
			DIA_TYPE_ADD_VARIABLE("Position", mPosition)
			DIA_TYPE_ADD_VARIABLE("Radius", mRadius)
			DIA_TYPE_ADD_VARIABLE("Colour", mColour)
		DIA_TYPE_DEFINITION_END()

		//------------------------------------------------------------------------------
		void DebugFrameDataCircle2D::AcceptVisitor(const DebugFrameDataVisitor& visitor)const
		{
			visitor.Visit(*this);
		}
	}
}