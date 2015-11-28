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

		FrameData& FrameData::operator=(const FrameData& rhs)
		{
			Copy(rhs);

			return *this;
		}

		void FrameData::Clear()
		{
			DebugFrameData::ClearDebugBuffer();
			UIFrameData::ClearUIBuffer();
		}

		void FrameData::Copy(const FrameData& rhs)
		{
			DebugFrameData::CopyDebugBuffer(rhs);
			UIFrameData::CopyUIBuffer(rhs);
		}
	}
}