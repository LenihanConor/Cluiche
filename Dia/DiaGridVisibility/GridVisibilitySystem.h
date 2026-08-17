#pragma once

#include <DiaGridVisibility/VisibilityGroupId.h>
#include <DiaGridVisibility/VisibilityState.h>
#include <DiaGridVisibility/VisibilityLogChannel.h>
#include <DiaGridVisibility/IVisibilityChangeObserver.h>

#include <DiaPathfinding/CPathGraph.h>
#include <DiaPathfinding/CellCoord.h>

#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>

#include <DiaMaths/Vector/Vector2D.h>

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Core/Assert.h>

#include <DiaObservation/Log/DiaLog.h>

#include <cmath>
#include <concepts>
#include <cstdint>
#include <span>

namespace Dia::GridVisibility {

    //------------------------------------------------------------------------------------
    // CVisibilityGraph
    //
    // CPathGraph only guarantees GetNeighbours + IsPassable. Chunk-grid consolidation and
    // recursive-octant shadowcasting both need the graph's extents, so the visibility system
    // constrains on a slightly stronger concept. SquarePathGrid and HexPathGrid both satisfy it.
    //------------------------------------------------------------------------------------
    template<typename T>
    concept CVisibilityGraph = Dia::Pathfinding::CPathGraph<T> && requires(const T& g) {
        { g.GetWidth()  } -> std::convertible_to<int>;
        { g.GetHeight() } -> std::convertible_to<int>;
    };

