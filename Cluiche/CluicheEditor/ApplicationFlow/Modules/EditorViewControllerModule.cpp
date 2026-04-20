#include "EditorViewControllerModule.h"

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorViewControllerModule::kTypeId("EditorViewControllerModule");

		EditorViewControllerModule::EditorViewControllerModule(Dia::Application::ProcessingUnit* pu)
			: Dia::Application::Module(pu, kTypeId, RunningEnum::kUpdate)
		{
		}
	}
}
