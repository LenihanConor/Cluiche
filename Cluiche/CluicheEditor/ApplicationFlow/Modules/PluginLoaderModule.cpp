#include "PluginLoaderModule.h"

#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/EditorManifestLoader.h>
#include <DiaEditor/MVC/EditorView.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Core/Log.h>

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
			Dia::Core::Log::OutputVaradicLine("PluginLoaderModule: DoStart");
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
			Dia::Core::Log::OutputVaradicLine("PluginLoaderModule: DoStop - unloading %u plugins", mLoadedPlugins.Size());
			for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
			{
				Dia::Core::Log::OutputVaradicLine("PluginLoaderModule: OnUnload '%s'", mLoadedPlugins[i]->GetName());
				mLoadedPlugins[i]->OnUnload();
				delete mLoadedPlugins[i];
			}
			mLoadedPlugins.RemoveAll();
		}

		void PluginLoaderModule::LoadBuiltInPlugins()
		{
			Dia::Core::Log::OutputVaradicLine("PluginLoaderModule: Loading built-in plugins");
			LoadPlugin(Dia::Core::StringCRC("HomeEditorPlugin"), Dia::Core::StringCRC("home_builtin"));
			LoadPlugin(Dia::Core::StringCRC("OutputConsoleEditorPlugin"), Dia::Core::StringCRC("outputconsole_builtin"));
			LoadPlugin(Dia::Core::StringCRC("GameConnectionEditorPlugin"), Dia::Core::StringCRC("gameconnection_builtin"));
		}

		void PluginLoaderModule::LoadManifest(const char* manifestPath)
		{
			DIA_ASSERT(manifestPath != nullptr, "PluginLoaderModule: manifest path must not be null");
			Dia::Core::Log::OutputVaradicLine("PluginLoaderModule: Loading manifest '%s'", manifestPath);

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
			Dia::Core::Log::OutputVaradicLine("PluginLoaderModule::LoadPlugin: Creating plugin");

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

			Dia::Core::Log::OutputVaradicLine("PluginLoaderModule::LoadPlugin: Created '%s'", plugin->GetName());
			plugin->OnLoad(mContext);
			Dia::Core::Log::OutputVaradicLine("PluginLoaderModule::LoadPlugin: OnLoad complete for '%s'", plugin->GetName());

			if (mView != nullptr)
			{
				mView->RegisterComponent(plugin->GetName(), plugin->GetUIPath());
				Dia::Core::Log::OutputVaradicLine("PluginLoaderModule::LoadPlugin: Registered '%s' at '%s'", plugin->GetName(), plugin->GetUIPath());
			}

			mLoadedPlugins.Add(plugin);
		}

		void PluginLoaderModule::RegisterView(Dia::Editor::EditorView* view)
		{
			mView = view;

			for (unsigned int i = 0; i < mLoadedPlugins.Size(); ++i)
			{
				mView->RegisterComponent(mLoadedPlugins[i]->GetName(), mLoadedPlugins[i]->GetUIPath());
				Dia::Core::Log::OutputVaradicLine("PluginLoaderModule: Retroactively registered '%s'", mLoadedPlugins[i]->GetName());
			}
		}
	}
}
