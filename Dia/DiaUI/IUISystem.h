//////////////////
#pragma once

#include "DiaUI/UIDataBuffer.h"

#include <DiaInput/EMouseButton.h>
#include <DiaCore/Strings/String256.h>
#include <DiaCore/Strings/String64.h>

namespace Dia
{
	namespace UI
	{
		class Page
		{
		public:
			Page() {};
			Page(const Dia::Core::Containers::String256& url)
				: mUrl(url)
			{}

			virtual ~Page() {};

			virtual void BindMethods(IUISystem* parentSystem) = 0;
			const Dia::Core::Containers::String256& GetUrl()const { return mUrl;  };

		private:
			Dia::Core::Containers::String256 mUrl;
		};

		class IUISystem
		{
		public:
			IUISystem() {};
			virtual ~IUISystem() {};

			virtual void Initialize() = 0;

			virtual void LoadPage(const Page& newPage) = 0;
			virtual void OnLoadedPage() = 0;
			virtual void IsLoadingPage() = 0;
			virtual void UnloadPage() = 0;

			virtual void Update() = 0;

			virtual void FetchUIDataBuffer(UIDataBuffer& outBuffer)const = 0;

			void BindMethod(const Dia::Core::Containers::String64& methodName, );

			//Input
			virtual void InjectMouseMove(int x, int y) = 0;
			virtual void InjectMouseDown(Dia::Input::EMouseButton button, int x, int y) = 0;
			virtual void InjectMouseUp(Dia::Input::EMouseButton button, int x, int y) = 0;
			virtual void InjectMouseClick(Dia::Input::EMouseButton button, int x, int y) = 0;
			virtual void InjectMouseWheel(int scroll_vert, int scroll_horz) = 0;
		};
	}
}