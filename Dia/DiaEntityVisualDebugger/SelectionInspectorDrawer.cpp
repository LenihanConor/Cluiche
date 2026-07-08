#ifdef DIA_DEBUG

#include "SelectionInspectorDrawer.h"
#include <DiaEntity/IEntityInspectable.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaEntity/ComponentRegistry.h>
#include <DiaEntity/ComponentTypeDesc.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <imgui.h>

namespace Dia::EntityVisualDebugger
{

SelectionInspectorDrawer::SelectionInspectorDrawer(
    Dia::Entity::IEntityInspectable& inspectable,
    Dia::Entity::Domain& domain,
    const Dia::Debug::DebugLayerManager& manager)
    : mInspectable(inspectable)
    , mDomain(domain)
    , mManager(manager)
{
}

Dia::Core::StringCRC SelectionInspectorDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kEntityInspector;
}

void SelectionInspectorDrawer::Draw(Dia::Core::IDebugDraw& /*draw*/)
{
}

void SelectionInspectorDrawer::DrawImGui()
{
    ImGui::SetNextWindowPos(ImVec2(10.f, 300.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.f, 350.f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Entity Inspector");

    uint32_t selectedId = mManager.GetSelectedEntityId();
    if (selectedId == 0)
    {
        ImGui::TextDisabled("No entity selected");
        ImGui::End();
        return;
    }

    Dia::Entity::Entity entity = mDomain.GetAliveEntity(selectedId - 1);
    if (!entity.IsValid())
    {
        ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Entity no longer alive");
        ImGui::End();
        return;
    }

    const char* name = mDomain.GetDebugName(entity);
    ImGui::Text("Name: %s", name ? name : "(unnamed)");
    ImGui::Text("ID: %u", selectedId);
    ImGui::Separator();

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> typeIds;
    mInspectable.GetComponentTypeIds(entity, typeIds);

    auto& registry = Dia::Entity::ComponentRegistry::Get();

    for (unsigned int i = 0; i < typeIds.Size(); ++i)
    {
        const auto* desc = registry.Find(typeIds[i]);
        const char* compName = desc ? desc->debugName : "Unknown";

        if (ImGui::TreeNode(compName))
        {
            if (desc)
            {
                for (uint16_t f = 0; f < desc->fieldCount; ++f)
                {
                    const auto& field = desc->fields[f];
                    Json::Value val;
                    if (mInspectable.ReadField(entity, typeIds[i], field.name, val))
                    {
                        ImGui::Text("  %s = %s", field.name, val.toStyledString().c_str());
                    }
                    else
                    {
                        ImGui::TextDisabled("  %s = (read failed)", field.name);
                    }
                }
            }
            ImGui::TreePop();
        }
    }

    ImGui::End();
}

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
