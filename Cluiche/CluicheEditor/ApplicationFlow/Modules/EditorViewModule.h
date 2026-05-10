#pragma once

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaEditor/MVC/EditorView.h>
#include <DiaEditor/Sinks/EditorConsoleSink.h>

namespace Dia
{
	namespace Window { class IWindow; }
	namespace UI     { class IUISystem; }
}

namespace Cluiche
{
	namespace Editor
	{
		class EditorModelModule;
		class EditorViewControllerModule;
		class SplashScreenModule;

		class EditorViewModule : public Dia::ApplicationFlow::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit EditorViewModule(const Dia::Core::StringCRC& instanceId);
			~EditorViewModule();

			Dia::Editor::EditorView& GetView() { return mView; }

		protected:
			Dia::ApplicationFlow::StartResult DoStart() override;
			void DoUpdate(float deltaTime) override;
			Dia::ApplicationFlow::StopResult DoStop() override;

		private:
			Dia::Editor::EditorView          mView;
			Dia::Editor::EditorConsoleSink   mConsoleSink;
			Dia::Window::IWindow*            mWindow;
			Dia::UI::IUISystem*              mUISystem;

			Dia::ApplicationFlow::ModuleRef<EditorModelModule>          mModelRef;
			Dia::ApplicationFlow::ModuleRef<EditorViewControllerModule> mControllerRef;
			Dia::ApplicationFlow::ModuleRef<SplashScreenModule>         mSplashRef;
		};
	}
}
