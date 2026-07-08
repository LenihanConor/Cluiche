#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Entity { class IEntityInspectable; class Domain; }
namespace Dia::Core  { class IDebugContext; }

namespace Dia::EntityVisualDebugger
{

class EntityLabelsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    EntityLabelsDrawer(
        Dia::Entity::IEntityInspectable& inspectable,
        Dia::Entity::Domain& domain,
        const Dia::Core::IDebugContext& manager,
        Dia::Core::StringCRC positionComponentTypeId);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

    void SetFilter(const char* pattern);

private:
    Dia::Entity::IEntityInspectable& mInspectable;
    Dia::Entity::Domain& mDomain;
    const Dia::Core::IDebugContext& mManager;
    Dia::Core::StringCRC mPositionTypeId;
    char mFilterPattern[64] = {'*', '\0'};
    float mFontSize = 12.0f;
};

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
