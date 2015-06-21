////////////////////////////////////////////////////////////////////////////////
// Filename: Frame.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Frame/DebugFrameData.h"

#include "DiaGraphics/Frame/DebugFrameDataVisitor.h"

namespace Dia
{
	namespace Graphics
	{
		//------------------------------------------------------------------------------
		DebugFrameData::DebugFrameData()
		{}
		
		//------------------------------------------------------------------------------
		DebugFrameData::~DebugFrameData()
		{}

		//------------------------------------------------------------------------------
		void DebugFrameData::ClearDebugBuffer()
		{
			mDebug2DCircleBuffer.RemoveAll();
			mDebug2DLineBuffer.RemoveAll();
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::CopyDebugBuffer(const DebugFrameData& rhs)
		{
			mDebug2DCircleBuffer = rhs.mDebug2DCircleBuffer;
			mDebug2DLineBuffer = rhs.mDebug2DLineBuffer;
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::RequestDraw(const DebugFrameDataCircle2D& object)
		{
			mDebug2DCircleBuffer.Add(object);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::RequestDraw(const DebugFrameDataLine2D& object)
		{
			mDebug2DLineBuffer.Add(object);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::AcceptVisitor(const DebugFrameDataVisitor& visitor)const
		{
			//Todo: This should be sequentially in order of when they got added or at least in some sort of batch
			for (size_t i = 0; i < mDebug2DCircleBuffer.Size(); i++)
			{
				visitor.Visit(mDebug2DCircleBuffer[i]);
			}

			for (size_t i = 0; i < mDebug2DLineBuffer.Size(); i++)
			{
				visitor.Visit(mDebug2DLineBuffer[i]);
			}
		}
	}
}