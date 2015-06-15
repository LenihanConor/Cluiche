////////////////////////////////////////////////////////////////////////////////
// Filename: ApplicationFrameControl.h
////////////////////////////////////////////////////////////////////////////////
#ifndef _APPLICATIONFRAMECONTROL_H_
#define _APPLICATIONFRAMECONTROL_H_

#include <DiaApplication/ApplicationStateObject.h>

#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/CRCHashFunctor.h>
#include <DiaCore/Time/TimeRelative.h>

namespace Dia
{
	namespace Application
	{		
		////////////////////////////////////////////////////////////////////////////////
		// Class name: ApplicationFrameControl - Applicaiton Level system to control frame rate of a PU
		////////////////////////////////////////////////////////////////////////////////
		class FrameControl
		{
		public:
			////////////////////////////////////////////////////////////////////////////////
			// Enum name: BlockEnum, Should we block or continue on as fast as can be
			////////////////////////////////////////////////////////////////////////////////
			CLASSEDENUM(BlockEnum, \
				CE_ITEMVAL(kBlock, 0)\
				CE_ITEM(kContinue)
				, kContinue \
				);

			FrameControl(float idealFrameRate, BlockEnum willBlock);

			void StartFrame(const Dia::Core::TimeRelative& currentSystemTime);
			void EndFrame(const Dia::Core::TimeRelative& currentSystemTime);
			
			bool IsBlockIfTooFast()const { return mBlockIfTooFast == BlockEnum::kBlock; }	
			unsigned int GetFrameNumber()const { return mFrameNumber; }
			float GetIdealFramesPerSecond()const { return mIdealFramesPerSecond; }
			float GetCurrentFramesPerSecond()const { return mCurrentFramesPerSecond; }	
			
			void SetBlockIfTooFast(BlockEnum block){ mBlockIfTooFast = block; }
			void SetIdealFramesPerSecond(float idealFrameRate){ mIdealFramesPerSecond = idealFrameRate; }

		private:
			BlockEnum mBlockIfTooFast;		// Flag to enable blocking if we finish a frame to fast
			unsigned int mFrameNumber;		// Incrementing number from 0 upwards
			float mIdealFramesPerSecond;	// What speed should the frame be going
			float mCurrentFramesPerSecond;	// How many frames per second we are at if we did not block
		};
	}
}

#endif