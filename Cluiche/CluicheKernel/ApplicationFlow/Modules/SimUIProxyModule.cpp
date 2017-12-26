#include "ApplicationFlow/Modules/SimUIProxyModule.h"

#include <DiaUI/IUISystem.h>
#include <DiaUI/UIDataBuffer.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <ApplicationFlow/Modules/MainUIModule.h>
#include <CluicheKernel/ApplicationFlow/Modules/SimInputFrameStreamModule.h>
#include <CluicheKernel/ApplicationFlow/Modules/SimTimeServerModule.h>

namespace Cluiche
{
	const Dia::Core::StringCRC SimUIProxyModule::kUniqueId("SimUIProxyModule");

	SimUIProxyModule::SimUIProxyModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, Dia::Graphics::FrameData* renderFrameBuffer)
		: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Dia::Application::Module::RunningEnum::kUpdate)
		, mIsAvailable(false)
		, mMainUIModule(nullptr)
		, mRenderFrameBuffer(renderFrameBuffer)
	{}

	void SimUIProxyModule::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
	{
		this->AddDependancy(buildDependencies->GetModule(Cluiche::SimTimeServerModule::kUniqueId));
		this->AddDependancy(buildDependencies->GetModule(Cluiche::SimInputFrameStreamModule::kUniqueId));
	}

	void SimUIProxyModule::Initialize(MainUIModule* ui)
	{
		DIA_ARRAY__(mMainUIModule == nullptr, "mUISystem is allocated already");
		DIA_ARRAY__(ui != nullptr, "Cannot assign a null pointer ui system");

		mMainUIModule = ui;
	}

	Dia::Application::StateObject::OpertionResponse SimUIProxyModule::DoStart(const IStartData* startData)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mIsAvailable = mMainUIModule->HasStarted();
		mMainUIModule->AttachToObserver(this);

		return StateObject::OpertionResponse::kImmediate;
	}

	void SimUIProxyModule::DoStop()
	{ 
		std::lock_guard<std::mutex> lock(mMutex);

		mIsAvailable = false;

		mMainUIModule->DetachFromObserver(this);
	}

	void SimUIProxyModule::DoUpdate()
	{
		std::lock_guard<std::mutex> lock(mMutex);

		if (IsUISystenAvailable() && mMainUIModule->GetUISystem()->IsPageLoaded())
		{
	/*		const SimInputFrameStreamModule::InputFrameStream* stream = GetModule<Cluiche::SimInputFrameStreamModule>()->GetStream();
			const Dia::Core::TimeServer& timeServer = GetModule<Cluiche::SimTimeServerModule>()->GetTimeServer();

			Dia::Core::Containers::DynamicArrayC<const Dia::Input::EventData*, 32> inputEventPtrBuffer;
			stream->FetchAllDataUpToTime(timeServer.GetTime(), inputEventPtrBuffer);

			for (unsigned int i = 0; i < inputEventPtrBuffer.Size(); i++)
			{
				const Dia::Input::EventData* pEventData = inputEventPtrBuffer[i];

				// This is where an application will handle any user input
				for (unsigned int j = 0; j < pEventData->Size(); j++)
				{
					Dia::Input::Event event = (*pEventData)[j];
					switch (event.type)
					{
					case Dia::Input::Event::EType::kMouseMoved:
					{
//						mMainUIModule->GetUISystem()->InjectMouseMove(event.mouseMove.x, event.mouseMove.y);
					}
					break;
					case Dia::Input::Event::EType::kMouseButtonPressed:
					{
						mMainUIModule->GetUISystem()->InjectMouseDown(event.mouseButton.AsMouseButton(), event.mouseButton.x, event.mouseButton.y);
					}
					break;
					}
				}
			}
			*/
			Dia::UI::UIDataBuffer uiBuffer;
			mMainUIModule->GetUISystem()->FetchUIDataBuffer(uiBuffer);
			mRenderFrameBuffer->RequestDrawUI(uiBuffer);
		}
		else
		{
			Dia::UI::UIDataBuffer uiBuffer;
			mRenderFrameBuffer->RequestDrawUI(uiBuffer);
		}
	}

	bool SimUIProxyModule::IsUISystenAvailable()const
	{
		return mIsAvailable;
	}

	const Dia::UI::IUISystem* SimUIProxyModule::GetUISystem()const
	{
		DIA_ASSERT(IsUISystenAvailable(), "Ui system is null");

		return mMainUIModule->GetUISystem();
	}

	void SimUIProxyModule::ObserverNotification(const Dia::Core::ObserverSubject* subject, int message)
	{
		switch (message)
		{
		case MainUIModule::NotificationEnum::kStarted:
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mIsAvailable = true;
		}break;
		case MainUIModule::NotificationEnum::kStopped:
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mIsAvailable = false;
		}break;
		default:
			break;
		}
	}
}