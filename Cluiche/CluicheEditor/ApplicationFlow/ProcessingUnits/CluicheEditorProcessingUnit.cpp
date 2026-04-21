#include "CluicheEditorProcessingUnit.h"

#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/EditorManifestLoader.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Core/Log.h>
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
			mViewModule.SetController(&mViewControllerModule.GetController());

			mProjectPath[0] = '\0';

			Initialize();
		}

		CluicheEditorProcessingUnit::~CluicheEditorProcessingUnit()
		{
			for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
			{
				Dia::Core::Log::OutputVaradicLine("CluicheEditorPU: OnUnload '%s'", mLoadedPlugins[i]->GetName());
				mLoadedPlugins[i]->OnUnload();
				delete mLoadedPlugins[i];
			}
			mLoadedPlugins.RemoveAll();
		}

		void CluicheEditorProcessingUnit::PostPhaseUpdate()
		{
			static const float kFixedDeltaTime = 1.0f / 60.0f;
			for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
			{
				mLoadedPlugins[i]->OnUpdate(kFixedDeltaTime);
			}
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

		void CluicheEditorProcessingUnit::LoadPlugin(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& instanceId)
		{
			Dia::Core::Log::OutputVaradicLine("CluicheEditorPU::LoadPlugin: Creating plugin");

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

			Dia::Core::Log::OutputVaradicLine("CluicheEditorPU::LoadPlugin: Created '%s'", plugin->GetName());
			plugin->OnLoad(&mModelModule.GetModel());
			Dia::Core::Log::OutputVaradicLine("CluicheEditorPU::LoadPlugin: OnLoad complete for '%s'", plugin->GetName());
			mViewModule.GetView().RegisterComponent(plugin->GetName(), plugin->GetUIPath());
			Dia::Core::Log::OutputVaradicLine("CluicheEditorPU::LoadPlugin: Registered '%s' at '%s'", plugin->GetName(), plugin->GetUIPath());
			mLoadedPlugins.Add(plugin);
		}

		void CluicheEditorProcessingUnit::LoadEditorManifest(const char* manifestPath)
		{
			DIA_ASSERT(manifestPath != nullptr, "CluicheEditorProcessingUnit: manifest path must not be null");
			Dia::Core::Log::OutputVaradicLine("CluicheEditorPU::LoadEditorManifest: Loading '%s'", manifestPath);

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
