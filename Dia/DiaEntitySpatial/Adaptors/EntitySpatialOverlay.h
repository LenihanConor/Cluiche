// EntitySpatialOverlay.h
// Header-only debug overlay adaptors for DiaEntitySpatial.
// Provides three independently-togglable IVisualDebugger implementations:
//   EntitySpatialGridOverlay     — draws grid cell outlines (square or hex)
//   EntitySpatialEntityOverlay   — draws spatially-indexed entities as circles + labels
//   EntitySpatialQueryOverlay    — draws the most recent query shape and matched entities
//
// OPTIONAL ADAPTOR — not part of DiaEntitySpatial.vcxproj build.
// DiaEntitySpatial.vcxproj carries NO hard dependency on DiaVisualDebugger.
// Consumers must add DiaVisualDebugger to their own project dependencies.
//
// Namespace: Dia::EntitySpatial::Adaptors

#pragma once

#include <DiaEntitySpatial/EntitySpatialIndex.h>
#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace Dia
{
    namespace EntitySpatial
    {
        namespace Adaptors
        {

            // ----------------------------------------------------------------
            // Constants
            // ----------------------------------------------------------------

            static constexpr int kMaxLayerColours = 8;

            // ----------------------------------------------------------------
            // EntityOverlayConfig — palette + hit highlight config
            // ----------------------------------------------------------------

            struct EntityOverlayConfig
            {
                Dia::Core::RGBA layerColours[kMaxLayerColours] = {
                    Dia::Core::RGBA(100, 149, 237, 200), // cornflower blue
                    Dia::Core::RGBA( 50, 205,  50, 200), // lime green
                    Dia::Core::RGBA(255, 165,   0, 200), // orange
                    Dia::Core::RGBA(220,  20,  60, 200), // crimson
                    Dia::Core::RGBA(147, 112, 219, 200), // medium purple
                    Dia::Core::RGBA(  0, 206, 209, 200), // dark turquoise
                    Dia::Core::RGBA(255, 105, 180, 200), // hot pink
                    Dia::Core::RGBA(210, 180, 140, 200), // tan
                };
                Dia::Core::RGBA hitColour  = Dia::Core::RGBA(255, 255,   0, 220); // yellow
                bool            showLabels = false;
                float           labelSize  = 12.0f;
            };

            // ----------------------------------------------------------------
            // QueryDescriptor — describes a query shape for the query overlay
            // ----------------------------------------------------------------

            struct QueryDescriptor
            {
                enum class Shape { Circle, Region, Ray, Sector, KNearest };

                Shape                shape  = Shape::Circle;
                Dia::Maths::Vector2D origin;

                // Circle / KNearest
                float radius   = 0.0f;
                // KNearest k
                int   k        = 0;
                // Ray
                Dia::Maths::Vector2D dir;
                float maxDist  = 0.0f;
                // Sector
                float halfAngle = 0.0f;
                // Region
                Dia::Geometry2D::AARect rect;
            };

            // ================================================================
            // EntitySpatialGridOverlay
            // ================================================================

            class EntitySpatialGridOverlay : public Dia::Debug::IVisualDebugger
            {
            public:
                // Square topology constructor
                EntitySpatialGridOverlay(const EntitySpatialIndex::SquareDef& def,
                                         Dia::Core::StringCRC layerName,
                                         Dia::Core::RGBA cellColour = Dia::Core::RGBA(80, 80, 80, 120))
                    : mLayerName(layerName)
                    , mCellColour(cellColour)
                    , mTopology(EntitySpatialIndex::Topology::SquareGrid)
                    , mSquareDef(def)
                    , mHexDef()
                {}

                // Hex topology constructor
                EntitySpatialGridOverlay(const EntitySpatialIndex::HexDef& def,
                                         Dia::Core::StringCRC layerName,
                                         Dia::Core::RGBA cellColour = Dia::Core::RGBA(80, 80, 80, 120))
                    : mLayerName(layerName)
                    , mCellColour(cellColour)
                    , mTopology(EntitySpatialIndex::Topology::HexGrid)
                    , mSquareDef()
                    , mHexDef(def)
                {}

                Dia::Core::StringCRC GetLayerName() const override { return mLayerName; }

                void Draw(Dia::Core::IDebugDraw& draw) override
                {
                    if (!IsEnabled())
                        return;

                    if (mTopology == EntitySpatialIndex::Topology::SquareGrid)
                    {
                        DrawSquareGrid(draw);
                    }
                    else
                    {
                        DrawHexGrid(draw);
                    }
                }

            private:

                void DrawSquareGrid(Dia::Core::IDebugDraw& draw) const
                {
                    const Dia::Maths::Vector2D& bl = mSquareDef.worldBounds.GetBottomLeft();
                    const Dia::Maths::Vector2D& tr = mSquareDef.worldBounds.GetTopRight();
                    const float cs = mSquareDef.cellSize;

                    if (cs <= 0.0f)
                        return;

                    const float width  = tr.x - bl.x;
                    const float height = tr.y - bl.y;

                    const int cols = static_cast<int>(width  / cs);
                    const int rows = static_cast<int>(height / cs);

                    for (int row = 0; row < rows; ++row)
                    {
                        for (int col = 0; col < cols; ++col)
                        {
                            const float minX = bl.x + static_cast<float>(col) * cs;
                            const float minY = bl.y + static_cast<float>(row) * cs;
                            const float maxX = minX + cs;
                            const float maxY = minY + cs;

                            draw.RequestDrawRect(
                                Dia::Maths::Vector2D(minX, minY),
                                Dia::Maths::Vector2D(maxX, maxY),
                                mCellColour);
                        }
                    }
                }

                void DrawHexGrid(Dia::Core::IDebugDraw& draw) const
                {
                    const float hr = mHexDef.hexRadius;
                    if (hr <= 0.0f)
                        return;

                    // Pointy-top hex: vertex i is at angle (30 + 60*i) degrees
                    static constexpr float kPI = 3.14159265358979323846f;

                    // Pre-compute 6 unit vertex offsets (relative to centre)
                    float vx[6], vy[6];
                    for (int i = 0; i < 6; ++i)
                    {
                        const float angleDeg = 30.0f + 60.0f * static_cast<float>(i);
                        const float angleRad = angleDeg * kPI / 180.0f;
                        vx[i] = hr * std::cos(angleRad);
                        vy[i] = hr * std::sin(angleRad);
                    }

                    for (int r = 0; r < mHexDef.rowCount; ++r)
                    {
                        for (int q = 0; q < mHexDef.colCount; ++q)
                        {
                            // World-space centre for axial (q, r), pointy-top
                            const float cx = mHexDef.origin.x
                                + hr * (static_cast<float>(1.7320508f) * static_cast<float>(q)
                                        + static_cast<float>(1.7320508f) * 0.5f * static_cast<float>(r));
                            const float cy = mHexDef.origin.y
                                + hr * (1.5f * static_cast<float>(r));

                            // Draw 6 edges
                            for (int i = 0; i < 6; ++i)
                            {
                                const int j = (i + 1) % 6;
                                draw.RequestDraw(
                                    Dia::Maths::Vector2D(cx + vx[i], cy + vy[i]),
                                    Dia::Maths::Vector2D(cx + vx[j], cy + vy[j]),
                                    mCellColour);
                            }
                        }
                    }
                }

                Dia::Core::StringCRC              mLayerName;
                Dia::Core::RGBA                   mCellColour;
                EntitySpatialIndex::Topology      mTopology;
                EntitySpatialIndex::SquareDef     mSquareDef;
                EntitySpatialIndex::HexDef        mHexDef;
            };

            // ================================================================
            // EntitySpatialEntityOverlay
            // ================================================================

            class EntitySpatialEntityOverlay : public Dia::Debug::IVisualDebugger
            {
            public:
                EntitySpatialEntityOverlay(const EntitySpatialModule& /*module*/,
                                            const Dia::Entity::Domain& domain,
                                            Dia::Core::StringCRC layerName,
                                            EntityOverlayConfig config = EntityOverlayConfig())
                    : mDomain(domain)
                    , mLayerName(layerName)
                    , mConfig(config)
                {}

                Dia::Core::StringCRC GetLayerName() const override { return mLayerName; }

                void SetQueryHits(const Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 64>& hits)
                {
                    mQueryHits.Assign(hits);
                }

                void SetLabelSize(float s) { mConfig.labelSize = s; }

                void Draw(Dia::Core::IDebugDraw& draw) override
                {
                    if (!IsEnabled())
                    {
                        mQueryHits.RemoveAll();
                        return;
                    }

                    for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain; ++i)
                    {
                        const Dia::Entity::Entity entity = mDomain.GetAliveEntity(i);
                        if (!entity.IsValid())
                            continue;

                        const SpatialComponent* sc = mDomain.GetComponent<SpatialComponent>(entity);
                        if (sc == nullptr)
                            continue;

                        const Dia::Maths::Vector2D pos    = sc->position;
                        const float                radius = sc->radius;

                        // Determine colour: hit colour if in query hits, else palette by layer mask
                        const bool isHit = IsQueryHit(entity);
                        Dia::Core::RGBA colour;
                        if (isHit)
                        {
                            colour = mConfig.hitColour;
                        }
                        else
                        {
                            const uint32_t layerMask  = sc->GetLayerMask();
                            const uint32_t paletteIdx = layerMask % static_cast<uint32_t>(kMaxLayerColours);
                            colour = mConfig.layerColours[paletteIdx];
                        }

                        draw.RequestDraw(pos, radius, colour);

                        if (mConfig.showLabels)
                        {
                            char labelBuf[32];
                            std::snprintf(labelBuf, sizeof(labelBuf), "%u", entity.GetIndex());
                            draw.RequestDrawText(pos, labelBuf, mConfig.labelSize, colour);
                        }
                    }

                    // Clear hits after draw (persist for one frame)
                    mQueryHits.RemoveAll();
                }

            private:

                bool IsQueryHit(const Dia::Entity::Entity& entity) const
                {
                    for (uint32_t i = 0; i < mQueryHits.Size(); ++i)
                    {
                        if (mQueryHits[i] == entity)
                            return true;
                    }
                    return false;
                }

                const Dia::Entity::Domain& mDomain;
                Dia::Core::StringCRC       mLayerName;
                EntityOverlayConfig        mConfig;
                Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 64> mQueryHits;
            };

            // ================================================================
            // EntitySpatialQueryOverlay
            // ================================================================

            class EntitySpatialQueryOverlay : public Dia::Debug::IVisualDebugger
            {
            public:
                static constexpr int kMaxRetained = 1;

                explicit EntitySpatialQueryOverlay(
                    Dia::Core::StringCRC layerName,
                    Dia::Core::RGBA queryColour = Dia::Core::RGBA(0, 255, 200, 180))
                    : mLayerName(layerName)
                    , mQueryColour(queryColour)
                    , mDescriptorCount(0)
                {}

                Dia::Core::StringCRC GetLayerName() const override { return mLayerName; }

                void PushQuery(const QueryDescriptor& desc)
                {
                    // Ring: keep only the last kMaxRetained
                    mDescriptors[0]  = desc;
                    mDescriptorCount = 1;
                }

                void Draw(Dia::Core::IDebugDraw& draw) override
                {
                    if (!IsEnabled())
                        return;

                    for (int i = 0; i < mDescriptorCount; ++i)
                    {
                        DrawDescriptor(draw, mDescriptors[i]);
                    }
                }

            private:

                void DrawDescriptor(Dia::Core::IDebugDraw& draw, const QueryDescriptor& desc) const
                {
                    static constexpr float kPI = 3.14159265358979323846f;
                    static constexpr float kRad2Deg = 180.0f / kPI;

                    switch (desc.shape)
                    {
                        case QueryDescriptor::Shape::Circle:
                        {
                            draw.RequestDraw(desc.origin, desc.radius, mQueryColour);
                            break;
                        }

                        case QueryDescriptor::Shape::Region:
                        {
                            draw.RequestDrawRect(
                                desc.rect.GetBottomLeft(),
                                desc.rect.GetTopRight(),
                                mQueryColour);
                            break;
                        }

                        case QueryDescriptor::Shape::Ray:
                        {
                            // Normalise dir before passing to RequestDrawRay
                            const float dx = desc.dir.x;
                            const float dy = desc.dir.y;
                            const float len = std::sqrt(dx * dx + dy * dy);
                            if (len > 0.0001f)
                            {
                                draw.RequestDrawRay(
                                    desc.origin,
                                    Dia::Maths::Vector2D(dx / len, dy / len),
                                    desc.maxDist,
                                    mQueryColour);
                            }
                            break;
                        }

                        case QueryDescriptor::Shape::Sector:
                        {
                            // Arc: from (dir rotated by -halfAngle) to (dir rotated by +halfAngle)
                            const float dirAngleRad = std::atan2(desc.dir.y, desc.dir.x);
                            const float startRad = dirAngleRad - desc.halfAngle;
                            const float endRad   = dirAngleRad + desc.halfAngle;

                            const float startDeg = startRad * kRad2Deg;
                            const float endDeg   = endRad   * kRad2Deg;

                            draw.RequestDrawArc(desc.origin, desc.radius, startDeg, endDeg, mQueryColour);

                            // Two radius lines from origin to arc endpoints
                            const Dia::Maths::Vector2D p1(
                                desc.origin.x + desc.radius * std::cos(startRad),
                                desc.origin.y + desc.radius * std::sin(startRad));
                            const Dia::Maths::Vector2D p2(
                                desc.origin.x + desc.radius * std::cos(endRad),
                                desc.origin.y + desc.radius * std::sin(endRad));

                            draw.RequestDraw(desc.origin, p1, mQueryColour);
                            draw.RequestDraw(desc.origin, p2, mQueryColour);
                            break;
                        }

                        case QueryDescriptor::Shape::KNearest:
                        {
                            // Draw a unit circle at origin; no bound radius
                            draw.RequestDraw(desc.origin, 10.0f, mQueryColour);
                            break;
                        }

                        default:
                            break;
                    }
                }

                Dia::Core::StringCRC mLayerName;
                Dia::Core::RGBA      mQueryColour;
                QueryDescriptor      mDescriptors[kMaxRetained];
                int                  mDescriptorCount;
            };

        } // namespace Adaptors
    } // namespace EntitySpatial
} // namespace Dia
