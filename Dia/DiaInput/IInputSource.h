////////////////////////////////////////////////////////////////////////////////
// Filename: IInputSource.h: Interface to wrap the polling of input controller events
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaInput\EventStream.h"

namespace Dia
{
	namespace Input
	{
		////////////////////////////////////////////////////////////////////////////////
		// Enum name: IInputSource
		////////////////////////////////////////////////////////////////////////////////
		class IInputSource
		{
		public:
			IInputSource(){ static int sNextId = 0xFFFF; mId = sNextId++; };
			virtual ~IInputSource(){}
			
			virtual void StartFrame(){}
			virtual void Poll(EventStream& outStream) = 0;
			virtual void EndFrame(){}

			int GetUniqueID()const { return mId; }

		private:	
			int mId;
		};
	}
}