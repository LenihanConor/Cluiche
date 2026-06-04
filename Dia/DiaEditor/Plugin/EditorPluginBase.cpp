#include "DiaEditor/Plugin/EditorPluginBase.h"
#include "DiaEditor/MVC/EditorModel.h"

namespace Dia
{
	namespace Editor
	{
		EditorPluginBase::EditorPluginBase(const EditorPluginMetadata& meta)
			: mMeta(meta)
			, mBridge(nullptr)
			, mPluginLoader(nullptr)
			, mServices(nullptr)
			, mModel(nullptr)
			, mView(nullptr)
			, mProjectPath(nullptr)
			, mIsDirty(false)
			, mSubscribedToProject(false)
		{}

		const char* EditorPluginBase::GetName() const { return mMeta.name; }
		const char* EditorPluginBase::GetVersion() const { return mMeta.version; }
		const char* EditorPluginBase::GetDescription() const { return mMeta.description; }
		const char* EditorPluginBase::GetUIPath() const { return mMeta.uiPath; }
		LayoutMode EditorPluginBase::GetLayoutMode() const { return mMeta.layoutMode; }

		EditorToolbarItem EditorPluginBase::GetToolbarItem() const
		{
			EditorToolbarItem item;
			if (mMeta.iconChar != nullptr)
			{
				strncpy_s(item.iconChar, sizeof(item.iconChar), mMeta.iconChar, _TRUNCATE);
			}
			else if (mMeta.name != nullptr && mMeta.name[0] != '\0')
			{
				item.iconChar[0] = mMeta.name[0];
				item.iconChar[1] = '\0';
			}
			if (mMeta.name != nullptr)
			{
				strncpy_s(item.label, sizeof(item.label), mMeta.name, _TRUNCATE);
			}
			item.pinned = mMeta.pinned;
			return item;
		}

		void EditorPluginBase::OnLoad(const EditorPluginContext& context)
		{
			mBridge = context.mBridge;
			mPluginLoader = context.mPluginLoader;
			mServices = context.mServices;
			mModel = context.mModel;
			mView = context.mView;
			mProjectPath = context.mProjectPath;

			if (mModel != nullptr)
			{
				mModel->OnDiagameProjectChanged(&EditorPluginBase::ProjectChangedTrampoline, this);
				mSubscribedToProject = true;
			}

			OnPluginLoad();
		}

		void EditorPluginBase::OnUnload()
		{
			OnPluginUnload();
			UnregisterAllHandlers();

			mBridge = nullptr;
			mPluginLoader = nullptr;
			mServices = nullptr;
			mModel = nullptr;
			mView = nullptr;
			mProjectPath = nullptr;
			mIsDirty = false;
			mSubscribedToProject = false;
		}

		void EditorPluginBase::OnUpdate(float /*deltaTime*/) {}

		void EditorPluginBase::RegisterHandler(
			const Dia::Core::StringCRC& requestType,
			WebUIBridge::RequestHandler handler)
		{
			if (mBridge != nullptr)
			{
				mBridge->RegisterRequestHandler(requestType, handler);
				mTrackedRequestHandlers.Add(requestType);
			}
		}

		void EditorPluginBase::RegisterEvent(
			const Dia::Core::StringCRC& eventType,
			WebUIBridge::EventHandler handler)
		{
			if (mBridge != nullptr)
			{
				mBridge->RegisterEventHandler(eventType, handler);
				mTrackedEventHandlers.Add(eventType);
			}
		}

		Json::Value EditorPluginBase::MakeSuccessResponse()
		{
			Json::Value result;
			result["success"] = true;
			return result;
		}

		Json::Value EditorPluginBase::MakeSuccessResponse(const Json::Value& data)
		{
			Json::Value result;
			result["success"] = true;
			result["data"] = data;
			return result;
		}

		Json::Value EditorPluginBase::MakeErrorResponse(const char* message)
		{
			Json::Value result;
			result["success"] = false;
			result["error"] = message != nullptr ? message : "Unknown error";
			return result;
		}

		bool EditorPluginBase::IsDirty() const { return mIsDirty; }

		void EditorPluginBase::MarkDirty()
		{
			mIsDirty = true;
			if (mBridge != nullptr && mMeta.dirtyTopic != nullptr)
			{
				Json::Value payload(true);
				mBridge->NotifyUIDataChanged(mMeta.dirtyTopic, payload);
			}
		}

		void EditorPluginBase::ClearDirty()
		{
			mIsDirty = false;
			if (mBridge != nullptr && mMeta.dirtyTopic != nullptr)
			{
				Json::Value payload(false);
				mBridge->NotifyUIDataChanged(mMeta.dirtyTopic, payload);
			}
		}

		WebUIBridge* EditorPluginBase::GetBridge() const { return mBridge; }
		IPluginLoader* EditorPluginBase::GetPluginLoader() const { return mPluginLoader; }
		PluginServiceLocator* EditorPluginBase::GetServices() const { return mServices; }
		EditorModel* EditorPluginBase::GetModel() const { return mModel; }
		EditorView* EditorPluginBase::GetView() const { return mView; }
		const char* EditorPluginBase::GetProjectPath() const { return mProjectPath; }

		void EditorPluginBase::ProjectChangedTrampoline(const ProjectContext& ctx, void* userData)
		{
			auto* self = static_cast<EditorPluginBase*>(userData);
			self->OnProjectChanged(ctx);
		}

		void EditorPluginBase::UnregisterAllHandlers()
		{
			if (mBridge != nullptr)
			{
				for (unsigned int i = 0; i < mTrackedRequestHandlers.Size(); ++i)
				{
					mBridge->UnregisterRequestHandler(mTrackedRequestHandlers[i]);
				}

				for (unsigned int i = 0; i < mTrackedEventHandlers.Size(); ++i)
				{
					mBridge->UnregisterEventHandler(mTrackedEventHandlers[i]);
				}
			}

			mTrackedRequestHandlers.RemoveAll();
			mTrackedEventHandlers.RemoveAll();
		}
	}
}
