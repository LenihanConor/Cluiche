////////////////////////////////////////////////////////////////////////////////
// Filename: GridVisibilityDebugDomain.h
// Description: IDebugDomain implementation for the GridVisibility system.
//              Owns three world-space drawers: CellStateDrawer, SightRadiiDrawer,
//              and ShadowcastBoundaryDrawer. Full implementations are provided in
//              Tasks 4–6; all Draw() methods are stubbed here.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaGridVisibility/GridVisibilitySystem.h>
#include <DiaGridVisibility/VisibilityGroupId.h>
#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
    namespace GridVisibilityVisualDebugger
    {
        ////////////////////////////////////////////////////////////////////////////////
        // GridVisibilityDebugDomain
        //
        // Visual debugger domain for the GridVisibility system. Contributes three
        // world-space drawers that can be toggled independently from the debug panel:
        //   - CellStateDrawer         (GV.CellState)   — per-cell visibility overlay
        //   - SightRadiiDrawer        (GV.SightRadii)  — sight source radius circles
        //   - ShadowcastBoundaryDrawer(GV.Boundary)    — shadowcast octant outlines
        //
        // Templated on CVisibilityGraph so that the same domain works with any
        // grid type that satisfies the GridVisibility constraint.
        ////////////////////////////////////////////////////////////////////////////////
        template<Dia::GridVisibility::CVisibilityGraph TGraph>
        class GridVisibilityDebugDomain : public Dia::VisualDebugger::IDebugDomain
        {
        public:

            // ----------------------------------------------------------------
            // Inner drawer: CellStateDrawer
            // ----------------------------------------------------------------
            class CellStateDrawer : public Dia::Debug::IVisualDebugger
            {
            public:
                CellStateDrawer(
                    const Dia::GridVisibility::GridVisibilitySystem<TGraph>& system,
                    const Dia::GridVisibility::VisibilityGroupId&             selectedGroup,
                    float                                                     cellSize)
                    : mSystem(system)
                    , mSelectedGroup(&selectedGroup)
                    , mCellSize(cellSize)
                {}

                Dia::Core::StringCRC GetLayerName() const override
                {
                    return Dia::Core::StringCRC("GV.CellState");
                }

                void Draw(Dia::Core::IDebugDraw& draw) override
                {
                    // stub — full implementation in Task 4
                }

            private:
                const Dia::GridVisibility::GridVisibilitySystem<TGraph>& mSystem;
                const Dia::GridVisibility::VisibilityGroupId*             mSelectedGroup;
                float                                                     mCellSize;
            };

            // ----------------------------------------------------------------
            // Inner drawer: SightRadiiDrawer
            // ----------------------------------------------------------------
            class SightRadiiDrawer : public Dia::Debug::IVisualDebugger
            {
            public:
                SightRadiiDrawer(
                    const Dia::GridVisibility::GridVisibilitySystem<TGraph>& system,
                    const Dia::GridVisibility::VisibilityGroupId&             selectedGroup,
                    float                                                     cellSize)
                    : mSystem(system)
                    , mSelectedGroup(&selectedGroup)
                    , mCellSize(cellSize)
                {}

                Dia::Core::StringCRC GetLayerName() const override
                {
                    return Dia::Core::StringCRC("GV.SightRadii");
                }

                void Draw(Dia::Core::IDebugDraw& draw) override
                {
                    // stub — full implementation in Task 5
                }

            private:
                const Dia::GridVisibility::GridVisibilitySystem<TGraph>& mSystem;
                const Dia::GridVisibility::VisibilityGroupId*             mSelectedGroup;
                float                                                     mCellSize;
            };

            // ----------------------------------------------------------------
            // Inner drawer: ShadowcastBoundaryDrawer
            // ----------------------------------------------------------------
            class ShadowcastBoundaryDrawer : public Dia::Debug::IVisualDebugger
            {
            public:
                ShadowcastBoundaryDrawer(
                    const Dia::GridVisibility::GridVisibilitySystem<TGraph>& system,
                    const Dia::GridVisibility::VisibilityGroupId&             selectedGroup,
                    float                                                     cellSize)
                    : mSystem(system)
                    , mSelectedGroup(&selectedGroup)
                    , mCellSize(cellSize)
                {
                    SetEnabled(false);
                }

                Dia::Core::StringCRC GetLayerName() const override
                {
                    return Dia::Core::StringCRC("GV.Boundary");
                }

                void Draw(Dia::Core::IDebugDraw& draw) override
                {
                    // stub — full implementation in Task 6
                }

            private:
                const Dia::GridVisibility::GridVisibilitySystem<TGraph>& mSystem;
                const Dia::GridVisibility::VisibilityGroupId*             mSelectedGroup;
                float                                                     mCellSize;
            };

            // ----------------------------------------------------------------
            // GridVisibilityDebugDomain constructor
            // ----------------------------------------------------------------
            GridVisibilityDebugDomain(
                const Dia::GridVisibility::GridVisibilitySystem<TGraph>& system,
                const Dia::EntitySpatial::EntitySpatialModule&            spatial,
                float                                                     cellSize)
                : mSystem(system)
                , mSpatial(spatial)
                , mCellSize(cellSize)
                , mSelectedGroup{}
                , mCellStateDrawer(system, mSelectedGroup, cellSize)
                , mSightRadiiDrawer(system, mSelectedGroup, cellSize)
                , mBoundaryDrawer(system, mSelectedGroup, cellSize)
                , mLayerManager(nullptr)
            {}

            // ----------------------------------------------------------------
            // IDebugDomain — Identity
            // ----------------------------------------------------------------

            Dia::Core::StringCRC GetDomainId() const override
            {
                return Dia::Core::StringCRC("GridVisibility");
            }

            const char* GetDisplayName() const override
            {
                return "Grid Visibility";
            }

            const char* GetDescription() const override
            {
                return "Grid visibility — cell state overlay, sight radii, shadowcast boundary";
            }

            Dia::Core::StringCRC GetGroup() const override
            {
                return Dia::Core::StringCRC("Spatial");
            }

            Dia::Core::RGBA GetAccentColour() const override
            {
                return Dia::VisualDebugger::DebugGroupAccents::kSpatial;
            }

            // ----------------------------------------------------------------
            // IDebugDomain — World-space drawer capability
            // ----------------------------------------------------------------

            bool HasWorldDrawers() const override { return true; }

            // ----------------------------------------------------------------
            // IDebugDomain — Lifecycle
            // ----------------------------------------------------------------

            void Register(Dia::Debug::DebugLayerManager& mgr) override
            {
                if (mLayerManager != nullptr) return;
                mgr.Register(&mCellStateDrawer,   10, Dia::Core::StringCRC("GridVisibility"));
                mgr.Register(&mSightRadiiDrawer,  20, Dia::Core::StringCRC("GridVisibility"));
                mgr.Register(&mBoundaryDrawer,    30, Dia::Core::StringCRC("GridVisibility"));
                mLayerManager = &mgr;
            }

            void Unregister(Dia::Debug::DebugLayerManager& mgr) override
            {
                mgr.Unregister(mCellStateDrawer.GetLayerName());
                mgr.Unregister(mSightRadiiDrawer.GetLayerName());
                mgr.Unregister(mBoundaryDrawer.GetLayerName());
                mLayerManager = nullptr;
            }

            // ----------------------------------------------------------------
            // IDebugDomain — Data bridge to DiaDebugPanel
            // ----------------------------------------------------------------

            void GetJSONState(Json::Value& out) override
            {
                // stub — full impl in Task 7
                out["drawers"]       = Json::Value(Json::arrayValue);
                out["stats"]         = Json::Value(Json::objectValue);
                out["selectedGroup"] = Json::Value("");
                out["groups"]        = Json::Value(Json::arrayValue);
            }

            void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override
            {
                // stub — full impl in Task 7
            }

            // ----------------------------------------------------------------
            // IDebugDomain — World-space drawer access
            // ----------------------------------------------------------------

            int GetDrawerCount() const override { return 3; }

            Dia::Debug::IVisualDebugger* GetDrawer(int index) override
            {
                switch (index)
                {
                    case 0: return &mCellStateDrawer;
                    case 1: return &mSightRadiiDrawer;
                    case 2: return &mBoundaryDrawer;
                    default: return nullptr;
                }
            }

        private:
            const Dia::GridVisibility::GridVisibilitySystem<TGraph>& mSystem;
            const Dia::EntitySpatial::EntitySpatialModule&            mSpatial;
            float                                                     mCellSize;

            Dia::GridVisibility::VisibilityGroupId mSelectedGroup;

            CellStateDrawer          mCellStateDrawer;
            SightRadiiDrawer         mSightRadiiDrawer;
            ShadowcastBoundaryDrawer mBoundaryDrawer;

            Dia::Debug::DebugLayerManager* mLayerManager = nullptr;
        };

    } // namespace GridVisibilityVisualDebugger
} // namespace Dia

#endif // DIA_DEBUG
