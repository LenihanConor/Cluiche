#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace CluicheTest {

class EntityTestDrawer : public Dia::Debug::IVisualDebugger
{
public:
    EntityTestDrawer(
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
        const Dia::Debug::DebugLayerManager& mgr);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;
    void DrawImGui() override;

private:
    Dia::Entity::Domain&              mDomain;
    Dia::Entity::Entity               mParent;
    Dia::Entity::Entity               mChildA;
    Dia::Entity::Entity               mChildB;
    Dia::Entity::Entity               mChildC;
    Dia::Entity::Entity               mQueryEntities[4];
    Dia::Entity::Entity               mDoomed;
    const bool&                       mDoomedDestroyed;
    const bool&                       mHasSelection;
    const unsigned int&               mSelectedIdx;
    const Dia::Entity::Entity*        mAllEntities;
    unsigned int                      mEntityCount;
    const Dia::Debug::DebugLayerManager& mManager;
    Dia::Geometry2DVisualDebugger::ShapeDrawer mDrawer;
};

} // namespace CluicheTest

#endif // DIA_DEBUG
