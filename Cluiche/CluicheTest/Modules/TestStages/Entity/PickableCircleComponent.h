#pragma once
#include <diaentitytemplate/IComponent.h>
#include <diaentitytemplate/ComponentMacros.h>

#ifdef DIA_DEBUG
#include <DiaGeometry2DPicking/Adapters/ShapePickable.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <memory>
#endif

namespace CluicheTest {

// Behaviour component: makes this entity pickable as a circle.
// OnAttach reads TransformComponent + VisualTestRenderComponent to build circle
// geometry, then registers with PickingService2D via domain.GetService<T>().
// OnDetach unregisters. Carries selection state written by the module after
// draining pick events.
class PickableCircleComponent : public Dia::Entity::IComponent
{
    DIA_COMPONENT(PickableCircleComponent, "cluichetest.pickable-circle", 1)

    FIELD(unsigned int, objectIdx, 0u)
    FIELD(bool, isSelected, false)

public:
    void OnAttach(Dia::Entity::Domain& domain, Dia::Entity::Entity self) override;
    void OnDetach(Dia::Entity::Domain& domain, Dia::Entity::Entity self) override;

#ifdef DIA_DEBUG
private:
    Dia::Geometry2D::Circle                                mCircle;
    std::unique_ptr<Dia::Geometry2DPicking::ShapePickable> mPickable;
#endif
};

} // namespace CluicheTest
