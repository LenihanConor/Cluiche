#pragma once
#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>

#ifdef DIA_DEBUG
#include <DiaGeometry2DPicking/Adapters/ShapePickable.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <memory>
namespace Dia::Geometry2DPicking { class PickingService2D; }
#endif

namespace CluicheTest {

// Behaviour component: owns a ShapePickable for this entity's circle.
// OnAttach reads TransformComponent + VisualTestRenderComponent, builds the
// circle geometry, creates the ShapePickable, and registers with the injected
// PickingService2D.  OnDetach unregisters and destroys.
//
// objectIdx is set by the module before QueueAddComponent so each entity gets
// a unique index carried back in the pick event.
//
// In Release builds the component is a no-op (FIELD only, no picking system).
class PickableCircleComponent : public Dia::Entity::IComponent
{
    DIA_COMPONENT(PickableCircleComponent, "cluichetest.pickable-circle", 1)

    FIELD(unsigned int, objectIdx, 0u)

public:
    void OnAttach(Dia::Entity::Domain& domain, Dia::Entity::Entity self) override;
    void OnDetach(Dia::Entity::Domain& domain, Dia::Entity::Entity self) override;

#ifdef DIA_DEBUG
    static Dia::Geometry2DPicking::PickingService2D* sPickingService;

private:
    // Owned geometry — stable pointer held by mPickable.
    // Declared here so HandlePool<PickableCircleComponent> destructs them properly.
    Dia::Geometry2D::Circle                                mCircle;
    std::unique_ptr<Dia::Geometry2DPicking::ShapePickable> mPickable;
#endif
};

} // namespace CluicheTest
