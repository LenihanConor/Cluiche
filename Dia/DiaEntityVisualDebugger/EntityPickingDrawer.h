#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Entity { class IEntityInspectable; class Domain; }
namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::EntityVisualDebugger
{

class EntityPickingDrawer : public Dia::Debug::IVisualDebugger
{
public:
    EntityPickingDrawer(
        Dia::Entity::IEntityInspectable& inspectable,
        Dia::Entity::Domain& domain,
        const Dia::Debug::DebugLayerManager& manager,
        Dia::Core::StringCRC positionComponentTypeId);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;
    void DrawImGui() override;

private:
    Dia::Entity::IEntityInspectable& mInspectable;
    Dia::Entity::Domain& mDomain;
    const Dia::Debug::DebugLayerManager& mManager;
    Dia::Core::StringCRC mPositionTypeId;
    float mHighlightRadius = 12.0f;
};

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
