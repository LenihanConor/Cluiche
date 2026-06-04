#include "PluginLoaderModule.h"
#include "EditorModelModule.h"
#include "EditorViewModule.h"

#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/EditorManifestLoader.h>
#include <DiaEditor/MVC/EditorView.h>
#include <DiaEditor/Layout/DockingLayout.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <string>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC PluginLoaderModule::kTypeId("PluginLoaderModule");

		PluginLoaderModule::PluginLoaderModule(const Dia::Core::StringCRC& instanceId)
			: Dia::ApplicationFlow::Module(instanceId)
			, mView(nullptr)
			, mModelRef(this, EditorModelModule::kTypeId)
			, mViewRef(this, EditorViewModule::kTypeId)
		{
			mContext.mPluginLoader = this;
			mContext.mServices = &mServiceLocator;
		}

		void PluginLoaderModule::SetBridge(Dia::Editor::WebUIBridge* bridge)
		{
			mContext.mBridge = bridge;
		}

		Dia::ApplicationFlow::StartResult PluginLoaderModule::DoStart()
		{
			EditorModelModule* modelModule = mModelRef.Get();
			DIA_ASSERT(modelModule != nullptr, "PluginLoaderModule requires EditorModelModule");
			if (modelModule != nullptr)
			{
				mContext.mModel = &modelModule->GetModel();
				mContext.mProjectPath = modelModule->GetProjectPath();
			}

			EditorViewModule* viewModule = mViewRef.Get();
			if (viewModule != nullptr)
			{
				Dia::Editor::EditorView& view = viewModule->GetView();

				// Layout config path must be set BEFORE RegisterView so NotifyPanelsChanged calls
				// during plugin load can persist to the right file, and before LoadLayoutFromDisk.
				view.SetLayoutPath("assets/configs/editor-layout.json");
				view.LoadLayoutFromDisk();

				RegisterView(&view);
				SetBridge(view.GetWebUIBridge());
			}

			DIA_LOG_INFO("Application", "PluginLoaderModule: DoStart");
			LoadBuiltInPlugins();
			RestoreLayoutPlugins();

			// Load the project (if one was specified on the command line) and any
			// manifests it references.  The module's mProjectPath is populated by
			// EditorModelModule::DoStart from GetCommandLineW().
			if (modelModule != nullptr)
			{
				const char* projectPath = modelModule->GetProjectPath();
				if (projectPath != nullptr && projectPath[0] != '\0')
				{
					DIA_LOG_INFO("Application", "PluginLoaderModule: Loading project '%s'", projectPath);
					Dia::Editor::EditorModel& model = modelModule->GetModel();
					model.LoadProject(projectPath);

					unsigned int manifestCount = model.GetManifestCount();
					for (unsigned int i = 0; i < manifestCount; ++i)
					{
						LoadManifest(model.GetManifestPath(i));
					}

					if (manifestCount > 0)
					{
						Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, Dia::Editor::EditorPluginRegistry::kMaxManifests> active;
						for (unsigned int i = 0; i < manifestCount; ++i)
							active.Add(Dia::Core::StringCRC(model.GetManifestPath(i)));
						Dia::Editor::EditorPluginRegistry::Instance().SetActiveManifests(active);
					}
				}
				else
				{
					DIA_LOG_INFO("Application", "PluginLoaderModule: No project path set, skipping project load");
				}
			}

			return Dia::ApplicationFlow::StartResult::kReady;
		}

		void PluginLoaderModule::DoUpdate(float deltaTime)
		{
			for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
			{
				mLoadedPlugins[i].plugin->OnUpdate(deltaTime);
			}
		}

		Dia::ApplicationFlow::StopResult PluginLoaderModule::DoStop()
		{
			DIA_LOG_INFO("Application", "PluginLoaderModule: DoStop - unloading %u plugins", mLoadedPlugins.Size());
			for (int i = static_cast<int>(mLoadedPlugins.Size()) - 1; i >= 0; --i)
			{
				DIA_LOG_INFO("Application", "PluginLoaderModule: OnUnload '%s'", mLoadedPlugins[i].plugin->GetName());
				mLoadedPlugins[i].plugin->OnUnload();
				delete mLoadedPlugins[i].plugin;
			}
			mLoadedPlugins.RemoveAll();
			return Dia::ApplicationFlow::StopResult::kDone;
		}

		void PluginLoaderModule::LoadBuiltInPlugins()
		{
			// Dump the full registry at the start of plugin loading. This is the canonical
			// place a session log shows which plugins are linked in and how they map their
			// (typeId -> display name). Layout restore keys on the display name (GetName()),
			// so a mismatch here is the most common cause of "plugin loads iframe but no handlers".
			Dia::Editor::EditorPluginRegistry& registry = Dia::Editor::EditorPluginRegistry::Instance();
			const unsigned int registryCount = registry.GetRegisteredCount();
			DIA_LOG_INFO("Application", "PluginLoaderModule: Plugin registry has %u entries:", registryCount);
			for (unsigned int r = 0; r < registryCount; ++r)
			{
				Dia::Editor::EditorPluginInfo info = registry.GetFactory(r)->GetPluginInfo();
				DIA_LOG_INFO("Application",
					"  registry[%u]: typeId='%s' name='%s'",
					r, registry.GetRegisteredTypeId(r).AsChar(), info.name);
			}

			DIA_LOG_INFO("Application", "PluginLoaderModule: Loading built-in plugins");
			LoadPlugin(Dia::Core::StringCRC("HomeEditorPlugin"),             Dia::Core::StringCRC("home_builtin"));
			LoadPlugin(Dia::Core::StringCRC("OutputConsoleEditorPlugin"),    Dia::Core::StringCRC("outputconsole_builtin"));
			LoadPlugin(Dia::Core::StringCRC("GameConnectionEditorPlugin"),   Dia::Core::StringCRC("gameconnection_builtin"));
			LoadPlugin(Dia::Core::StringCRC("PluginBrowserEditorPlugin"),    Dia::Core::StringCRC("pluginbrowser_builtin"));
		}

		void PluginLoaderModule::RestoreLayoutPlugins()
		{
			if (mView == nullptr)
			{
				DIA_LOG_WARNING("Application", "PluginLoaderModule::RestoreLayoutPlugins: mView is null, skipping");
				return;
			}

			Dia::Editor::DockingLayout* layout = mView->GetDockingLayout();
			if (layout == nullptr)
			{
				DIA_LOG_WARNING("Application", "PluginLoaderModule::RestoreLayoutPlugins: docking layout is null, skipping");
				return;
			}

			Dia::Editor::EditorPluginRegistry& registry = Dia::Editor::EditorPluginRegistry::Instance();
			const unsigned int panelCount    = layout->GetPanelCount();
			const unsigned int registryCount = registry.GetRegisteredCount();
			DIA_LOG_INFO("Application",
				"PluginLoaderModule::RestoreLayoutPlugins: panels=%u registry=%u",
				panelCount, registryCount);

			for (unsigned int p = 0; p < panelCount; ++p)
			{
				const char* panelName = layout->GetPanel(p).name;
				bool matched = false;

				for (unsigned int r = 0; r < registryCount; ++r)
				{
					Dia::Editor::EditorPluginInfo info = registry.GetFactory(r)->GetPluginInfo();
					if (strcmp(info.name, panelName) == 0)
					{
						const Dia::Core::StringCRC& typeId = registry.GetRegisteredTypeId(r);
						if (IsPluginTypeLoaded(typeId))
						{
							DIA_LOG_INFO("Application",
								"PluginLoaderModule::RestoreLayoutPlugins: panel '%s' matched typeId='%s' (already loaded as built-in)",
								panelName, typeId.AsChar());
						}
						else
						{
							DIA_LOG_INFO("Application",
								"PluginLoaderModule::RestoreLayoutPlugins: panel '%s' matched typeId='%s', loading",
								panelName, typeId.AsChar());
							Dia::Core::StringCRC instanceId((std::string(panelName) + "_layout").c_str());
							LoadPlugin(typeId, instanceId);
						}
						matched = true;
						break;
					}
				}

				if (!matched)
				{
					DIA_LOG_ERROR("Application",
						"PluginLoaderModule::RestoreLayoutPlugins: panel '%s' has NO matching plugin (tried %u registry entries by display name). "
						"This panel will render its iframe but request handlers will NOT be registered. "
						"Check that the layout panel 'name' matches a registered plugin's GetName().",
						panelName, registryCount);

					for (unsigned int r = 0; r < registryCount; ++r)
					{
						Dia::Editor::EditorPluginInfo info = registry.GetFactory(r)->GetPluginInfo();
						DIA_LOG_ERROR("Application",
							"  registry[%u]: typeId='%s' name='%s'",
							r, registry.GetRegisteredTypeId(r).AsChar(), info.name);
					}
				}
			}
		}

		void PluginLoaderModule::LoadManifest(const char* manifestPath)
		{
			DIA_ASSERT(manifestPath != nullptr, "PluginLoaderModule: manifest path must not be null");
			DIA_LOG_INFO("Application", "PluginLoaderModule: Loading manifest '%s'", manifestPath);

			struct LoadCtx { PluginLoaderModule* module; const char* manifestPath; };
			LoadCtx ctx{ this, manifestPath };

			Dia::Editor::EditorManifestLoader::Load(manifestPath,
				[](const Dia::Editor::EditorManifestLoader::PluginEntry& entry, void* userData)
				{
					auto* c = static_cast<LoadCtx*>(userData);
					c->module->LoadPlugin(
						Dia::Core::StringCRC(entry.typeId),
						Dia::Core::StringCRC(entry.instanceId));
					Dia::Editor::EditorPluginRegistry::Instance().TagPluginManifest(
						Dia::Core::StringCRC(entry.typeId),
						Dia::Core::StringCRC(c->manifestPath));
				},
				&ctx);
		}

		void PluginLoaderModule::LoadPlugin(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& instanceId)
		{
			if (IsPluginTypeLoaded(typeId))
			{
				DIA_LOG_INFO("Application", "PluginLoaderModule::LoadPlugin: '%s' already loaded — calling OnNavigate", typeId.AsChar());
				for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
				{
					if (mLoadedPlugins[i].typeId == typeId)
					{
						mLoadedPlugins[i].plugin->OnNavigate(instanceId);
						break;
					}
				}
				return;
			}

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
			plugin->OnNavigate(instanceId);

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
					return true;
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

namespace { using PluginLoaderModule_ = Cluiche::Editor::PluginLoaderModule; }
DIA_MODULE(PluginLoaderModule_);
DIA_DESCRIBE(PluginLoaderModule_::kTypeId, "Discovers and hot-loads editor plugin DLLs, registering their module types at startup.");
