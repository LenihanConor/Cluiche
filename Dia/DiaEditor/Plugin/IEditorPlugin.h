#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Editor
	{
		class EditorModel;

		enum class LayoutMode
		{
			kFullScreen,
			kDockable
		};

		class IEditorPlugin
		{
		public:
			virtual ~IEditorPlugin() = default;

			virtual const char* GetName() const = 0;
			virtual const char* GetVersion() const = 0;
			virtual const char* GetDescription() const = 0;
			virtual const char* GetUIPath() const = 0;
			virtual LayoutMode GetLayoutMode() const = 0;

			virtual void OnLoad(EditorModel* model) = 0;
			virtual void OnUnload() = 0;
			virtual void OnUpdate(float deltaTime) = 0;
		};

		class IEditorPluginFactory
		{
		public:
			virtual ~IEditorPluginFactory() = default;
			virtual IEditorPlugin* Create() = 0;
		};
	}
}
