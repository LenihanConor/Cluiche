////////////////////////////////////////////////////////////////////////////////
// Filename: ApplicationFrameControl.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplication/ApplicationFrameControl.h"

namespace Dia
{
	namespace Application
	{
		////////////////////////////////////////////////////////////////////////////////
		// Class name: Module
		////////////////////////////////////////////////////////////////////////////////
		
		//-----------------------------------------------------------------------------
		FrameControl::FrameControl(float idealFrameRate, BlockEnum willBlock)
			: mBlockIfTooFast(willBlock)
			, mFrameNumber(0)
			, mIdealFramesPerSecond(idealFrameRate)
		{

		}

		//-----------------------------------------------------------------------------
		void FrameControl::StartFrame(const Dia::Core::TimeRelative& currentSystemTime)
		{

		}

		//-----------------------------------------------------------------------------
		void FrameControl::EndFrame(const Dia::Core::TimeRelative& currentSystemTime)
		{

		}
	}
}