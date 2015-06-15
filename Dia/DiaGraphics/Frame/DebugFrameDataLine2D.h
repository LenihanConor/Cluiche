////////////////////////////////////////////////////////////////////////////////
// Filename: DebugFrameDataLine2D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Type/TypeDefinitionMacros.h>

#include "DiaGraphics/Misc/RGBA.h"

#include "DiaGraphics/Frame/DebugFrameDataBase.h"

namespace Dia
{
	namespace Graphics
	{
		// Forward declerations
		class DebugFrameDataVisitor;

		///
		/// DebugFrameDataLine2D - Debug frame data for a 2D circle
		///
		class DebugFrameDataLine2D : public DebugFrameDataBase
		{
		public:
			DIA_TYPE_DECLARATION;

			DebugFrameDataLine2D() : DebugFrameDataBase(), mPosition1(), mPosition2(10.0f, 10.0f), mColour(RGBA::White) {};
			DebugFrameDataLine2D(const Maths::Vector2D& pos1, const Maths::Vector2D& pos2, RGBA colour = RGBA::White) : DebugFrameDataBase(), mPosition1(pos1), mPosition2(pos2), mColour(colour){};

			// Rendering Interface
			virtual void AcceptVisitor(const DebugFrameDataVisitor& visitor)const override;

			// Accesor Functions
			const Maths::Vector2D& GetPosition1()const { return mPosition1; }
			const Maths::Vector2D& GetPosition2()const { return mPosition2; }
			RGBA GetColour()const { return mColour; }

		private:
			Maths::Vector2D mPosition1;
			Maths::Vector2D mPosition2;
			RGBA mColour;
		};
	}
}
