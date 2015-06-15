////////////////////////////////////////////////////////////////////////////////
// Filename: DebugFrameDataVisitor.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
	namespace Graphics
	{
		// Forward declerations
		class DebugFrameDataCircle2D;
		class DebugFrameDataLine2D;
		class DebugFrameData;

		///
		/// DebugFrameDataVisitor - Used my external systems to interface with frame data
		///
		class DebugFrameDataVisitor
		{
		public:
			DebugFrameDataVisitor(){};
			virtual ~DebugFrameDataVisitor(){};

			virtual void Visit(const DebugFrameDataCircle2D& object)const = 0;
			virtual void Visit(const DebugFrameDataLine2D& object)const = 0;
			virtual void Visit(const DebugFrameData& object)const = 0;
		};
	}
}