    //------------------------------------------------------------------------------------
    // GridVisibilitySystem
    //
    // Owns per-group, per-cell visibility state on a coarser grid derived from a pathfinding
    // graph. Sight sources are registered per group; Update() sweeps the dirty ones with
    // shadowcasting, merges into the group grid, decays uncovered cells Visible -> Revealed
    // and fires IVisibilityChangeObserver callbacks.
    //
    // Single-threaded. Register / Update / query all from SimPU.
    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    class GridVisibilitySystem
    {
    public:
        // chunkSize: number of pathfinding cells per visibility cell side (1 = full resolution).
        // A chunkSize of 4 reduces a 256x256 path grid to a 64x64 visibility grid.
        explicit GridVisibilitySystem(const TGraph& graph, int chunkSize = 1);

        GridVisibilitySystem(const GridVisibilitySystem&)            = delete;
        GridVisibilitySystem& operator=(const GridVisibilitySystem&) = delete;
        GridVisibilitySystem(GridVisibilitySystem&&)                 = delete;
        GridVisibilitySystem& operator=(GridVisibilitySystem&&)      = delete;

        // Register entity as a sight source contributing to groupId's merged visibility.
        // Re-registration updates sightRadius (and groupId). Must be called from SimPU.
        void RegisterSightSource(Dia::Entity::Entity entity,
                                 VisibilityGroupId groupId,
                                 float sightRadius);
        void UnregisterSightSource(Dia::Entity::Entity entity);

        // Subscribe / unsubscribe to cell and entity visibility change notifications.
        void AddChangeListener(IVisibilityChangeObserver* listener);
        void RemoveChangeListener(IVisibilityChangeObserver* listener);

        // Advance all group visibility grids one tick.
        // cellSize: world units per *pathfinding* cell (must match the graph provided).
        // spatial: the spatial index the sight sources and candidate targets are registered with.
        // domain: source of SpatialComponent world positions and of the alive-entity set.
        void Update(float cellSize,
                    const Dia::EntitySpatial::EntitySpatialModule& spatial,
                    const Dia::Entity::Domain& domain);

        // Can groupId currently see targetEntity? O(1) - precomputed during Update().
        bool CanSee(Dia::Entity::Entity targetEntity,
                    VisibilityGroupId groupId) const;

        // Raw per-cell visibility state for a group. O(1) lookup. Coordinates are
        // visibility-grid coordinates, not pathfinding-grid coordinates.
        VisibilityState GetCellState(Dia::Pathfinding::CellCoord cell,
                                     VisibilityGroupId groupId) const;

        // All entities whose current cell is Visible to groupId.
        // Span is valid until the next Update() call.
        std::span<const Dia::Entity::Entity> GetVisibleEntities(VisibilityGroupId groupId) const;

        int GetGroupCount() const { return static_cast<int>(mGroups.Size()); }

        // Consolidated visibility-grid geometry (derived from the graph at construction).
        int  GetVisWidth()  const { return mVisWidth; }
        int  GetVisHeight() const { return mVisHeight; }
        int  GetChunkSize() const { return mChunkSize; }

        // True if any pathfinding cell inside this visibility cell's chunk is passable.
        bool IsVisCellPassable(Dia::Pathfinding::CellCoord cell) const;

    private:
        //--------------------------------------------------------------------------------
        // Internal capacities
        //--------------------------------------------------------------------------------
        static constexpr int kMaxVisCells     = 4096;   // 64x64 consolidated visibility grid
        static constexpr int kMaxGroups       = 16;
        static constexpr int kMaxSightSources = 256;
        static constexpr int kMaxShadowCells  = 512;    // cells one source can illuminate
        static constexpr int kMaxListeners    = 16;

        static constexpr Dia::Pathfinding::CellCoord kInvalidVisCell{ -1, -1 };

        //--------------------------------------------------------------------------------
        // Internal state types
        //--------------------------------------------------------------------------------
        struct SightSourceEntry
        {
            Dia::Entity::Entity         entity{};
            VisibilityGroupId           groupId{};
            float                       sightRadius{ 0.0f };
            Dia::Pathfinding::CellCoord lastVisCell{ -1, -1 };   // last vis-grid coord (dirty detection)
            bool                        dirty{ true };

            // Vis-grid cells currently illuminated by this source.
            Dia::Core::Containers::DynamicArrayC<Dia::Pathfinding::CellCoord, kMaxShadowCells> shadowCells;
        };

        struct GroupState
        {
            VisibilityGroupId id{};

            // Parallel arrays indexed by vis-grid flat index (y * mVisWidth + x).
            // coverageCounts[i] = number of sight sources currently illuminating cell i.
            Dia::Core::Containers::DynamicArrayC<VisibilityState, kMaxVisCells> cellStates;
            Dia::Core::Containers::DynamicArrayC<uint8_t, kMaxVisCells>         coverageCounts;

            // Entities currently visible to this group (rebuilt each Update()).
            Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity,
                                                 Dia::Entity::kMaxEntitiesPerDomain> visibleEntities;
        };

        // One alive, spatially-tracked entity plus its visibility-grid coordinate.
        // Built once per Update() and shared by every group's visible-entity rebuild.
        struct EntityCellEntry
        {
            Dia::Entity::Entity         entity{};
            Dia::Pathfinding::CellCoord cell{ -1, -1 };
        };

        //--------------------------------------------------------------------------------
        // Internal helpers
        //--------------------------------------------------------------------------------
        int  VisCellCount() const { return mVisWidth * mVisHeight; }

        bool IsVisCellInBounds(Dia::Pathfinding::CellCoord cell) const
        {
            return cell.x >= 0 && cell.y >= 0 && cell.x < mVisWidth && cell.y < mVisHeight;
        }

        // Flat index into the per-cell arrays, or -1 if out of bounds.
        int VisCellIndex(Dia::Pathfinding::CellCoord cell) const
        {
            return IsVisCellInBounds(cell) ? (cell.y * mVisWidth + cell.x) : -1;
        }

        int FindGroupIndex(VisibilityGroupId groupId) const
        {
            for (unsigned int i = 0; i < mGroups.Size(); ++i)
            {
                if (mGroups[i].id == groupId)
                    return static_cast<int>(i);
            }
            return -1;
        }

        int FindSourceIndex(Dia::Entity::Entity entity) const
        {
            for (unsigned int i = 0; i < mSources.Size(); ++i)
            {
                if (mSources[i].entity == entity)
                    return static_cast<int>(i);
            }
            return -1;
        }

        // Returns the index of the GroupState for groupId, creating it if absent.
        // Returns -1 if the group table is full.
        int EnsureGroup(VisibilityGroupId groupId);

        // Removes this source's contribution from its group's coverage counts and clears
        // its shadow-cell list. Cell states are not touched here - the Visible -> Revealed
        // decay pass in Update() observes the dropped coverage on the next tick.
        void ReleaseSourceCoverage(SightSourceEntry& source);

        //--------------------------------------------------------------------------------
        // Shadowcasting (GVD-007: recursive octant shadowcasting, O(visible cells))
        //--------------------------------------------------------------------------------

        // World position -> visibility-grid coordinate. Uses floor so negative world
        // coordinates map monotonically (a plain int cast truncates toward zero and would
        // fold [-1,1) onto cell 0).
        static Dia::Pathfinding::CellCoord WorldToVisCell(const Dia::Maths::Vector2D& position,
                                                          float visCellSize);

        // Full 8-octant sweep from origin. Rebuilds source.shadowCells and adds this
        // source's contribution to its group's coverage counts / cell states.
        void ComputeShadow(SightSourceEntry& source,
                           Dia::Pathfinding::CellCoord origin,
                           float sightRadiusVisCells,
                           int groupIndex);

        void ScanOctant(Dia::Pathfinding::CellCoord origin,
                        int octant,
                        int row,
                        float startSlope,
                        float endSlope,
                        float sightRadiusVisCells,
                        SightSourceEntry& source,
                        int groupIndex);

        // Maps octant-local (row, col) to a visibility-grid coordinate.
        static Dia::Pathfinding::CellCoord TransformOctant(Dia::Pathfinding::CellCoord origin,
                                                           int row,
                                                           int col,
                                                           int octant);

        // Records cell as lit by source: appends to shadowCells (once per sweep),
        // bumps group coverage and promotes the cell to Visible.
        void MarkVisCell(Dia::Pathfinding::CellCoord cell,
                         SightSourceEntry& source,
                         int groupIndex);

        // Writes newState and fires OnCellStateChanged only on an actual transition (GVD-004).
        void SetCellState(GroupState& group,
                          int cellIndex,
                          VisibilityState newState,
                          VisibilityGroupId groupId);

        // Per-group visible-entity list rebuild + entity transition callbacks.
        void RebuildVisibleEntities(GroupState& group);

        //--------------------------------------------------------------------------------
        // State
        //--------------------------------------------------------------------------------
        const TGraph& mGraph;
        int           mChunkSize;
        int           mVisWidth;
        int           mVisHeight;

        // Per-vis-cell passability, indexed by flat vis-grid index.
        Dia::Core::Containers::DynamicArrayC<bool, kMaxVisCells> mPassable;

        Dia::Core::Containers::DynamicArrayC<GroupState, kMaxGroups>             mGroups;
        Dia::Core::Containers::DynamicArrayC<SightSourceEntry, kMaxSightSources> mSources;
        Dia::Core::Containers::DynamicArrayC<IVisibilityChangeObserver*, kMaxListeners> mListeners;

        //--------------------------------------------------------------------------------
        // Update() scratch. Members rather than locals: the entity buffers are 8-24 KB and
        // the default MSVC stack is 1 MB (C6262).
        //--------------------------------------------------------------------------------

        // Per-vis-cell "already marked by the sweep in progress" flag. Prevents the eight
        // octants double-counting the cells they share along the axes and diagonals, which
        // would otherwise burn through kMaxShadowCells. Reset at the end of every sweep.
        Dia::Core::Containers::DynamicArrayC<uint8_t, kMaxVisCells> mMarkScratch;

        Dia::Core::Containers::DynamicArrayC<EntityCellEntry,
                                             Dia::Entity::kMaxEntitiesPerDomain> mEntityCellScratch;
        Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity,
                                             Dia::Entity::kMaxEntitiesPerDomain> mPrevVisibleScratch;
    };

