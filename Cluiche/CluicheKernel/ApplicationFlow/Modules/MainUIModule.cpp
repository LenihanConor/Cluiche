#include "ApplicationFlow/Modules/MainUIModule.h"
#include <DiaCore/Core/Log.h>
#include <DiaUI/IUISystem.h>
#include <DiaUI/UIDataBuffer.h>
#include <DiaUIAwesomium/AwesomiumUISystem.h>
#include <CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h>
#include <DiaCore/Memory/Memory.h>

namespace Cluiche
{
	namespace Main
	{
		const Dia::Core::StringCRC UIModule::kUniqueId("Main::UIModule");

		UIModule::UIModule(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Dia::Application::Module::RunningEnum::kUpdate)
			, mAwesomiumUISystem(nullptr)
		{}

		Dia::UI::IUISystem* UIModule::GetUISystem()
		{
			return mAwesomiumUISystem;
		}

		const Dia::UI::IUISystem* UIModule::GetUISystem()const
		{
			return mAwesomiumUISystem;
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
			mAwesomiumUISystem = DIA_NEW(Dia::UI::Awesomium::UISystem(kernel->GetWindow()));
			mAwesomiumUISystem->Initialize();

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
					mAwesomiumUISystem->InjectMouseMove(event.mouseMove.x, event.mouseMove.y);
				}
				break;
				case Dia::Input::Event::EType::kMouseButtonPressed:
				{
					mAwesomiumUISystem->InjectMouseClick(event.mouseButton.AsMouseButton(), event.mouseButton.x, event.mouseButton.y);
				}
				break;
				default:
					break;
				}
			}

			mAwesomiumUISystem->Update();
		}

		void UIModule::DoStop()
		{
			this->NotifyObservers(NotificationEnum::kStopped);

			DIA_DELETE(mAwesomiumUISystem);
		}
	}
}