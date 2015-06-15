////////////////////////////////////////////////////////////////////////////////
// Filename: DebugFrameDataCircle2D.h
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
		/// DebugFrameDataCircle2D - Debug frame data for a 2D circle
		///
		class DebugFrameDataCircle2D : public DebugFrameDataBase
		{
		public:
			DIA_TYPE_DECLARATION;

			DebugFrameDataCircle2D() : DebugFrameDataBase(), mPosition(), mRadius(1.0f), mColour(RGBA::White) {};
			DebugFrameDataCircle2D(const Maths::Vector2D& pos, float radius, RGBA colour = RGBA::White) : DebugFrameDataBase(), mPosition(pos), mRadius(radius), mColour(colour){};

			// Rendering Interface
			virtual void AcceptVisitor(const DebugFrameDataVisitor& visitor)const override;

			// Accesor Functions
			const Maths::Vector2D& GetPosition()const { return mPosition; }
			float GetRadius()const { return mRadius; }
			RGBA GetColour()const { return mColour; }

		private:
			Maths::Vector2D mPosition;
			float mRadius;
			RGBA mColour;
		};
	}
}
