#pragma once

namespace Dia
{
	namespace Editor
	{
		class EditorModel;
		class WebUIBridge;

		struct EditorPluginContext
		{
			EditorModel* mModel;
			WebUIBridge* mBridge;

			EditorPluginContext()
				: mModel(nullptr)
				, mBridge(nullptr)
			{}
		};
	}
}
