#include "CluicheEditorProcessingUnit.h"

#include <DiaLogger/DebugOutputSink.h>
#include <DiaEditor/Sinks/EditorConsoleSink.h>
#include <string.h>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC CluicheEditorProcessingUnit::kTypeId("CluicheEditorApp");

		CluicheEditorProcessingUnit::CluicheEditorProcessingUnit()
			: Dia::Application::ProcessingUnit(kTypeId, 60.0f)
			, mModelModule(this)
			, mCommandHistoryModule(this)
			, mViewModule(this)
			, mViewControllerModule(this)
			, mPluginLoaderModule(this, &mModelModule.GetModel())
			, mLoggerModule(this)
			, mBootPhase(this)
			, mRunningPhase(this)
			, mShutdownPhase(this)
		{
			mLoggerModule.AddSink(new Dia::Logger::DebugOutputSink());
			mLoggerModule.AddSink(new Dia::Editor::EditorConsoleSink(&mViewModule.GetView()));
			mLoggerModule.ApplyConfig("Data/editor-logger.json");

			AddModule(&mLoggerModule);
			AddModule(&mModelModule);
			AddModule(&mCommandHistoryModule);
			AddModule(&mViewModule);
			AddModule(&mViewControllerModule);
			AddModule(&mPluginLoaderModule);

			AddPhase(&mBootPhase);
			AddPhase(&mRunningPhase);
			AddPhase(&mShutdownPhase);

			SetInitialPhase(&mBootPhase);
			AddPhaseTransiton(&mBootPhase, &mRunningPhase);
			AddPhaseTransiton(&mRunningPhase, &mShutdownPhase);

			mViewControllerModule.GetController().SetCommandHistory(&mCommandHistoryModule.GetHistory());
			mViewControllerModule.GetController().SetModel(&mModelModule.GetModel());
			mViewModule.SetModel(&mModelModule.GetModel());
			mViewModule.SetController(&mViewControllerModule.GetController());

			mProjectPath[0] = '\0';

			Initialize();
		}

		void CluicheEditorProcessingUnit::SetProjectPath(const char* path)
		{
			if (path == nullptr)
			{
				mProjectPath[0] = '\0';
				return;
			}
			strncpy_s(mProjectPath, kMaxPathLength, path, _TRUNCATE);
		}

		bool CluicheEditorProcessingUnit::FlaggedToStopUpdating() const
		{
			auto* phase = const_cast<CluicheEditorProcessingUnit*>(this)->GetCurrentPhase();
			return phase != nullptr && phase->FlaggedToStopUpdating();
		}
	}
}
