#include "DiaPathfinding/PathfindingSystem.h"
#include "DiaPathfinding/FindPath.h"
#include <DiaObservation/Log/DiaLog.h>
#include <chrono>
#include <algorithm>

namespace Dia::Pathfinding {

    template<CPathGraph TGraph>
    PathfindingSystem<TGraph>::PathfindingSystem(const TGraph& graph)
        : mGraph(graph)
    {
    }

    template<CPathGraph TGraph>
    PathRequestId PathfindingSystem<TGraph>::RequestPath(const PathRequest& request)
    {
        mQueue.push_back(request);
        DIA_LOG_INFO("Pathfinding", "PathRequest submitted: %s", request.id.AsChar());
        return request.id;
    }

    template<CPathGraph TGraph>
    void PathfindingSystem<TGraph>::CancelRequest(PathRequestId id)
    {
        mQueue.erase(
            std::remove_if(mQueue.begin(), mQueue.end(),
                [id](const PathRequest& r) { return r.id == id; }),
            mQueue.end());
    }

    template<CPathGraph TGraph>
    void PathfindingSystem<TGraph>::Update(float budgetMs)
    {
        using Clock     = std::chrono::steady_clock;
        using Duration  = std::chrono::duration<float, std::milli>;

        const auto startTime = Clock::now();

        while (!mQueue.empty())
        {
            const Duration elapsed = Clock::now() - startTime;
            if (elapsed.count() >= budgetMs)
            {
                break;
            }

            PathRequest req = mQueue.front();
            mQueue.erase(mQueue.begin());

            PathResult result = FindPath(mGraph, req.from, req.to, *req.costs);

            if (result.success)
            {
                if (req.observer != nullptr)
                {
                    req.observer->OnPathFound(req.id, result);
                }
                DIA_LOG_INFO("Pathfinding", "PathRequest completed: %s", req.id.AsChar());
            }
            else
            {
                if (req.observer != nullptr)
                {
                    req.observer->OnPathFailed(req.id);
                }
                DIA_LOG_INFO("Pathfinding", "PathRequest failed: %s", req.id.AsChar());
            }
        }
    }

    template<CPathGraph TGraph>
    int PathfindingSystem<TGraph>::GetPendingCount() const
    {
        return static_cast<int>(mQueue.size());
    }

} // namespace Dia::Pathfinding
