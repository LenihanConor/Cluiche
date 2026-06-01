#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/EntityTestDrawer.h"
#include "Modules/TestStages/Entity/TransformComponent.h"
#include "Modules/TestStages/Entity/VisualTestRenderComponent.h"
#include "Modules/TestStages/Entity/PickableCircleComponent.h"
#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/Line.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <imgui.h>

namespace CluicheTest {

static Dia::Graphics::RGBA ColourFromUint32(uint32_t rgba)
{
    return Dia::Graphics::RGBA(
        static_cast<uint8_t>((rgba >> 24) & 0xFF),
        static_cast<uint8_t>((rgba >> 16) & 0xFF),
        static_cast<uint8_t>((rgba >>  8) & 0xFF),
        static_cast<uint8_t>((rgba      ) & 0xFF));
}

EntityTestDrawer::EntityTestDrawer(
    Dia::Entity::Domain&              domain,
    Dia::Entity::Entity               parent,
    Dia::Entity::Entity               childA,
    Dia::Entity::Entity               childB,
    Dia::Entity::Entity               childC,
    Dia::Entity::Entity               queryEntities[4],
    Dia::Entity::Entity               doomed,
    const bool&                       doomedDestroyed,
    const bool&                       hasSelection,
    const unsigned int&               selectedIdx,
    const Dia::Entity::Entity*        allEntities,
    unsigned int                      entityCount,
    const Dia::Debug::DebugLayerManager& mgr)
    : mDomain(domain)
    , mParent(parent)
    , mChildA(childA)
    , mChildB(childB)
    , mChildC(childC)
    , mDoomed(doomed)
    , mDoomedDestroyed(doomedDestroyed)
    , mHasSelection(hasSelection)
    , mSelectedIdx(selectedIdx)
    , mAllEntities(allEntities)
    , mEntityCount(entityCount)
    , mManager(mgr)
{
    mQueryEntities[0] = queryEntities[0];
    mQueryEntities[1] = queryEntities[1];
    mQueryEntities[2] = queryEntities[2];
    mQueryEntities[3] = queryEntities[3];
}

Dia::Core::StringCRC EntityTestDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("entity.shapes");
}

void EntityTestDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    Dia::Geometry2DVisualDebugger::ShapeDrawer drawer(mManager);

    static const Dia::Graphics::RGBA kSelectOutline(255, 255, 0, 255);

    auto submitEntity = [&](Dia::Entity::Entity e, unsigned int idx)
    {
        if (!mDomain.IsAlive(e)) return;
        auto* tc  = mDomain.GetComponent<TransformComponent>(e);
        auto* vrc = mDomain.GetComponent<VisualTestRenderComponent>(e);
        if (!tc || !vrc) return;

        Dia::Geometry2D::Circle circle(vrc->radius, Dia::Maths::Vector2D(tc->x, tc->y));
        drawer.SubmitCircle(circle, ColourFromUint32(vrc->colour));

        // Selection highlight
        if (mHasSelection && mSelectedIdx == idx)
        {
            Dia::Geometry2D::Circle outline(vrc->radius + 4.f, Dia::Maths::Vector2D(tc->x, tc->y));
            drawer.SubmitCircle(outline, kSelectOutline);
        }
    };

    submitEntity(mParent,  0);
    submitEntity(mChildA,  1);
    submitEntity(mChildB,  2);
    submitEntity(mChildC,  3);

    // Hierarchy lines
    {
        auto* ptc = mDomain.GetComponent<TransformComponent>(mParent);
        if (ptc)
        {
            const Dia::Entity::Entity children[] = { mChildA, mChildB, mChildC };
            for (auto child : children)
            {
                auto* ctc = mDomain.GetComponent<TransformComponent>(child);
                if (!ctc) continue;
                Dia::Geometry2D::Line line(
                    Dia::Maths::Vector2D(ptc->x, ptc->y),
                    Dia::Maths::Vector2D(ctc->x, ctc->y));
                drawer.SubmitLine(line, Dia::Graphics::RGBA(100, 180, 255, 180));
            }
        }
    }

    for (int i = 0; i < 4; ++i)
        submitEntity(mQueryEntities[i], 4 + static_cast<unsigned int>(i));

    if (!mDoomedDestroyed)
        submitEntity(mDoomed, 8);

    drawer.Draw(frameData);
}

void EntityTestDrawer::DrawImGui()
{
    ImGui::SetNextWindowPos(ImVec2(10.f, 300.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260.f, 200.f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Entity Inspector");

    if (!mHasSelection)
    {
        ImGui::TextDisabled("No entity selected");
        ImGui::TextDisabled("Click a circle to inspect");
    }
    else if (mSelectedIdx < mEntityCount)
    {
        Dia::Entity::Entity e = mAllEntities[mSelectedIdx];
        if (!mDomain.IsAlive(e))
        {
            ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Entity destroyed");
        }
        else
        {
            const char* name = mDomain.GetDebugName(e);
            ImGui::Text("Name:  %s", name ? name : "(none)");
            ImGui::Text("Index: %u", mSelectedIdx);
            ImGui::Separator();

            auto* tc = mDomain.GetComponent<TransformComponent>(e);
            if (tc)
            {
                ImGui::Text("TransformComponent");
                ImGui::Text("  x = %.1f", tc->x);
                ImGui::Text("  y = %.1f", tc->y);
            }

            auto* vrc = mDomain.GetComponent<VisualTestRenderComponent>(e);
            if (vrc)
            {
                ImGui::Text("VisualTestRenderComponent");
                ImGui::Text("  radius = %.1f", vrc->radius);
                ImGui::Text("  colour = #%08X", vrc->colour);
            }

            auto* pc = mDomain.GetComponent<PickableCircleComponent>(e);
            if (pc)
            {
                ImGui::Text("PickableCircleComponent");
                ImGui::Text("  objectIdx  = %u", pc->objectIdx);
                ImGui::Text("  isSelected = %s", pc->isSelected ? "true" : "false");
            }
        }
    }

    ImGui::End();
}

} // namespace CluicheTest

#endif // DIA_DEBUG
