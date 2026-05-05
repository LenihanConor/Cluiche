#include "DiaAssetCatalogueEditor/DiaAssetCatalogueEditorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/MVC/EditorView.h>
#include <DiaLogger/DiaLog.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			void DiaAssetCatalogueEditorPlugin::OnLoad(const Dia::Editor::EditorPluginContext& context)
			{
				DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: OnLoad");
				mRelationshipIndex = &mRegistry.GetRelationshipIndex();
				context.mView->RegisterComponent("AssetCatalogue", GetUIPath());
				DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: Initialized");
			}

			void DiaAssetCatalogueEditorPlugin::OnUnload()
			{
				DIA_LOG_INFO("Editor", "DiaAssetCatalogueEditorPlugin: OnUnload");
				mRelationshipIndex = nullptr;
			}

			void DiaAssetCatalogueEditorPlugin::OnUpdate(float /*deltaTime*/)
			{
			}
		}
	}
}

using namespace Dia::AssetCatalogue::Editor;

REGISTER_EDITOR_PLUGIN(DiaAssetCatalogueEditorPlugin, "DiaAssetCatalogueEditorPlugin")
