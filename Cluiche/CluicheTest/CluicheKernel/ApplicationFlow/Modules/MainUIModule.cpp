#include "ApplicationFlow/Modules/MainUIModule.h"
#include <DiaCore/Core/Log.h>
#include <DiaUI/IUISystem.h>
#include <DiaUI/UIDataBuffer.h>
#ifdef _WIN64
#include <DiaUIUltralight/UltralightUISystem.h>
#else
#include <DiaUIAwesomium/AwesomiumUISystem.h>
#endif
#include <CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h>
#include <DiaCore/Memory/Memory.h>

namespace Cluiche
{
	namespace Main
	{
		const Dia::Core::StringCRC UIModule::kUniqueId("Main::UIModule");

		UIModule::UIModule(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Dia::Application::Module::RunningEnum::kUpdate)
			, mUISystem(nullptr)
		{}

		Dia::UI::IUISystem* UIModule::GetUISystem()
		{
			return mUISystem;
		}

		const Dia::UI::IUISystem* UIModule::GetUISystem()const
		{
			return mUISystem;
		}

		void UIModule::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			this->AddDependancy(buildDependencies->GetModule(Cluiche::Main::KernelModule::kUniqueId));
		}

		Dia::Application::StateObject::OpertionResponse UIModule::DoStart(const IStartData* startData)
		{
			Cluiche::Main::KernelModule* kernel = this->GetModule<Cluiche::Main::KernelModule>();

			if (kernel == nullptr)
			{
				DIA_ASSERT(0, "MainKernel has not been initialized");
				return StateObject::OpertionResponse::kImmediate;
			}
			// Setup UI
#ifdef _WIN64
			mUISystem = DIA_NEW(Dia::UI::Ultralight::UISystem(kernel->GetWindow()));
#else
			mUISystem = DIA_NEW(Dia::UI::Awesomium::UISystem(kernel->GetWindow()));
#endif
			mUISystem->Initialize();

			this->NotifyObservers(NotificationEnum::kStarted);

			return StateObject::OpertionResponse::kImmediate;
		}

		void UIModule::DoUpdate()
		{
			Cluiche::Main::KernelModule* kernel = this->GetModule<Cluiche::Main::KernelModule>();

			const Dia::Input::EventData& inputEventPtrBuffer = kernel->GetInputEventData();

			// This is where an application will handle any user input
			for (unsigned int j = 0; j < inputEventPtrBuffer.Size(); j++)
			{
				Dia::Input::Event event = inputEventPtrBuffer[j];
				switch (event.type)
				{
				case Dia::Input::Event::EType::kMouseMoved:
				{
					mUISystem->InjectMouseMove(event.mouseMove.x, event.mouseMove.y);
				}
				break;
				case Dia::Input::Event::EType::kMouseButtonReleased:
				{
					mUISystem->InjectMouseClick(event.mouseButton.AsMouseButton(), event.mouseButton.x, event.mouseButton.y);
				}
				break;
				default:
					break;
				}
			}

			mUISystem->Update();
		}

		void UIModule::DoStop()
		{
			this->NotifyObservers(NotificationEnum::kStopped);

			DIA_DELETE(mUISystem);
		}
	}
}
