////////////////////////////////////////////////////////////////////////////////
// Filename: DebugFrameDataVisitor.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/Frame/DebugPrimitive.h>

namespace Dia
{
	namespace Graphics
	{
		// Forward declarations
		class DebugFrameData;

		///
		/// DebugFrameDataVisitor - Implemented by renderers and test recorders to consume debug primitives
		///
		class DebugFrameDataVisitor
		{
		public:
			DebugFrameDataVisitor() {}
			virtual ~DebugFrameDataVisitor() {}

			virtual void Visit(const DebugPrimitive& primitive) const = 0;
			virtual void Visit(const DebugFrameData& frameData) const = 0;
		};
	}
}
