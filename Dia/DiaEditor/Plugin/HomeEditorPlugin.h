#pragma once

#include <DiaEditor/Plugin/EditorPluginBase.h>

namespace Dia
{
	namespace Editor
	{
		class HomeEditorPlugin : public EditorPluginBase
		{
		public:
			HomeEditorPlugin()
				: EditorPluginBase({
					"Home",
					"1.0",
					"Built-in Home panel",
					"dia://plugins/home/index.html",
					LayoutMode::kDockable,
					nullptr,
					nullptr,
					false,
					false
				})
			{}
		};
	}
}
