#pragma once
#include "DiaPathfinding/CPathGraph.h"
#include "DiaPathfinding/IPathCostProvider.h"
#include "DiaPathfinding/IPathResultObserver.h"
#include "DiaPathfinding/PathResult.h"
#include "DiaPathfinding/FindPath.h"
#include <DiaCore/CRC/StringCRC.h>
#include <vector>   // internal only — NOT in public API

namespace Dia::Pathfinding {

    struct PathRequest {
        PathRequestId        id;
        CellCoord            from;
        CellCoord            to;
        IPathCostProvider*   costs;    // non-owning; must outlive the request
        IPathResultObserver* observer; // non-owning
    };

    template<CPathGraph TGraph>
    class PathfindingSystem {
    public:
        explicit PathfindingSystem(const TGraph& graph);

        PathRequestId RequestPath(const PathRequest& request);
        void          CancelRequest(PathRequestId id);
        void          Update(float budgetMs);
        int           GetPendingCount() const;

    private:
        const TGraph&            mGraph;
        std::vector<PathRequest> mQueue;  // internal STL queue is fine
    };

} // namespace Dia::Pathfinding

#include "DiaPathfinding/PathfindingSystem.inl"
