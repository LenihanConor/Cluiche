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
			// TODO - UI SHOULD BE SAVE AT A DIFFERENT FRAME RATE AS I DONT WANT IT BEING PLAYED BACK
			FrameData();

			FrameData& operator=(const FrameData& rhs);

			void Clear();
			void Copy(const FrameData& rhs);
		};
	}
}