////////////////////////////////////////////////////////////////////////////////
// Filename: InputSourceManager.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaInput/EventStream.h"
#include "DiaInput/IInputSource.h"

namespace Dia
{
	namespace Input
	{
		////////////////////////////////////////////////////////////////////////////////
		// Enum name: InputSourceManager
		////////////////////////////////////////////////////////////////////////////////
		class InputSourceManager
		{
		public:
			typedef Dia::Core::Containers::DynamicArrayC<IInputSource*, 8> InputSourceList;

			InputSourceManager();

			void AddInputSource(IInputSource* inputSource);
			void RemoveInputSource(IInputSource* inputSource);

			void StartFrame();
			void Update(EventStream& outStream);
			void EndFrame();
			
		private:
			InputSourceList mInputSourceList;
		};
	}
}