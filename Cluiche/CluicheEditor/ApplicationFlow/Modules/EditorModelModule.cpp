#include "EditorModelModule.h"

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorModelModule::kTypeId("EditorModelModule");

		EditorModelModule::EditorModelModule(Dia::Application::ProcessingUnit* pu)
			: Dia::Application::Module(pu, kTypeId, RunningEnum::kIdle)
		{
		}

		void EditorModelModule::DoStop()
		{
			mModel.Reset();
		}
	}
}
