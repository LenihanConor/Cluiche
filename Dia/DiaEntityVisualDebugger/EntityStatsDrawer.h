#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Entity { class IEntityInspectable; }

namespace Dia::EntityVisualDebugger
{

class EntityStatsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit EntityStatsDrawer(Dia::Entity::IEntityInspectable& inspectable);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    Dia::Entity::IEntityInspectable& mInspectable;
};

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
