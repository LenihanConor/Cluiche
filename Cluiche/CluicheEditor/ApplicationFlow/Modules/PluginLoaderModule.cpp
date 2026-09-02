#include "PluginLoaderModule.h"
#include "EditorModelModule.h"
#include "EditorViewModule.h"
#include "EditorActionModule.h"

#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/EditorManifestLoader.h>
#include <DiaEditor/MVC/EditorView.h>
#include <DiaEditor/Layout/DockingLayout.h>
#include <DiaEditor/Memory/EditorMemory.h>
#include <DiaEditor/EditorAPI/EditorActionPythonModule.h>
#include <DiaEditor/EditorAPI/EditorActionQueueService.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <string>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC PluginLoaderModule::kTypeId("PluginLoaderModule");

		PluginLoaderModule::PluginLoaderModule(const Dia::Core::StringCRC& instanceId)
			: Dia::ApplicationFlow::MainModule(instanceId)
			, mView(nullptr)
			, mModelRef(this, EditorModelModule::kTypeId)
			, mViewRef(this, EditorViewModule::kTypeId)
			, mActionModuleRef(this, EditorActionModule::kTypeId)
		{
			mContext.mPluginLoader = this;
			mContext.mServices = &mServiceLocator;
		}

		void PluginLoaderModule::SetBridge(Dia::Editor::WebUIBridge* bridge)
		{
			mContext.mBridge = bridge;
			mNotificationService.Initialize(bridge);
			if (!mServiceLocator.HasService<Dia::Editor::NotificationService>())
				mServiceLocator.RegisterService(&mNotificationService);

			// Register EditorActionRegistry and EditorActionQueue on the service locator.
			EditorActionModule* actionModule = mActionModuleRef.Get();
			if (actionModule != nullptr && !mServiceLocator.HasService<Dia::Editor::EditorActionRegistryService>())
			{
				mRegistryService = new Dia::Editor::EditorActionRegistryService(actionModule->GetRegistry());
				mServiceLocator.RegisterService(mRegistryService);
			}
			if (actionModule != nullptr && !mServiceLocator.HasService<Dia::Editor::EditorActionQueueService>())
			{
				mQueueService = new Dia::Editor::EditorActionQueueService(actionModule->GetQueue());
				mServiceLocator.RegisterService(mQueueService);
			}

			// Initialize AppEditorController — needs bridge, context, plugin loader, and registry.
			// Context comes from EditorModelModule; registry from EditorActionModule (may be null).
			if (!mServiceLocator.HasService<Dia::Editor::AppEditorController>())
			{
				EditorModelModule* modelModule = mModelRef.Get();
				Dia::Editor::IEditorContext* ctx = (modelModule != nullptr) ? &modelModule->GetModel() : nullptr;
				Dia::Editor::EditorActionRegistry* api = (actionModule != nullptr) ? actionModule->GetRegistry() : nullptr;
				mAppEditorController.Initialize(bridge, ctx, this, api);
				mServiceLocator.RegisterService(&mAppEditorController);
			}
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
			PruneHeadlessPanels();

			// Restore plugins + layout from .memory.json (previous session)
			static const char* kMemoryPath = "../../../../out/CluicheEditor/.memory.json";
			Dia::Editor::EditorMemory memory;
			bool memoryLoaded = memory.Load(kMemoryPath);

			if (memoryLoaded)
			{
				DIA_LOG_INFO("Application", "PluginLoaderModule: Restoring %u plugins from memory", memory.GetPluginCount());
				Dia::Editor::EditorPluginRegistry& registry = Dia::Editor::EditorPluginRegistry::Instance();
				for (unsigned int i = 0; i < memory.GetPluginCount(); ++i)
				{
					const Dia::Editor::MemoryPluginEntry& entry = memory.GetPlugin(i);
					Dia::Core::StringCRC typeId(entry.typeId);
					if (!registry.IsPluginRegistered(typeId))
					{
						DIA_LOG_WARNING("Application", "PluginLoaderModule: Skipping unregistered plugin '%s' from memory (AC3)", entry.typeId);
						continue;
					}
					if (!IsPluginTypeLoaded(typeId))
					{
						Dia::Core::StringCRC instanceId(entry.instanceId);
						LoadPlugin(typeId, instanceId);
					}
				}

				if (mView != nullptr && !memory.GetLayoutTree().isNull())
				{
					Dia::Editor::DockingLayout* layout = mView->GetDockingLayout();
					if (layout != nullptr)
					{
						const Json::Value& tree = memory.GetLayoutTree();
						if (tree.isMember("tree") && !tree["tree"].isNull())
							layout->SetMosaicTree(tree["tree"]);
						DIA_LOG_INFO("Application", "PluginLoaderModule: Restored layout from memory");
					}
				}
			}
			else
			{
				DIA_LOG_INFO("Application", "PluginLoaderModule: No memory file found, starting fresh");
			}

			// Load the project (CLI arg takes priority; fall back to memory.last_project)
			if (modelModule != nullptr)
			{
				const char* projectPath = modelModule->GetProjectPath();
				bool hasCliProject = (projectPath != nullptr && projectPath[0] != '\0');
				bool hasMemoryProject = (memoryLoaded && memory.GetLastProject() != nullptr && memory.GetLastProject()[0] != '\0');

				const char* effectiveProject = hasCliProject ? projectPath
				                             : hasMemoryProject ? memory.GetLastProject()
				                             : nullptr;

				if (effectiveProject != nullptr && effectiveProject[0] != '\0')
				{
					DIA_LOG_INFO("Application", "PluginLoaderModule: Loading project '%s'", effectiveProject);
					Dia::Editor::EditorModel& model = modelModule->GetModel();
					model.LoadProject(effectiveProject);

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

			// Generate the dia_editor Python module now that all plugins have registered actions.
			EditorActionModule* actionModule = mActionModuleRef.Get();
			if (actionModule != nullptr)
			{
				Dia::Editor::GeneratePythonModule(
					actionModule->GetRegistry(),
					actionModule->GetQueue());

				Dia::Editor::EmitPythonStubs(
					actionModule->GetRegistry(),
					"../../../../out/CluicheEditor/scripts/dia_editor.pyi");
			}

			return Dia::ApplicationFlow::StartResult::kReady;
		}

		void PluginLoaderModule::DoUpdate(const Dia::SimTime::MainTimeContext& ctx)
		{
			const float deltaTime = ctx.wallClockDt;
			for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
			{
				mLoadedPlugins[i].plugin->OnUpdate(deltaTime);
			}
		}

		Dia::ApplicationFlow::StopResult PluginLoaderModule::DoStop()
		{
			// --- Save editor memory before unloading plugins ---
			Dia::Editor::EditorMemory memory;

			const Dia::Core::StringCRC kBuiltinHome("HomeEditorPlugin");
			const Dia::Core::StringCRC kBuiltinOutput("OutputConsoleEditorPlugin");
			const Dia::Core::StringCRC kBuiltinGameConn("GameConnectionEditorPlugin");
			const Dia::Core::StringCRC kBuiltinBrowser("PluginBrowserEditorPlugin");

			for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
			{
				const Dia::Core::StringCRC& typeId = mLoadedPlugins[i].typeId;
				if (typeId == kBuiltinHome || typeId == kBuiltinOutput ||
					typeId == kBuiltinGameConn || typeId == kBuiltinBrowser)
					continue;
				memory.AddPlugin(typeId.AsChar(), typeId.AsChar());
			}

			if (mView != nullptr)
			{
				Dia::Editor::DockingLayout* layout = mView->GetDockingLayout();
				if (layout != nullptr)
				{
					Json::Value layoutTree;
					const Json::Value& mosaicTree = layout->GetMosaicTree();
					if (!mosaicTree.isNull())
						layoutTree["tree"] = mosaicTree;
					memory.SetLayoutTree(layoutTree);
				}
			}

			EditorModelModule* modelModule = mModelRef.Get();
			if (modelModule != nullptr)
			{
				const char* projectPath = modelModule->GetProjectPath();
				if (projectPath != nullptr && projectPath[0] != '\0')
					memory.SetLastProject(projectPath);
			}

			static const char* kMemoryPath = "../../../../out/CluicheEditor/.memory.json";
			if (memory.Save(kMemoryPath))
				DIA_LOG_INFO("Application", "PluginLoaderModule: Saved editor memory to '%s'", kMemoryPath);
			else
				DIA_LOG_WARNING("Application", "PluginLoaderModule: Failed to save editor memory to '%s'", kMemoryPath);

			// --- Unload plugins ---
			DIA_LOG_INFO("Application", "PluginLoaderModule: DoStop - unloading %u plugins", mLoadedPlugins.Size());
			for (int i = static_cast<int>(mLoadedPlugins.Size()) - 1; i >= 0; --i)
			{
				DIA_LOG_INFO("Application", "PluginLoaderModule: OnUnload '%s'", mLoadedPlugins[i].plugin->GetName());
				mLoadedPlugins[i].plugin->OnUnload();
				delete mLoadedPlugins[i].plugin;
			}
			mLoadedPlugins.RemoveAll();

			if (mServiceLocator.HasService<Dia::Editor::AppEditorController>())
			{
				mServiceLocator.UnregisterService<Dia::Editor::AppEditorController>();
				mAppEditorController.Shutdown();
			}

			if (mRegistryService != nullptr)
			{
				mServiceLocator.UnregisterService<Dia::Editor::EditorActionRegistryService>();
				delete mRegistryService;
				mRegistryService = nullptr;
			}

			if (mQueueService != nullptr)
			{
				mServiceLocator.UnregisterService<Dia::Editor::EditorActionQueueService>();
				delete mQueueService;
				mQueueService = nullptr;
			}

			return Dia::ApplicationFlow::StopResult::kDone;
		}

		void PluginLoaderModule::PruneHeadlessPanels()
		{
			if (mView == nullptr)
				return;

			Dia::Editor::DockingLayout* layout = mView->GetDockingLayout();
			if (layout == nullptr)
				return;

			Dia::Editor::EditorPluginRegistry& registry = Dia::Editor::EditorPluginRegistry::Instance();
			const unsigned int registryCount = registry.GetRegisteredCount();

			unsigned int p = 0;
			unsigned int pruned = 0;
			while (p < layout->GetPanelCount())
			{
				const char* panelName = layout->GetPanel(p).name;
				bool isHeadless = false;

				for (unsigned int r = 0; r < registryCount; ++r)
				{
					Dia::Editor::EditorPluginInfo info = registry.GetFactory(r)->GetPluginInfo();
					if (strcmp(info.name, panelName) == 0)
					{
						if (info.layoutMode == Dia::Editor::LayoutMode::kHeadless)
							isHeadless = true;
						break;
					}
				}

				if (isHeadless)
				{
					DIA_LOG_INFO("Application", "PluginLoaderModule::PruneHeadlessPanels: removing stale headless panel '%s'", panelName);
					layout->RemovePanel(panelName);
					++pruned;
				}
				else
				{
					++p;
				}
			}

			if (pruned > 0)
				DIA_LOG_INFO("Application", "PluginLoaderModule::PruneHeadlessPanels: pruned %u stale headless panel(s)", pruned);
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
							LoadPlugin(typeId, Dia::Core::StringCRC::kZero);
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

			if (mView != nullptr && plugin->GetLayoutMode() != Dia::Editor::LayoutMode::kHeadless)
			{
				mView->RegisterComponent(plugin->GetName(), plugin->GetUIPath());
				DIA_LOG_INFO("Application", "PluginLoaderModule::LoadPlugin: Registered '%s' at '%s'", plugin->GetName(), plugin->GetUIPath());
			}
			else if (plugin->GetLayoutMode() == Dia::Editor::LayoutMode::kHeadless)
			{
				DIA_LOG_INFO("Application", "PluginLoaderModule::LoadPlugin: Skipping panel registration for headless plugin '%s'", plugin->GetName());
			}

			LoadedPluginEntry entry;
			entry.typeId = typeId;
			entry.plugin = plugin;
			mLoadedPlugins.Add(entry);

			// Regenerate dia_editor Python module so newly registered actions are callable.
			EditorActionModule* actionModule = mActionModuleRef.Get();
			if (actionModule != nullptr)
			{
				Dia::Editor::GeneratePythonModule(
					actionModule->GetRegistry(),
					actionModule->GetQueue());
			}

			if (mView != nullptr && plugin->GetLayoutMode() != Dia::Editor::LayoutMode::kHeadless)
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
				if (mLoadedPlugins[i].plugin->GetLayoutMode() == Dia::Editor::LayoutMode::kHeadless)
					continue;
				mView->RegisterComponent(mLoadedPlugins[i].plugin->GetName(), mLoadedPlugins[i].plugin->GetUIPath());
				DIA_LOG_INFO("Application", "PluginLoaderModule: Retroactively registered '%s'", mLoadedPlugins[i].plugin->GetName());
			}
		}
	}
}

namespace { using PluginLoaderModule_ = Cluiche::Editor::PluginLoaderModule; }
DIA_MODULE(PluginLoaderModule_);
DIA_DESCRIBE(PluginLoaderModule_::kTypeId, "Discovers and hot-loads editor plugin DLLs, registering their module types at startup.");
