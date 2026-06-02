#ifdef DIA_DEBUG

#include "EntityLabelsDrawer.h"
#include "EntityPositionHelper.h"
#include <DiaEntity/IEntityInspectable.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <imgui.h>

namespace Dia::EntityVisualDebugger
{

EntityLabelsDrawer::EntityLabelsDrawer(
    Dia::Entity::IEntityInspectable& inspectable,
    Dia::Entity::Domain& domain,
    const Dia::Debug::DebugLayerManager& manager,
    Dia::Core::StringCRC positionComponentTypeId)
    : mInspectable(inspectable)
    , mDomain(domain)
    , mManager(manager)
    , mPositionTypeId(positionComponentTypeId)
{
}

Dia::Core::StringCRC EntityLabelsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kEntityLabels;
}

void EntityLabelsDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, Dia::Entity::kMaxEntitiesPerDomain> entities;
    mInspectable.GetAllEntities(entities);

    const float scale = mManager.GetDebugScale();
    const float fontSize = mFontSize * scale;

    for (unsigned int i = 0; i < entities.Size(); ++i)
    {
        Dia::Entity::Entity entity = entities[i];
        const char* name = mDomain.GetDebugName(entity);
        if (!name) continue;

        if (!EntityPositionHelper::GlobMatch(mFilterPattern, name))
            continue;

        Dia::Maths::Vector2D pos = EntityPositionHelper::GetPosition(mInspectable, entity, mPositionTypeId);
        frameData.RequestDrawText(pos, name, fontSize, Dia::Debug::DebugColourPalette::kActive);
    }
}

void EntityLabelsDrawer::SetFilter(const char* pattern)
{
    if (!pattern) return;
    strncpy_s(mFilterPattern, pattern, sizeof(mFilterPattern) - 1);
    mFilterPattern[sizeof(mFilterPattern) - 1] = '\0';
}

void EntityLabelsDrawer::DrawImGui()
{
    ImGui::InputText("Filter", mFilterPattern, sizeof(mFilterPattern));
    ImGui::SliderFloat("Font size", &mFontSize, 6.0f, 32.0f);
}

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
