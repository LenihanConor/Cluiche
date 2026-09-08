#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/Project/ProjectContext.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace Editor
	{
		struct EditorPluginMetadata
		{
			const char* name;
			const char* version;
			const char* description;
			const char* uiPath;
			LayoutMode  layoutMode;
			const char* dirtyTopic;
			const char* iconChar;
			bool        pinned;
		};

		class EditorPluginBase : public IEditorPlugin
		{
		public:
			explicit EditorPluginBase(const EditorPluginMetadata& meta);
			virtual ~EditorPluginBase() = default;

			const char* GetName() const override final;
			const char* GetVersion() const override final;
			const char* GetDescription() const override final;
			const char* GetUIPath() const override final;
			LayoutMode GetLayoutMode() const override final;
			EditorToolbarItem GetToolbarItem() const override final;

			void OnLoad(const EditorPluginContext& context) override final;
			void OnUnload() override final;
			void OnUpdate(float deltaTime) override;

			static Json::Value MakeSuccessResponse();
			static Json::Value MakeSuccessResponse(const Json::Value& data);
			static Json::Value MakeErrorResponse(const char* message);

		protected:
			virtual void OnPluginLoad() {}
			virtual void OnPluginUnload() {}

			virtual void OnProjectChanged(const ProjectContext& context) { (void)context; }

			void RegisterHandler(const Dia::Core::StringCRC& requestType,
				WebUIBridge::RequestHandler handler);
			void RegisterEvent(const Dia::Core::StringCRC& eventType,
				WebUIBridge::EventHandler handler);

			bool IsDirty() const;
			void MarkDirty();
			void ClearDirty();

			WebUIBridge* GetBridge() const;
			IPluginLoader* GetPluginLoader() const;
			PluginServiceLocator* GetServices() const;
			EditorModel* GetModel() const;
			EditorView* GetView() const;
			const char* GetProjectPath() const;

		private:
			static void ProjectChangedTrampoline(const ProjectContext& ctx, void* userData);
			void UnregisterAllHandlers();

			EditorPluginMetadata mMeta;

			WebUIBridge* mBridge;
			IPluginLoader* mPluginLoader;
			PluginServiceLocator* mServices;
			EditorModel* mModel;
			EditorView* mView;
			const char* mProjectPath;

			static const unsigned int kMaxTrackedHandlers = 64;
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxTrackedHandlers> mTrackedRequestHandlers;
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxTrackedHandlers> mTrackedEventHandlers;

			bool mIsDirty;
			bool mSubscribedToProject;
		};
	}
}
