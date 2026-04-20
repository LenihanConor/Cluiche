#include "CluicheEditorProcessingUnit.h"

#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/EditorManifestLoader.h>
#include <DiaCore/Core/Assert.h>

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
			, mGameConnectionModule(this)
			, mBootPhase(this)
			, mRunningPhase(this)
			, mShutdownPhase(this)
		{
			AddModule(&mModelModule);
			AddModule(&mCommandHistoryModule);
			AddModule(&mViewModule);
			AddModule(&mViewControllerModule);
			AddModule(&mGameConnectionModule);

			AddPhase(&mBootPhase);
			AddPhase(&mRunningPhase);
			AddPhase(&mShutdownPhase);

			SetInitialPhase(&mBootPhase);
			AddPhaseTransiton(&mBootPhase, &mRunningPhase);
			AddPhaseTransiton(&mRunningPhase, &mShutdownPhase);

			mViewControllerModule.GetController().SetCommandHistory(&mCommandHistoryModule.GetHistory());
			mViewControllerModule.GetController().SetModel(&mModelModule.GetModel());
			mViewModule.SetModel(&mModelModule.GetModel());

			Initialize();
		}

		bool CluicheEditorProcessingUnit::FlaggedToStopUpdating() const
		{
			auto* phase = const_cast<CluicheEditorProcessingUnit*>(this)->GetCurrentPhase();
			return phase != nullptr && phase->FlaggedToStopUpdating();
		}

		void CluicheEditorProcessingUnit::LoadPlugin(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& instanceId)
		{
			if (mLoadedPlugins.IsFull())
			{
				DIA_ASSERT(false, "CluicheEditorProcessingUnit: max plugin capacity reached");
				return;
			}

			Dia::Editor::IEditorPlugin* plugin = Dia::Editor::EditorPluginRegistry::Instance().CreatePlugin(typeId);
			if (plugin == nullptr)
			{
				DIA_ASSERT(false, "CluicheEditorProcessingUnit: plugin type not registered");
				return;
			}

			plugin->OnLoad(&mModelModule.GetModel());
			mViewModule.GetView().RegisterComponent(plugin->GetName(), plugin->GetUIPath());
			mLoadedPlugins.Add(plugin);
		}

		void CluicheEditorProcessingUnit::LoadEditorManifest(const char* manifestPath)
		{
			DIA_ASSERT(manifestPath != nullptr, "CluicheEditorProcessingUnit: manifest path must not be null");

			struct LoadCtx { CluicheEditorProcessingUnit* pu; };
			LoadCtx ctx{ this };

			Dia::Editor::EditorManifestLoader::Load(manifestPath,
				[](const Dia::Editor::EditorManifestLoader::PluginEntry& entry, void* userData)
				{
					auto* c = static_cast<LoadCtx*>(userData);
					c->pu->LoadPlugin(
						Dia::Core::StringCRC(entry.typeId),
						Dia::Core::StringCRC(entry.instanceId));
				},
				&ctx);
		}
	}
}
