////////////////////////////////////////////////////////////////////////////////
// Filename: DebugFrame.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Core/Assert.h>

#include "DiaGraphics/Frame/DebugFrameDataCircle2D.h"
#include "DiaGraphics/Frame/DebugFrameDataLine2D.h"

namespace Dia
{
	namespace Graphics
	{
		// Forward declerations
		class DebugFrameDataVisitor;

		///
		/// DebugFrameData - A single frame that stores all the info that the render that is debug only
		///
		class DebugFrameData
		{
		public:
			DebugFrameData();
			~DebugFrameData();

			void RemoveAllDebugBuffer();

			void RequestDraw(const DebugFrameDataCircle2D& object);
			void RequestDraw(const DebugFrameDataLine2D& object);

			void AcceptVisitor(const DebugFrameDataVisitor& visitor)const;

		private:
			Core::Containers::DynamicArrayC<DebugFrameDataCircle2D, 16> mDebug2DCircleBuffer;
			Core::Containers::DynamicArrayC<DebugFrameDataLine2D, 16> mDebug2DLineBuffer;
		};
	}
}