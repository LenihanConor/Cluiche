#pragma once

namespace Dia
{
	namespace Editor
	{
		class EditorModel;
		class EditorView;
		class WebUIBridge;
		class IPluginLoader;
		class PluginServiceLocator;

		struct EditorPluginContext
		{
			EditorModel* mModel;
			EditorView* mView;
			WebUIBridge* mBridge;
			IPluginLoader* mPluginLoader;
			PluginServiceLocator* mServices;

			EditorPluginContext()
				: mModel(nullptr)
				, mView(nullptr)
				, mBridge(nullptr)
				, mPluginLoader(nullptr)
				, mServices(nullptr)
			{}
		};
	}
}