    //====================================================================================
    // Implementation
    //====================================================================================

    template<CVisibilityGraph TGraph>
    GridVisibilitySystem<TGraph>::GridVisibilitySystem(const TGraph& graph, int chunkSize)
        : mGraph(graph)
        , mChunkSize(chunkSize > 0 ? chunkSize : 1)
        , mVisWidth(0)
        , mVisHeight(0)
        , mPassable()
        , mGroups()
        , mSources()
        , mListeners()
        , mMarkScratch()
        , mEntityCellScratch()
        , mPrevVisibleScratch()
    {
        DIA_ASSERT(chunkSize > 0, "chunkSize must be >= 1");

        const int pathWidth  = graph.GetWidth();
        const int pathHeight = graph.GetHeight();

        // Ceiling division - a partial chunk at the far edge still yields a visibility cell.
        mVisWidth  = (pathWidth  + mChunkSize - 1) / mChunkSize;
        mVisHeight = (pathHeight + mChunkSize - 1) / mChunkSize;

        if (mVisWidth < 0)  { mVisWidth = 0; }
        if (mVisHeight < 0) { mVisHeight = 0; }

        DIA_ASSERT(mVisWidth * mVisHeight <= kMaxVisCells,
                   "Consolidated visibility grid exceeds kMaxVisCells - increase chunkSize");

        // Consolidation pass: a visibility cell is passable if ANY pathfinding cell in its
        // chunk is passable (GVD-009).
        for (int visY = 0; visY < mVisHeight; ++visY)
        {
            for (int visX = 0; visX < mVisWidth; ++visX)
            {
                bool anyPassable = false;

                for (int dy = 0; dy < mChunkSize && !anyPassable; ++dy)
                {
                    for (int dx = 0; dx < mChunkSize && !anyPassable; ++dx)
                    {
                        const Dia::Pathfinding::CellCoord pathCell{
                            visX * mChunkSize + dx,
                            visY * mChunkSize + dy
                        };

                        if (pathCell.x < pathWidth && pathCell.y < pathHeight &&
                            graph.IsPassable(pathCell))
                        {
                            anyPassable = true;
                        }
                    }
                }

                if (!mPassable.IsFull())
                {
                    mPassable.Add(anyPassable);
                    mMarkScratch.Add(0u);
                }
            }
        }

        DIA_LOG_INFO(kLogChannel,
                     "GridVisibilitySystem constructed: %dx%d vis grid from %dx%d path grid (chunkSize=%d)",
                     mVisWidth, mVisHeight, pathWidth, pathHeight, mChunkSize);
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    bool GridVisibilitySystem<TGraph>::IsVisCellPassable(Dia::Pathfinding::CellCoord cell) const
    {
        const int index = VisCellIndex(cell);
        return index >= 0 && mPassable[static_cast<unsigned int>(index)];
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    int GridVisibilitySystem<TGraph>::EnsureGroup(VisibilityGroupId groupId)
    {
        const int existing = FindGroupIndex(groupId);
        if (existing >= 0)
            return existing;

        if (mGroups.IsFull())
        {
            DIA_ASSERT(false, "GridVisibilitySystem group table is full");
            DIA_LOG_ERROR(kLogChannel,
                          "RegisterSightSource: group table full (%d groups) - group '%s' ignored",
                          static_cast<int>(mGroups.Size()), groupId.AsChar());
            return -1;
        }

        mGroups.AddDefault();

        GroupState& group = mGroups.Back();
        group.id = groupId;
        group.cellStates.RemoveAll();
        group.coverageCounts.RemoveAll();
        group.visibleEntities.RemoveAll();

        const int cellCount = VisCellCount();
        for (int i = 0; i < cellCount; ++i)
        {
            group.cellStates.Add(VisibilityState::Unexplored);
            group.coverageCounts.Add(0);
        }

        return static_cast<int>(mGroups.Size()) - 1;
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    void GridVisibilitySystem<TGraph>::ReleaseSourceCoverage(SightSourceEntry& source)
    {
        const int groupIndex = FindGroupIndex(source.groupId);
        if (groupIndex >= 0)
        {
            GroupState& group = mGroups[static_cast<unsigned int>(groupIndex)];

            for (unsigned int i = 0; i < source.shadowCells.Size(); ++i)
            {
                const int cellIndex = VisCellIndex(source.shadowCells[i]);
                if (cellIndex >= 0)
                {
                    uint8_t& coverage = group.coverageCounts[static_cast<unsigned int>(cellIndex)];
                    if (coverage > 0)
                        --coverage;
                }
            }
        }

        source.shadowCells.RemoveAll();
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    void GridVisibilitySystem<TGraph>::RegisterSightSource(Dia::Entity::Entity entity,
                                                           VisibilityGroupId groupId,
                                                           float sightRadius)
    {
        DIA_ASSERT(entity.IsValid(), "RegisterSightSource called with an invalid entity handle");
        if (!entity.IsValid())
            return;

        const int existing = FindSourceIndex(entity);
        if (existing >= 0)
        {
            SightSourceEntry& source = mSources[static_cast<unsigned int>(existing)];

            if (!(source.groupId == groupId))
            {
                // Group reassignment: drop the old group's coverage, then start fresh.
                ReleaseSourceCoverage(source);

                if (EnsureGroup(groupId) < 0)
                    return;

                source.groupId     = groupId;
                source.lastVisCell = kInvalidVisCell;
                source.dirty       = true;
            }

            if (source.sightRadius != sightRadius)
            {
                source.sightRadius = sightRadius;
                source.dirty       = true;
            }

            return;
        }

        if (mSources.IsFull())
        {
            DIA_ASSERT(false, "GridVisibilitySystem sight-source table is full");
            DIA_LOG_ERROR(kLogChannel,
                          "RegisterSightSource: source table full (%d sources) - entity %u ignored",
                          static_cast<int>(mSources.Size()), entity.GetIndex());
            return;
        }

        if (EnsureGroup(groupId) < 0)
            return;

        mSources.AddDefault();

        SightSourceEntry& source = mSources.Back();
        source.entity      = entity;
        source.groupId     = groupId;
        source.sightRadius = sightRadius;
        source.lastVisCell = kInvalidVisCell;
        source.dirty       = true;
        source.shadowCells.RemoveAll();
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    void GridVisibilitySystem<TGraph>::UnregisterSightSource(Dia::Entity::Entity entity)
    {
        const int existing = FindSourceIndex(entity);
        if (existing < 0)
            return;

        ReleaseSourceCoverage(mSources[static_cast<unsigned int>(existing)]);
        mSources.RemoveAt(static_cast<unsigned int>(existing));
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    void GridVisibilitySystem<TGraph>::AddChangeListener(IVisibilityChangeObserver* listener)
    {
        if (listener == nullptr)
            return;

        if (mListeners.FindIndex(listener) >= 0)
            return;

        if (mListeners.IsFull())
        {
            DIA_ASSERT(false, "GridVisibilitySystem listener table is full");
            return;
        }

        mListeners.Add(listener);
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    void GridVisibilitySystem<TGraph>::RemoveChangeListener(IVisibilityChangeObserver* listener)
    {
        if (listener == nullptr)
            return;

        const int index = mListeners.FindIndex(listener);
        if (index >= 0)
            mListeners.RemoveAt(static_cast<unsigned int>(index));
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    Dia::Pathfinding::CellCoord
        GridVisibilitySystem<TGraph>::WorldToVisCell(const Dia::Maths::Vector2D& position,
                                                     float visCellSize)
    {
        return Dia::Pathfinding::CellCoord{
            static_cast<int>(std::floor(position.x / visCellSize)),
            static_cast<int>(std::floor(position.y / visCellSize))
        };
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    Dia::Pathfinding::CellCoord
        GridVisibilitySystem<TGraph>::TransformOctant(Dia::Pathfinding::CellCoord origin,
                                                      int row,
                                                      int col,
                                                      int octant)
    {
        switch (octant)
        {
        case 0:  return Dia::Pathfinding::CellCoord{ origin.x + col, origin.y - row };
        case 1:  return Dia::Pathfinding::CellCoord{ origin.x + row, origin.y - col };
        case 2:  return Dia::Pathfinding::CellCoord{ origin.x + row, origin.y + col };
        case 3:  return Dia::Pathfinding::CellCoord{ origin.x + col, origin.y + row };
        case 4:  return Dia::Pathfinding::CellCoord{ origin.x - col, origin.y + row };
        case 5:  return Dia::Pathfinding::CellCoord{ origin.x - row, origin.y + col };
        case 6:  return Dia::Pathfinding::CellCoord{ origin.x - row, origin.y - col };
        case 7:  return Dia::Pathfinding::CellCoord{ origin.x - col, origin.y - row };
        default: return origin;
        }
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    void GridVisibilitySystem<TGraph>::SetCellState(GroupState& group,
                                                    int cellIndex,
                                                    VisibilityState newState,
                                                    VisibilityGroupId groupId)
    {
        if (cellIndex < 0 || mVisWidth <= 0)
            return;

        const unsigned int flat     = static_cast<unsigned int>(cellIndex);
        const VisibilityState oldState = group.cellStates[flat];
        if (oldState == newState)
            return;

        group.cellStates[flat] = newState;

        if (mListeners.IsEmpty())
            return;

        const Dia::Pathfinding::CellCoord cell{ cellIndex % mVisWidth, cellIndex / mVisWidth };
        for (unsigned int i = 0; i < mListeners.Size(); ++i)
            mListeners[i]->OnCellStateChanged(cell, groupId, oldState, newState);
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    void GridVisibilitySystem<TGraph>::MarkVisCell(Dia::Pathfinding::CellCoord cell,
                                                   SightSourceEntry& source,
                                                   int groupIndex)
    {
        const int cellIndex = VisCellIndex(cell);
        if (cellIndex < 0)
            return;

        const unsigned int flat = static_cast<unsigned int>(cellIndex);

        // Octants overlap along the axes and diagonals - only record each cell once.
        if (mMarkScratch[flat] != 0u)
            return;

        if (source.shadowCells.IsFull())
            return;

        mMarkScratch[flat] = 1u;
        source.shadowCells.Add(cell);

        if (groupIndex < 0)
            return;

        GroupState& group = mGroups[static_cast<unsigned int>(groupIndex)];

        uint8_t& coverage = group.coverageCounts[flat];
        if (coverage < 255u)
            ++coverage;

        SetCellState(group, cellIndex, VisibilityState::Visible, group.id);
    }

    //------------------------------------------------------------------------------------
    // Recursive octant shadowcasting. Walks outward one row at a time; each contiguous run
    // of blocking cells narrows the slope range handed to the rows beyond it.
    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    void GridVisibilitySystem<TGraph>::ScanOctant(Dia::Pathfinding::CellCoord origin,
                                                  int octant,
                                                  int row,
                                                  float startSlope,
                                                  float endSlope,
                                                  float maxRadius,
                                                  SightSourceEntry& source,
                                                  int groupIndex)
    {
        const int maxRow = static_cast<int>(maxRadius);
        if (row > maxRow)
            return;
        if (startSlope < endSlope)
            return;
        if (source.shadowCells.IsFull())
            return;

        bool  prevBlocked     = false;
        float savedStartSlope = startSlope;

        const float rowF   = static_cast<float>(row);
        const int   maxCol = static_cast<int>(std::ceil(rowF * startSlope));
        const int   minCol = static_cast<int>(std::floor(rowF * endSlope));

        for (int col = maxCol; col >= minCol; --col)
        {
            const Dia::Pathfinding::CellCoord cell = TransformOctant(origin, row, col, octant);

            const int cellIndex = VisCellIndex(cell);
            if (cellIndex < 0)
                continue;

            // Circular sight radius - clip the corners of the square scan region.
            const float dx = static_cast<float>(cell.x - origin.x);
            const float dy = static_cast<float>(cell.y - origin.y);
            if (dx * dx + dy * dy > maxRadius * maxRadius)
                continue;

            const bool blocked = !mPassable[static_cast<unsigned int>(cellIndex)];

            // Blockers are themselves visible - only what is behind them is not.
            MarkVisCell(cell, source, groupIndex);

            if (blocked)
            {
                if (!prevBlocked)
                {
                    // Start of a blocking run: recurse into the still-lit wedge above it.
                    const float leftSlope = (static_cast<float>(col) + 0.5f) / rowF;
                    ScanOctant(origin, octant, row + 1, savedStartSlope, leftSlope,
                               maxRadius, source, groupIndex);
                }

                prevBlocked     = true;
                savedStartSlope = (static_cast<float>(col) - 0.5f) / rowF;
            }
            else
            {
                if (prevBlocked)
                {
                    // End of a blocking run: resume from just past the blocker's edge.
                    savedStartSlope = (static_cast<float>(col) + 0.5f) / rowF;
                }

                prevBlocked = false;
            }
        }

        // The run reaching the end of the row is fully blocked - nothing beyond it is lit.
        if (!prevBlocked)
        {
            ScanOctant(origin, octant, row + 1, savedStartSlope, endSlope,
                       maxRadius, source, groupIndex);
        }
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    void GridVisibilitySystem<TGraph>::ComputeShadow(SightSourceEntry& source,
                                                      Dia::Pathfinding::CellCoord origin,
                                                      float sightRadiusVisCells,
                                                      int groupIndex)
    {
        source.shadowCells.RemoveAll();

        // The source always sees the cell it stands in, even if that cell is impassable.
        MarkVisCell(origin, source, groupIndex);

        for (int octant = 0; octant < 8; ++octant)
        {
            ScanOctant(origin, octant, 1, 1.0f, 0.0f,
                       sightRadiusVisCells, source, groupIndex);
        }

        // Reset only the stamps this sweep set - cheaper than clearing the whole grid.
        for (unsigned int i = 0; i < source.shadowCells.Size(); ++i)
        {
            const int cellIndex = VisCellIndex(source.shadowCells[i]);
            if (cellIndex >= 0)
                mMarkScratch[static_cast<unsigned int>(cellIndex)] = 0u;
        }
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    void GridVisibilitySystem<TGraph>::RebuildVisibleEntities(GroupState& group)
    {
        // Snapshot the previous list so entity transitions can be diffed after the rebuild.
        mPrevVisibleScratch.RemoveAll();
        for (unsigned int i = 0; i < group.visibleEntities.Size(); ++i)
            mPrevVisibleScratch.Add(group.visibleEntities[i]);

        group.visibleEntities.RemoveAll();

        for (unsigned int i = 0; i < mEntityCellScratch.Size(); ++i)
        {
            const int cellIndex = VisCellIndex(mEntityCellScratch[i].cell);
            if (cellIndex < 0)
                continue;

            if (group.cellStates[static_cast<unsigned int>(cellIndex)] != VisibilityState::Visible)
                continue;

            if (group.visibleEntities.IsFull())
                break;

            group.visibleEntities.Add(mEntityCellScratch[i].entity);
        }

        if (mListeners.IsEmpty())
            return;

        // GVD-004: fire synchronously, and only on a transition.
        for (unsigned int i = 0; i < group.visibleEntities.Size(); ++i)
        {
            const Dia::Entity::Entity entity = group.visibleEntities[i];
            if (mPrevVisibleScratch.FindIndex(entity) >= 0)
                continue;

            for (unsigned int l = 0; l < mListeners.Size(); ++l)
                mListeners[l]->OnEntityVisibilityChanged(entity, group.id, true);
        }

        for (unsigned int i = 0; i < mPrevVisibleScratch.Size(); ++i)
        {
            const Dia::Entity::Entity entity = mPrevVisibleScratch[i];
            if (group.visibleEntities.FindIndex(entity) >= 0)
                continue;

            for (unsigned int l = 0; l < mListeners.Size(); ++l)
                mListeners[l]->OnEntityVisibilityChanged(entity, group.id, false);
        }
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    void GridVisibilitySystem<TGraph>::Update(float cellSize,
                                              const Dia::EntitySpatial::EntitySpatialModule& spatial,
                                              const Dia::Entity::Domain& domain)
    {
        // Sight-source positions and target positions both come from SpatialComponent; the
        // spatial index itself is only a participation contract here (sources and targets are
        // expected to be registered with it). Kept in the signature per the system spec.
        (void)spatial;

        DIA_ASSERT(cellSize > 0.0f, "GridVisibilitySystem::Update requires a positive cellSize");
        if (!(cellSize > 0.0f) || VisCellCount() == 0)
            return;

        // A visibility cell spans chunkSize pathfinding cells on each side.
        const float visCellSize = cellSize * static_cast<float>(mChunkSize);

        //--------------------------------------------------------------------------------
        // 1 + 2. Dirty detection, then a shadowcasting sweep for the dirty sources only
        //        (GVD-008: standing still costs nothing).
        //--------------------------------------------------------------------------------
        int dirtyCount = 0;

        for (unsigned int i = 0; i < mSources.Size(); ++i)
        {
            SightSourceEntry& source = mSources[i];

            const Dia::EntitySpatial::SpatialComponent* spatialComponent =
                domain.GetComponent<Dia::EntitySpatial::SpatialComponent>(source.entity);

            if (spatialComponent == nullptr)
                continue;   // entity died or lost its SpatialComponent - keep its last shadow

            const Dia::Pathfinding::CellCoord currentVisCell =
                WorldToVisCell(spatialComponent->position, visCellSize);

            if (!(currentVisCell == source.lastVisCell))
                source.dirty = true;

            if (!source.dirty)
                continue;

            const int groupIndex = FindGroupIndex(source.groupId);

            // ScanOctant recurses once per row, so clamp the radius to something the grid can
            // actually contain - a runaway sightRadius would otherwise blow the stack.
            float radiusVisCells = source.sightRadius / visCellSize;
            const float maxUsefulRadius = static_cast<float>(mVisWidth + mVisHeight);
            if (radiusVisCells > maxUsefulRadius) { radiusVisCells = maxUsefulRadius; }
            if (!(radiusVisCells > 0.0f))         { radiusVisCells = 0.0f; }

            ReleaseSourceCoverage(source);
            ComputeShadow(source, currentVisCell, radiusVisCells, groupIndex);

            source.lastVisCell = currentVisCell;
            source.dirty       = false;
            ++dirtyCount;
        }

        //--------------------------------------------------------------------------------
        // 3. Visible -> Revealed decay for cells no source covers any more.
        //--------------------------------------------------------------------------------
        const int cellCount = VisCellCount();

        for (unsigned int g = 0; g < mGroups.Size(); ++g)
        {
            GroupState& group = mGroups[g];

            for (int c = 0; c < cellCount; ++c)
            {
                const unsigned int flat = static_cast<unsigned int>(c);

                if (group.cellStates[flat] == VisibilityState::Visible &&
                    group.coverageCounts[flat] == 0u)
                {
                    SetCellState(group, c, VisibilityState::Revealed, group.id);
                }
            }
        }

        //--------------------------------------------------------------------------------
        // 4. Rebuild the per-group visible-entity lists.
        //--------------------------------------------------------------------------------
        mEntityCellScratch.RemoveAll();

        for (uint32_t e = 0; e < Dia::Entity::kMaxEntitiesPerDomain; ++e)
        {
            if (mEntityCellScratch.IsFull())
                break;

            const Dia::Entity::Entity entity = domain.GetAliveEntity(e);
            if (!entity.IsValid())
                continue;

            const Dia::EntitySpatial::SpatialComponent* spatialComponent =
                domain.GetComponent<Dia::EntitySpatial::SpatialComponent>(entity);

            if (spatialComponent == nullptr)
                continue;

            EntityCellEntry entry;
            entry.entity = entity;
            entry.cell   = WorldToVisCell(spatialComponent->position, visCellSize);
            mEntityCellScratch.Add(entry);
        }

        for (unsigned int g = 0; g < mGroups.Size(); ++g)
            RebuildVisibleEntities(mGroups[g]);

        DIA_LOG_INFO(kLogChannel,
                     "Update: %d groups, %d sources (%d dirty), %d spatial entities",
                     static_cast<int>(mGroups.Size()),
                     static_cast<int>(mSources.Size()),
                     dirtyCount,
                     static_cast<int>(mEntityCellScratch.Size()));
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    bool GridVisibilitySystem<TGraph>::CanSee(Dia::Entity::Entity targetEntity,
                                              VisibilityGroupId groupId) const
    {
        const int groupIndex = FindGroupIndex(groupId);
        if (groupIndex < 0)
            return false;
        const GroupState& group = mGroups[static_cast<unsigned int>(groupIndex)];
        for (unsigned int i = 0; i < group.visibleEntities.Size(); ++i)
        {
            if (group.visibleEntities[i] == targetEntity)
                return true;
        }
        return false;
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    VisibilityState GridVisibilitySystem<TGraph>::GetCellState(Dia::Pathfinding::CellCoord cell,
                                                               VisibilityGroupId groupId) const
    {
        const int groupIndex = FindGroupIndex(groupId);
        if (groupIndex < 0)
            return VisibilityState::Unexplored;
        const int cellIndex = VisCellIndex(cell);
        if (cellIndex < 0)
            return VisibilityState::Unexplored;
        return mGroups[static_cast<unsigned int>(groupIndex)]
                       .cellStates[static_cast<unsigned int>(cellIndex)];
    }

    //------------------------------------------------------------------------------------
    template<CVisibilityGraph TGraph>
    std::span<const Dia::Entity::Entity>
        GridVisibilitySystem<TGraph>::GetVisibleEntities(VisibilityGroupId groupId) const
    {
        const int groupIndex = FindGroupIndex(groupId);
        if (groupIndex < 0)
            return {};
        const GroupState& group = mGroups[static_cast<unsigned int>(groupIndex)];
        if (group.visibleEntities.IsEmpty())
            return {};
        return std::span<const Dia::Entity::Entity>(
            &group.visibleEntities[0u],
            group.visibleEntities.Size());
    }

} // namespace Dia::GridVisibility
