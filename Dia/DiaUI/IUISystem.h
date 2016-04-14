//////////////////
#pragma once

#include <DiaInput/EMouseButton.h>

#include <DiaCore/Strings/String64.h>

namespace Dia
{
	namespace UI
	{
		class UIDataBuffer;
		class Page;

		class IUISystem
		{
		public:
			IUISystem() {};
			virtual ~IUISystem() {};

			virtual void Initialize() = 0;

			virtual void LoadPage(Page& newPage) = 0;
			virtual void UnloadPage() = 0;
			virtual bool IsPageLoaded()const = 0;

			virtual void Update() = 0;

			virtual void FetchUIDataBuffer(UIDataBuffer& outBuffer)const = 0;

			//Input
			virtual void InjectMouseMove(int x, int y) = 0;
			virtual void InjectMouseDown(Dia::Input::EMouseButton button, int x, int y) = 0;
			virtual void InjectMouseUp(Dia::Input::EMouseButton button, int x, int y) = 0;
			virtual void InjectMouseClick(Dia::Input::EMouseButton button, int x, int y) = 0;
			virtual void InjectMouseWheel(int scroll_vert, int scroll_horz) = 0;
		};
	}
}