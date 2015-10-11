////////////////////////////////////////////////////////////////////////////////
// Filename: Frame.h
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "DebugFrameData.h"
#include "UIFrameData.h"

namespace Dia
{
	namespace Graphics
	{
		///
		/// Frame - A single frame that stores all the info that the render
		///
		class FrameData: public DebugFrameData, public UIFrameData
		{
		public:
			FrameData();

			FrameData& operator=(const FrameData& rhs);

			void Clear();
			void Copy(const FrameData& rhs);
		};
	}
}