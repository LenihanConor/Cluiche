////////////////////////////////////////////////////////////////////////////////
// Filename: ConsoleGamepadManager.h: Files to control a Xbox360 gamepad
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaInput/ConsoleGamepad.h"
#include "DiaInput/IInputSource.h"

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/Arrays/ArrayC.h>
namespace Dia
{
	namespace Input
	{
		////////////////////////////////////////////////////////////////////////////////
		// Enum name: DummyClass
		////////////////////////////////////////////////////////////////////////////////
		class ConsoleGamepadManager : public  Dia::Input::IInputSource
		{
		public:
			typedef Dia::Core::Containers::DynamicArrayC<ConsoleGamepad*, 8> ConsoleGamepadActiveList;
			typedef Dia::Core::Containers::ArrayC<ConsoleGamepad, 8> ConsoleGamepadPool;
			
			ConsoleGamepadManager();

			virtual void StartFrame()override;
			virtual void Poll(EventData& outStream)override;;
			virtual void EndFrame()override;

			bool IsRegisteredAsActive(const ConsoleGamepad& gamepad)const;

		private:
			ConsoleGamepadActiveList mGamepadActiveList;
			ConsoleGamepadPool mGamepadPool;
		};
	}
}