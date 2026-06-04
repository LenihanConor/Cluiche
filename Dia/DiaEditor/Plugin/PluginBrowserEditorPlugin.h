#pragma once

#include <DiaEditor/Plugin/EditorPluginBase.h>

namespace Dia
{
	namespace Editor
	{
		class PluginBrowserEditorPlugin : public EditorPluginBase
		{
		public:
			PluginBrowserEditorPlugin()
				: EditorPluginBase({
					"Plugin Browser",
					"1.0",
					"Browse and load available editor plugins",
					"dia://plugins/pluginbrowser/index.html",
					LayoutMode::kDockable,
					nullptr,
					"P",
					true
				})
			{}

		protected:
			void OnPluginLoad() override;
		};
	}
}
