////////////////////////////////////////////////////////////////////////////////
// Filename: Frame.h
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "DebugFrameData.h"

namespace Dia
{
	namespace Graphics
	{
		///
		/// Frame - A single frame that stores all the info that the render
		///
		class FrameData: public DebugFrameData
		{
		public:
			FrameData();
		};
	}
}