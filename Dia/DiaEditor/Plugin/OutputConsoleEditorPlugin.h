#pragma once

#include <DiaEditor/Plugin/EditorPluginBase.h>

namespace Dia
{
	namespace Editor
	{
		class OutputConsoleEditorPlugin : public EditorPluginBase
		{
		public:
			OutputConsoleEditorPlugin()
				: EditorPluginBase({
					"Output Console",
					"1.0",
					"Built-in output console panel",
					"dia://plugins/outputconsole/index.html",
					LayoutMode::kDockable,
					nullptr,
					"C",
					true
				})
			{}
		};
	}
}
