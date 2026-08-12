////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaInput/EMouseButton.h>
#include <DiaInput/EKey.h>

#include <DiaCore/Strings/String64.h>

#include <functional>
#include <string>

namespace Dia { namespace Input { class InputRouter; } }

namespace Dia
{
	namespace UI
	{
		class UIDataBuffer;
		class Page;
		class IPage;

		class IUISystem
		{
		public:
			using JSHandler = std::function<std::string(const std::string& argsJson)>;

			IUISystem() {};
			virtual ~IUISystem() {};

			virtual void Initialize() = 0;
			virtual void Shutdown() = 0;

			virtual void LoadPage(Page& newPage) = 0;
			virtual void UnloadPage() = 0;
			virtual bool IsPageLoaded()const = 0;

			virtual void Update() = 0;

			virtual void FetchUIDataBuffer(UIDataBuffer& outBuffer)const = 0;

			virtual IPage* CreatePage(const char* url, int width, int height) = 0;
			virtual void DestroyPage(IPage* page) = 0;
			virtual int GetPageCount() const = 0;

			//Input
			virtual void InjectMouseMove(int x, int y) = 0;
			virtual void InjectMouseDown(Dia::Input::EMouseButton button, int x, int y) = 0;
			virtual void InjectMouseUp(Dia::Input::EMouseButton button, int x, int y) = 0;
			virtual void InjectMouseClick(Dia::Input::EMouseButton button, int x, int y) = 0;
			virtual void InjectMouseWheel(int scroll_vert, int scroll_horz) = 0;

			// Keyboard injection (default no-op: CEF handles keyboard natively).
			virtual void InjectKeyDown(Dia::Input::EKey /*key*/, int /*modifiers*/ = 0) {}
			virtual void InjectKeyUp(Dia::Input::EKey /*key*/, int /*modifiers*/ = 0)   {}
			virtual void InjectCharacterInput(uint32_t /*codepoint*/)                    {}

			// Input router wiring — UIModule calls this after constructing both objects.
			virtual void SetInputRouter(Dia::Input::InputRouter* /*router*/) {}

			// JavaScript <-> C++ bridge (optional; default no-op for UI systems without JS).
			// RegisterJSHandler binds a name that JS can invoke as window.dia.callCpp(name, argsJson).
			// CallJSFunction pushes a notification to JS: dia.<functionName>(argsJson).
			virtual void RegisterJSHandler(const char* /*name*/, JSHandler /*handler*/) {}
			virtual void CallJSFunction(const char* /*functionName*/, const char* /*argsJson*/) {}
		};
	}
}