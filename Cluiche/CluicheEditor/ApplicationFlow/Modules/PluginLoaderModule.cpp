#include "PluginLoaderModule.h"
#include "EditorModelModule.h"
#include "EditorViewModule.h"

#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/EditorManifestLoader.h>
#include <DiaEditor/MVC/EditorView.h>
#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC PluginLoaderModule::kTypeId("PluginLoaderModule");

		PluginLoaderModule::PluginLoaderModule(Dia::Application::ProcessingUnit* pu)
			: Dia::Application::Module(pu, kTypeId, RunningEnum::kUpdate)
			, mView(nullptr)
		{
			mContext.mPluginLoader = this;
			mContext.mServices = &mServiceLocator;
		}

		void PluginLoaderModule::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			AddDependancy(buildDependencies->GetModule(EditorModelModule::kTypeId));
			AddDependancy(buildDependencies->GetModule(EditorViewModule::kTypeId));
		}

		void PluginLoaderModule::SetBridge(Dia::Editor::WebUIBridge* bridge)
		{
			mContext.mBridge = bridge;
		}

		Dia::Application::StateObject::OpertionResponse PluginLoaderModule::DoStart(const Dia::Application::StateObject::IStartData*)
		{
			EditorModelModule* modelModule = GetModule<EditorModelModule>();
			DIA_ASSERT(modelModule != nullptr, "PluginLoaderModule requires EditorModelModule");
			mContext.mModel = &modelModule->GetModel();

			DIA_LOG_INFO("Application", "PluginLoaderModule: DoStart");
			return Dia::Application::StateObject::OpertionResponse::kImmediate;
		}

		void PluginLoaderModule::DoUpdate()
		{
			static const float kFixedDeltaTime = 1.0f / 60.0f;
			for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
			{
				mLoadedPlugins[i].plugin->OnUpdate(kFixedDeltaTime);
			}
		}

		void PluginLoaderModule::DoStop()
		{
			DIA_LOG_INFO("Application", "PluginLoaderModule: DoStop - unloading %u plugins", mLoadedPlugins.Size());
			for (int i = static_cast<int>(mLoadedPlugins.Size()) - 1; i >= 0; --i)
			{
				DIA_LOG_INFO("Application", "PluginLoaderModule: OnUnload '%s'", mLoadedPlugins[i].plugin->GetName());
				mLoadedPlugins[i].plugin->OnUnload();
				delete mLoadedPlugins[i].plugin;
			}
			mLoadedPlugins.RemoveAll();
		}

		void PluginLoaderModule::LoadBuiltInPlugins()
		{
			DIA_LOG_INFO("Application", "PluginLoaderModule: Loading built-in plugins");
			LoadPlugin(Dia::Core::StringCRC("HomeEditorPlugin"), Dia::Core::StringCRC("home_builtin"));
			LoadPlugin(Dia::Core::StringCRC("OutputConsoleEditorPlugin"), Dia::Core::StringCRC("outputconsole_builtin"));
			LoadPlugin(Dia::Core::StringCRC("GameConnectionEditorPlugin"), Dia::Core::StringCRC("gameconnection_builtin"));
			LoadPlugin(Dia::Core::StringCRC("PluginBrowserEditorPlugin"), Dia::Core::StringCRC("pluginbrowser_builtin"));
		}

		void PluginLoaderModule::LoadManifest(const char* manifestPath)
		{
			DIA_ASSERT(manifestPath != nullptr, "PluginLoaderModule: manifest path must not be null");
			DIA_LOG_INFO("Application", "PluginLoaderModule: Loading manifest '%s'", manifestPath);

			struct LoadCtx { PluginLoaderModule* module; };
			LoadCtx ctx{ this };

			Dia::Editor::EditorManifestLoader::Load(manifestPath,
				[](const Dia::Editor::EditorManifestLoader::PluginEntry& entry, void* userData)
				{
					auto* c = static_cast<LoadCtx*>(userData);
					c->module->LoadPlugin(
						Dia::Core::StringCRC(entry.typeId),
						Dia::Core::StringCRC(entry.instanceId));
				},
				&ctx);
		}

		void PluginLoaderModule::LoadPlugin(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& instanceId)
		{
			DIA_LOG_INFO("Application", "PluginLoaderModule::LoadPlugin: Creating plugin");

			if (mLoadedPlugins.IsFull())
			{
				DIA_ASSERT(false, "PluginLoaderModule: max plugin capacity reached");
				return;
			}

			Dia::Editor::IEditorPlugin* plugin = Dia::Editor::EditorPluginRegistry::Instance().CreatePlugin(typeId);
			if (plugin == nullptr)
			{
				DIA_ASSERT(false, "PluginLoaderModule: plugin type not registered");
				return;
			}

			DIA_LOG_INFO("Application", "PluginLoaderModule::LoadPlugin: Created '%s'", plugin->GetName());
			plugin->OnLoad(mContext);
			DIA_LOG_INFO("Application", "PluginLoaderModule::LoadPlugin: OnLoad complete for '%s'", plugin->GetName());

			if (mView != nullptr)
			{
				mView->RegisterComponent(plugin->GetName(), plugin->GetUIPath());
				DIA_LOG_INFO("Application", "PluginLoaderModule::LoadPlugin: Registered '%s' at '%s'", plugin->GetName(), plugin->GetUIPath());
			}

			LoadedPluginEntry entry;
			entry.typeId = typeId;
			entry.plugin = plugin;
			mLoadedPlugins.Add(entry);

			if (mView != nullptr)
			{
				mView->NotifyPanelsChanged();
			}
		}

		bool PluginLoaderModule::UnloadPlugin(const Dia::Core::StringCRC& typeId)
		{
			if (IsPluginPinned(typeId))
			{
				DIA_LOG_WARNING("Application", "PluginLoaderModule: cannot unload pinned plugin");
				return false;
			}

			for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
			{
				if (mLoadedPlugins[i].typeId == typeId)
				{
					DIA_LOG_INFO("Application", "PluginLoaderModule: Unloading '%s'", mLoadedPlugins[i].plugin->GetName());

					if (mView != nullptr)
					{
						mView->UnregisterComponent(mLoadedPlugins[i].plugin->GetName());
					}

					mLoadedPlugins[i].plugin->OnUnload();
					delete mLoadedPlugins[i].plugin;
					mLoadedPlugins.RemoveAt(i);

					if (mView != nullptr)
					{
						mView->NotifyPanelsChanged();
					}

					return true;
				}
			}
			return false;
		}

		bool PluginLoaderModule::IsPluginTypeLoaded(const Dia::Core::StringCRC& typeId) const
		{
			for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
			{
				if (mLoadedPlugins[i].typeId == typeId)
				{
					return true;
				}
			}
			return false;
		}

		bool PluginLoaderModule::IsPluginPinned(const Dia::Core::StringCRC& typeId) const
		{
			for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
			{
				if (mLoadedPlugins[i].typeId == typeId)
				{
					Dia::Editor::EditorToolbarItem toolbar = mLoadedPlugins[i].plugin->GetToolbarItem();
					return toolbar.pinned;
				}
			}
			return false;
		}

		void PluginLoaderModule::RegisterView(Dia::Editor::EditorView* view)
		{
			mView = view;
			mContext.mView = view;

			for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
			{
				mView->RegisterComponent(mLoadedPlugins[i].plugin->GetName(), mLoadedPlugins[i].plugin->GetUIPath());
				DIA_LOG_INFO("Application", "PluginLoaderModule: Retroactively registered '%s'", mLoadedPlugins[i].plugin->GetName());
			}
		}
	}
}
