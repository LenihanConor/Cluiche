#pragma once
#include "DiaPathfinding/CellCoord.h"
#include "DiaPathfinding/PathResult.h"
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Pathfinding {

    using PathRequestId = Dia::Core::StringCRC;

    class IPathResultObserver {
    public:
        virtual ~IPathResultObserver() = default;
        virtual void OnPathFound(PathRequestId id, const PathResult& result) = 0;
        virtual void OnPathFailed(PathRequestId id) = 0;
    };

} // namespace Dia::Pathfinding
