#pragma once

#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include "../Modules/EditorModelModule.h"
#include "../Modules/CommandHistoryModule.h"
#include "../Modules/EditorViewModule.h"
#include "../Modules/EditorViewControllerModule.h"
#include "../Modules/GameConnectionModule.h"
#include "../Phases/CluicheEditorBootPhase.h"
#include "../Phases/CluicheEditorRunningPhase.h"
#include "../Phases/CluicheEditorShutdownPhase.h"

namespace Dia { namespace Editor { class IEditorPlugin; } }

namespace Cluiche
{
	namespace Editor
	{
		class CluicheEditorProcessingUnit : public Dia::Application::ProcessingUnit
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			CluicheEditorProcessingUnit();

			bool FlaggedToStopUpdating() const override;

			void LoadPlugin(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& instanceId);
			void LoadEditorManifest(const char* manifestPath);

			EditorModelModule& GetModelModule() { return mModelModule; }
			CommandHistoryModule& GetCommandHistoryModule() { return mCommandHistoryModule; }
			EditorViewModule& GetViewModule() { return mViewModule; }
			GameConnectionModule& GetGameConnectionModule() { return mGameConnectionModule; }

		private:
			EditorModelModule mModelModule;
			CommandHistoryModule mCommandHistoryModule;
			EditorViewModule mViewModule;
			EditorViewControllerModule mViewControllerModule;
			GameConnectionModule mGameConnectionModule;

			CluicheEditorBootPhase mBootPhase;
			CluicheEditorRunningPhase mRunningPhase;
			CluicheEditorShutdownPhase mShutdownPhase;

			static const unsigned int kMaxPlugins = 16;
			Dia::Core::Containers::DynamicArrayC<Dia::Editor::IEditorPlugin*, kMaxPlugins> mLoadedPlugins;
		};
	}
}
