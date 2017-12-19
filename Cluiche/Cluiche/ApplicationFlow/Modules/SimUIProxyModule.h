#pragma once

#include <DiaApplication/ApplicationModule.h>

namespace Dia { namespace UI { class IUISystem; } }
namespace Dia { namespace Graphics { class FrameData; } }

namespace Cluiche
{
	////////////////////////////////////////////////////
	//
	// SimUIProxyModule: Sim access to the ui system that runs on the main thread
	//
	////////////////////////////////////////////////////
	class SimUIProxyModule : public Dia::Application::Module
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		SimUIProxyModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, Dia::Graphics::FrameData* renderFrameBuffer);

		void Attach(Dia::UI::IUISystem* ui);
		void Detach();

		bool IsUISystenAvailable()const;
		const Dia::UI::IUISystem* GetUISystem()const;

	private:
		virtual void DoUpdate() override;

		Dia::UI::IUISystem* mUISystem;
		Dia::Graphics::FrameData* mRenderFrameBuffer;
	};
}
