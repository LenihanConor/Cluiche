////////////////////////////////////////////////////////////////////////////////
// Filename: Frame.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Frame/DebugFrameDataLine2D.h"

#include "DiaGraphics/Frame/DebugFrameDataVisitor.h"

namespace Dia
{
	namespace Graphics
	{
		//------------------------------------------------------------------------------
		DIA_TYPE_DEFINITION(DebugFrameDataLine2D)
			DIA_TYPE_ADD_VARIABLE("Position1", mPosition1)
			DIA_TYPE_ADD_VARIABLE("Position2", mPosition2)
			DIA_TYPE_ADD_VARIABLE("Colour", mColour)
		DIA_TYPE_DEFINITION_END()

		//------------------------------------------------------------------------------
		void DebugFrameDataLine2D::AcceptVisitor(const DebugFrameDataVisitor& visitor)const
		{
			visitor.Visit(*this);
		}
	}
}