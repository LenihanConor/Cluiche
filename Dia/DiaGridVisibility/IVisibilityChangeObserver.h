#pragma once
#include <DiaGridVisibility/VisibilityGroupId.h>
#include <DiaGridVisibility/VisibilityState.h>
#include <DiaPathfinding/CellCoord.h>
#include <DiaEntity/Entity.h>

namespace Dia::GridVisibility {

    class IVisibilityChangeObserver {
    public:
        virtual ~IVisibilityChangeObserver() = default;

        virtual void OnCellStateChanged(Dia::Pathfinding::CellCoord cell,
                                        VisibilityGroupId groupId,
                                        VisibilityState oldState,
                                        VisibilityState newState) {}

        virtual void OnEntityVisibilityChanged(Dia::Entity::Entity entity,
                                               VisibilityGroupId groupId,
                                               bool isNowVisible) {}
    };

} // namespace Dia::GridVisibility
