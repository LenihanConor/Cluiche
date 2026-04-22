#include "PluginLoaderModule.h"

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

		PluginLoaderModule::PluginLoaderModule(Dia::Application::ProcessingUnit* pu, Dia::Editor::EditorModel* model)
			: Dia::Application::Module(pu, kTypeId, RunningEnum::kUpdate)
			, mView(nullptr)
		{
			mContext.mModel = model;
		}

		void PluginLoaderModule::SetBridge(Dia::Editor::WebUIBridge* bridge)
		{
			mContext.mBridge = bridge;
		}

		Dia::Application::StateObject::OpertionResponse PluginLoaderModule::DoStart(const Dia::Application::StateObject::IStartData*)
		{
			DIA_LOG_INFO("Application", "PluginLoaderModule: DoStart");
			return Dia::Application::StateObject::OpertionResponse::kImmediate;
		}

		void PluginLoaderModule::DoUpdate()
		{
			static const float kFixedDeltaTime = 1.0f / 60.0f;
			for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
			{
				mLoadedPlugins[i]->OnUpdate(kFixedDeltaTime);
			}
		}

		void PluginLoaderModule::DoStop()
		{
			DIA_LOG_INFO("Application", "PluginLoaderModule: DoStop - unloading %u plugins", mLoadedPlugins.Size());
			for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
			{
				DIA_LOG_INFO("Application", "PluginLoaderModule: OnUnload '%s'", mLoadedPlugins[i]->GetName());
				mLoadedPlugins[i]->OnUnload();
				delete mLoadedPlugins[i];
			}
			mLoadedPlugins.RemoveAll();
		}

		void PluginLoaderModule::LoadBuiltInPlugins()
		{
			DIA_LOG_INFO("Application", "PluginLoaderModule: Loading built-in plugins");
			LoadPlugin(Dia::Core::StringCRC("HomeEditorPlugin"), Dia::Core::StringCRC("home_builtin"));
			LoadPlugin(Dia::Core::StringCRC("OutputConsoleEditorPlugin"), Dia::Core::StringCRC("outputconsole_builtin"));
			LoadPlugin(Dia::Core::StringCRC("GameConnectionEditorPlugin"), Dia::Core::StringCRC("gameconnection_builtin"));
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

			mLoadedPlugins.Add(plugin);
		}

		void PluginLoaderModule::RegisterView(Dia::Editor::EditorView* view)
		{
			mView = view;
			mContext.mView = view;

			for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
			{
				mView->RegisterComponent(mLoadedPlugins[i]->GetName(), mLoadedPlugins[i]->GetUIPath());
				DIA_LOG_INFO("Application", "PluginLoaderModule: Retroactively registered '%s'", mLoadedPlugins[i]->GetName());
			}
		}
	}
}
