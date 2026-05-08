#pragma once

namespace Dia
{
	namespace Editor
	{
		class EditorModel;
		class EditorView;
		class WebUIBridge;
		class IPluginLoader;

		struct EditorPluginContext
		{
			EditorModel* mModel;
			EditorView* mView;
			WebUIBridge* mBridge;
			IPluginLoader* mPluginLoader;

			EditorPluginContext()
				: mModel(nullptr)
				, mView(nullptr)
				, mBridge(nullptr)
				, mPluginLoader(nullptr)
			{}
		};
	}
}
