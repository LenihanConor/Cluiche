#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Entity { class IEntityInspectable; class Domain; }
namespace Dia::Core  { class IDebugContext; }

namespace Dia::EntityVisualDebugger
{

class ComponentFilterHighlightDrawer : public Dia::Debug::IVisualDebugger
{
public:
    ComponentFilterHighlightDrawer(
        Dia::Entity::IEntityInspectable& inspectable,
        Dia::Entity::Domain& domain,
        const Dia::Core::IDebugContext& manager,
        Dia::Core::StringCRC positionComponentTypeId);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

    void SetFilterTypeId(Dia::Core::StringCRC typeId);

private:
    Dia::Entity::IEntityInspectable& mInspectable;
    Dia::Entity::Domain& mDomain;
    const Dia::Core::IDebugContext& mManager;
    Dia::Core::StringCRC mPositionTypeId;
    Dia::Core::StringCRC mFilterTypeId;
    float mHighlightRadius = 8.0f;
    int mSelectedIndex = -1;
};

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
