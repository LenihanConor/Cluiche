#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/Architecture/Observer.h>

namespace Dia { namespace UI { class IUISystem; } }
#ifdef _WIN64
namespace Dia { namespace UI { namespace Ultralight { class UISystem; } } }
#else
namespace Dia { namespace UI { namespace Awesomium { class UISystem; } } }
#endif

namespace Cluiche
{
	namespace Main
	{
		////////////////////////////////////////////////////
		//
		// UIModule: UI System
		//
		////////////////////////////////////////////////////
		class UIModule : public Dia::Application::Module, public Dia::Core::ObserverSubject
		{
		public:
			////////////////////////////////////////////////////////////////////////////////
			// Enum name: NotificationEnum, message sent out at each notfication
			////////////////////////////////////////////////////////////////////////////////
			CLASSEDENUM(NotificationEnum, \
				CE_ITEMVAL(kStarted, 0)\
				CE_ITEM(kStopped)\
				, kStarted \
			);

			static const Dia::Core::StringCRC kUniqueId;

			UIModule(Dia::Application::ProcessingUnit* associatedProcessingUnit);

			Dia::UI::IUISystem* GetUISystem();
			const Dia::UI::IUISystem* GetUISystem()const;

		private:
			virtual void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)override;
			virtual Dia::Application::StateObject::OpertionResponse DoStart(const IStartData* startData) override;
			virtual void DoUpdate() override;
			virtual void DoStop() override;

#ifdef _WIN64
			Dia::UI::Ultralight::UISystem* mUISystem;
#else
			Dia::UI::Awesomium::UISystem* mUISystem;
#endif
		};
	}
}
