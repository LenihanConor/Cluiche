#pragma once

namespace Dia
{
	namespace Editor
	{
		class EditorModel;
		class EditorView;
		class WebUIBridge;

		struct EditorPluginContext
		{
			EditorModel* mModel;
			EditorView* mView;
			WebUIBridge* mBridge;

			EditorPluginContext()
				: mModel(nullptr)
				, mView(nullptr)
				, mBridge(nullptr)
			{}
		};
	}
}
