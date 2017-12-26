#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/Architecture/Observer.h>

namespace Dia { namespace UI { class IUISystem; } }
namespace Dia { namespace Graphics { class FrameData; } }
namespace Cluiche { class MainUIModule; }

namespace Cluiche
{
	////////////////////////////////////////////////////
	//
	// SimUIProxyModule: Sim access to the ui system that runs on the main thread
	//
	////////////////////////////////////////////////////
	class SimUIProxyModule : public Dia::Application::Module, public Dia::Core::Observer
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		SimUIProxyModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, Dia::Graphics::FrameData* renderFrameBuffer);

		void Initialize(MainUIModule* ui);

		bool IsUISystenAvailable()const;
		const Dia::UI::IUISystem* GetUISystem()const;

	private:
		virtual Dia::Application::StateObject::OpertionResponse DoStart(const IStartData* startData) override;
		virtual void DoUpdate() override;
		virtual void DoStop() override;

		virtual void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)override; 

		virtual void ObserverNotification(const Dia::Core::ObserverSubject* subject, int message) override;

		bool mIsAvailable;
		std::mutex mMutex;
		MainUIModule* mMainUIModule;
		Dia::Graphics::FrameData* mRenderFrameBuffer;
	};
}
