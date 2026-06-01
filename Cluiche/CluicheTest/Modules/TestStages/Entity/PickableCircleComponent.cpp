#include "Modules/TestStages/Entity/PickableCircleComponent.h"
#include "Modules/TestStages/Entity/TransformComponent.h"
#include "Modules/TestStages/Entity/VisualTestRenderComponent.h"
#include <DiaCore/Reflect/ReflectMacros.h>
#include <DiaEntity/Domain.h>

#ifdef DIA_DEBUG
#include <DiaGeometry2DPicking/PickingService2D.h>
#include <DiaPicking/PickLayer.h>
#endif

DIA_SERIALIZE(CluicheTest::PickableCircleComponent, CluicheTest::PickableCircleComponent::kVersion)
    DIA_FIELD(objectIdx)
DIA_SERIALIZE_END

namespace CluicheTest {

#ifdef DIA_DEBUG
Dia::Geometry2DPicking::PickingService2D* PickableCircleComponent::sPickingService = nullptr;
#endif

void PickableCircleComponent::OnAttach(Dia::Entity::Domain& domain, Dia::Entity::Entity self)
{
#ifdef DIA_DEBUG
    if (!sPickingService) return;

    auto* tc  = domain.GetComponent<TransformComponent>(self);
    auto* vrc = domain.GetComponent<VisualTestRenderComponent>(self);
    if (!tc || !vrc) return;

    mCircle  = Dia::Geometry2D::Circle(vrc->radius, Dia::Maths::Vector2D(tc->x, tc->y));
    mPickable = std::make_unique<Dia::Geometry2DPicking::ShapePickable>(
        &mCircle,
        Dia::Core::StringCRC("entity.circle"),
        objectIdx,
        10,
        Dia::Picking::PickLayer::kDefault);

    sPickingService->Register(mPickable.get());
#else
    (void)domain; (void)self;
#endif
}

void PickableCircleComponent::OnDetach(Dia::Entity::Domain& /*domain*/, Dia::Entity::Entity /*self*/)
{
#ifdef DIA_DEBUG
    if (mPickable && sPickingService)
        sPickingService->Unregister(mPickable.get());
    mPickable.reset();
#endif
}

static Dia::Entity::FieldDesc s_PickableCircleComponent_fields[] = {
    DIA_FIELD_ENTRY(unsigned int, objectIdx, PickableCircleComponent)
};

DIA_COMPONENT_REGISTER(PickableCircleComponent, "cluichetest.pickable-circle", false,
    s_PickableCircleComponent_fields, DIA_ARRAY_COUNT(s_PickableCircleComponent_fields),
    nullptr, 0)

} // namespace CluicheTest
