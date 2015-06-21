////////////////////////////////////////////////////////////////////////////////
// Filename: Frame.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Frame/FrameData.h"

namespace Dia
{
	namespace Graphics
	{
		FrameData::FrameData()
		{}

		void FrameData::Clear()
		{
			DebugFrameData::ClearDebugBuffer();
		}

		void FrameData::Copy(const FrameData& rhs)
		{
			DebugFrameData::CopyDebugBuffer(rhs);
		}
	}
}