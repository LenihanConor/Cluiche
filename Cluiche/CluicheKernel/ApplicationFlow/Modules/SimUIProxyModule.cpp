#include "ApplicationFlow/Modules/SimUIProxyModule.h"

#include <DiaUI/IUISystem.h>
#include <DiaUI/UIDataBuffer.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <ApplicationFlow/Modules/MainUIModule.h>
#include <CluicheKernel/ApplicationFlow/Modules/SimInputFrameStreamModule.h>
#include <CluicheKernel/ApplicationFlow/Modules/SimTimeServerModule.h>

namespace Cluiche
{
	namespace Sim
	{
		const Dia::Core::StringCRC UIProxyModule::kUniqueId("Sim::UIProxyModule");

		UIProxyModule::UIProxyModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, Dia::Graphics::FrameData* renderFrameBuffer)
			: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Dia::Application::Module::RunningEnum::kUpdate)
			, mIsAvailable(false)
			, mMainUIModule(nullptr)
			, mRenderFrameBuffer(renderFrameBuffer)
		{}

		void UIProxyModule::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			this->AddDependancy(buildDependencies->GetModule(Cluiche::Sim::TimeServerModule::kUniqueId));
			this->AddDependancy(buildDependencies->GetModule(Cluiche::Sim::InputFrameStreamModule::kUniqueId));
		}

		void UIProxyModule::Initialize(Main::UIModule* ui)
		{
			DIA_ARRAY__(mMainUIModule == nullptr, "mUISystem is allocated already");
			DIA_ARRAY__(ui != nullptr, "Cannot assign a null pointer ui system");

			mMainUIModule = ui;
		}

		Dia::Application::StateObject::OpertionResponse UIProxyModule::DoStart(const IStartData* startData)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mIsAvailable = mMainUIModule->HasStarted();
			mMainUIModule->AttachToObserver(this);

			return StateObject::OpertionResponse::kImmediate;
		}

		void UIProxyModule::DoStop()
		{
			std::lock_guard<std::mutex> lock(mMutex);

			mIsAvailable = false;

			mMainUIModule->DetachFromObserver(this);
		}

		void UIProxyModule::DoUpdate()
		{
			std::lock_guard<std::mutex> lock(mMutex);

			if (IsUISystenAvailable() && mMainUIModule->GetUISystem()->IsPageLoaded())
			{
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

		bool UIProxyModule::IsUISystenAvailable()const
		{
			return mIsAvailable;
		}

		const Dia::UI::IUISystem* UIProxyModule::GetUISystem()const
		{
			DIA_ASSERT(IsUISystenAvailable(), "Ui system is null");

			return mMainUIModule->GetUISystem();
		}

		void UIProxyModule::ObserverNotification(const Dia::Core::ObserverSubject* subject, int message)
		{
			switch (message)
			{
			case Main::UIModule::NotificationEnum::kStarted:
			{
				std::lock_guard<std::mutex> lock(mMutex);
				mIsAvailable = true;
			}break;
			case Main::UIModule::NotificationEnum::kStopped:
			{
				std::lock_guard<std::mutex> lock(mMutex);
				mIsAvailable = false;
			}break;
			default:
				break;
			}
		}
	}
}