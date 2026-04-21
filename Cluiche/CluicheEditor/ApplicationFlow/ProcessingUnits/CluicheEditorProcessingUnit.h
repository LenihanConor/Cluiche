#pragma once

#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaCore/CRC/StringCRC.h>

#include "../Modules/EditorModelModule.h"
#include "../Modules/CommandHistoryModule.h"
#include "../Modules/EditorViewModule.h"
#include "../Modules/EditorViewControllerModule.h"
#include "../Modules/GameConnectionModule.h"
#include "../Modules/PluginLoaderModule.h"
#include "../Phases/CluicheEditorBootPhase.h"
#include "../Phases/CluicheEditorRunningPhase.h"
#include "../Phases/CluicheEditorShutdownPhase.h"

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

			void SetProjectPath(const char* path);
			const char* GetProjectPath() const { return mProjectPath; }

			EditorModelModule& GetModelModule() { return mModelModule; }
			CommandHistoryModule& GetCommandHistoryModule() { return mCommandHistoryModule; }
			EditorViewModule& GetViewModule() { return mViewModule; }
			GameConnectionModule& GetGameConnectionModule() { return mGameConnectionModule; }
			PluginLoaderModule& GetPluginLoaderModule() { return mPluginLoaderModule; }

		private:
			EditorModelModule mModelModule;
			CommandHistoryModule mCommandHistoryModule;
			EditorViewModule mViewModule;
			EditorViewControllerModule mViewControllerModule;
			GameConnectionModule mGameConnectionModule;
			PluginLoaderModule mPluginLoaderModule;

			CluicheEditorBootPhase mBootPhase;
			CluicheEditorRunningPhase mRunningPhase;
			CluicheEditorShutdownPhase mShutdownPhase;

			static const unsigned int kMaxPathLength = 260;
			char mProjectPath[kMaxPathLength];
		};
	}
}
