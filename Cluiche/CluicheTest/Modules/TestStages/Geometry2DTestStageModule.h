#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/OORect.h>
#include <DiaGeometry2D/Shapes/Line.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaGeometry2D/Shapes/Triangle.h>
#include <DiaGeometry2D/Shapes/Capsule.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaGeometry2D/Shapes/Arc.h>
#include <DiaGeometry2D/Shapes/Sector.h>

#ifdef DIA_DEBUG
#include "Modules/TestStages/Drawers/Geometry2DIntersectionsDrawer.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaGeometry2D/Spatial/BVH.h>
#include <DiaGeometry2D/Spatial/Quadtree.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaGeometry2D/Spatial/HexGrid.h>
#include <DiaGeometry2DVisualDebugger/BVHDrawer.h>
#include <DiaGeometry2DVisualDebugger/QuadtreeDrawer.h>
#include <DiaGeometry2DVisualDebugger/SpatialGridDrawer.h>
#include <DiaGeometry2DVisualDebugger/HexGridDrawer.h>
#include <DiaGeometry2DPicking/Adapters/HexGridPickable.h>
#include <DiaGeometry2DPicking/Adapters/SpatialGridPickable.h>
#include <DiaGeometry2DPicking/PickHit2D.h>
#include <DiaPicking/PickEvent.h>
#include <DiaMailbox/MailboxTypes.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include "Modules/PickingModule.h"
#include <memory>
#endif

namespace CluicheTest {

#ifdef DIA_DEBUG
class Geometry2DShapesDrawer;
class Geometry2DLabelsDrawer;
class Geometry2DAABBDrawer;
#endif

class Geometry2DTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Validates DiaGeometry2D: gallery of all shapes, 6 intersection pairs";
    explicit Geometry2DTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 60; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    void SetupGallery();
    void SetupSpatialStructures();

    // Gallery shapes (10 primitives) — fixed world positions
    Dia::Geometry2D::Circle         mCircle;
    Dia::Geometry2D::AARect         mAARect;
    Dia::Geometry2D::OORect         mOORect;
    Dia::Geometry2D::Line           mLine;
    Dia::Geometry2D::Ray            mRay;
    Dia::Geometry2D::Triangle       mTriangle;
    Dia::Geometry2D::Capsule        mCapsule;
    Dia::Geometry2D::ConvexPolygon  mConvexPoly;
    Dia::Geometry2D::Arc            mArc;
    Dia::Geometry2D::Sector         mSector;

    int mShapeCount = 0;

    // Metrics
    Dia::Observation::Metric::Gauge* mMetricShapeCount          = nullptr;
    Dia::Observation::Metric::Gauge* mMetricIntersectionPairCount = nullptr;

#ifdef DIA_DEBUG
    void SetupIntersectionPairs();

    // Intersection pairs (debug only — drive visual overlay)
    Dia::Core::Containers::DynamicArrayC<IntersectionPair, 6> mIntersectionPairs;
    int mIntersectionPairCount = 0;

    // Spatial structures (debug only)
    using SpatialElem = unsigned int;
    static constexpr unsigned int kSpatialMax = 64;
    static constexpr unsigned int kMaxScatterShapes = 24;
    Dia::Core::Containers::DynamicArrayC<Dia::Geometry2D::AARect, kMaxScatterShapes> mSpatialScatter;

    std::unique_ptr<Dia::Geometry2D::BVH<SpatialElem, kSpatialMax>>         mBVH;
    std::unique_ptr<Dia::Geometry2D::Quadtree<SpatialElem, kSpatialMax>>    mQuadtree;
    std::unique_ptr<Dia::Geometry2D::SpatialGrid<SpatialElem, kSpatialMax>> mSpatialGrid;
    std::unique_ptr<Dia::Geometry2D::HexGrid<SpatialElem, kSpatialMax>>     mHexGrid;

    std::unique_ptr<Dia::Geometry2DVisualDebugger::BVHDrawer<SpatialElem, kSpatialMax>>         mBVHDrawer;
    std::unique_ptr<Dia::Geometry2DVisualDebugger::QuadtreeDrawer<SpatialElem, kSpatialMax>>    mQuadtreeDrawer;
    std::unique_ptr<Dia::Geometry2DVisualDebugger::SpatialGridDrawer<SpatialElem, kSpatialMax>> mSpatialGridDrawer;
    std::unique_ptr<Dia::Geometry2DVisualDebugger::HexGridDrawer<SpatialElem, kSpatialMax>>     mHexGridDrawer;

    std::unique_ptr<Geometry2DShapesDrawer>        mShapesDrawer;
    std::unique_ptr<Geometry2DLabelsDrawer>        mLabelsDrawer;
    std::unique_ptr<Geometry2DIntersectionsDrawer> mIntersectionsDrawer;
    std::unique_ptr<Geometry2DAABBDrawer>          mAABBDrawer;

    // Picking — same PU (SimPU), accessed via ModuleRef
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::PickingModule> mPickingRef{this};
    std::unique_ptr<Dia::Geometry2DPicking::HexGridPickable<SpatialElem, kSpatialMax>>     mHexGridPickable;
    std::unique_ptr<Dia::Geometry2DPicking::SpatialGridPickable<SpatialElem, kSpatialMax>> mSpatialGridPickable;
    Dia::Mailbox::SubscriberId     mPickSubscriberId{};
    bool                           mPickingRegistered{ false };

    // Selection state — owned here, passed by pointer to drawers each frame
    bool                           mHasHexSelection{ false };
    Dia::Geometry2D::HexCoord      mSelectedHex{ -1, -1 };
    bool                           mHasSpatialSelection{ false };
    Dia::Geometry2DVisualDebugger::SpatialGridDrawer<SpatialElem, kSpatialMax>::CellCoord
                                   mSelectedCell{ -1, -1 };
#endif
};

} // namespace CluicheTest
