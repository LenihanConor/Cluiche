////////////////////////////////////////////////////////////////////////////////
// Filename: DebugLayerNames.h
// Description: Canonical StringCRC constants for all Dia-owned debug layer names.
// Use these constants when registering draw classes and when toggling layers.
//
// Priority tiers (guidance):
//   0-9   : background / grid layers
//   10-19 : physics / geometry layers
//   20-29 : body / soft-body layers
//   30-39 : rig / animation layers
//   40-49 : IK layers
//   50+   : overlay / UI layers
//
// inline const avoids ODR violations when included in multiple TUs (C++20 required by PD-007).
// Note: StringCRC constructor is not constexpr, so constexpr cannot be used here.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
    namespace Debug
    {
        namespace LayerNames
        {
            // ----------------------------------------------------------------
            // Rig
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kRigBones    { "rig.bones"    };
            inline const Dia::Core::StringCRC kRigJoints   { "rig.joints"   };
            inline const Dia::Core::StringCRC kRigArrows   { "rig.arrows"   };
            inline const Dia::Core::StringCRC kRigRestPose { "rig.rest_pose" };
            inline const Dia::Core::StringCRC kRigLabels   { "rig.labels"   };

            // ----------------------------------------------------------------
            // Physics
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kPhysicsShapes      { "physics.shapes"      };
            inline const Dia::Core::StringCRC kPhysicsAABB        { "physics.aabb"        };
            inline const Dia::Core::StringCRC kPhysicsVelocity    { "physics.velocity"    };
            inline const Dia::Core::StringCRC kPhysicsContacts    { "physics.contacts"    };
            inline const Dia::Core::StringCRC kPhysicsConstraints { "physics.constraints" };

            // ----------------------------------------------------------------
            // Soft body
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kSoftParticles   { "soft.particles"   };
            inline const Dia::Core::StringCRC kSoftConstraints { "soft.constraints" };
            inline const Dia::Core::StringCRC kSoftAnchors     { "soft.anchors"     };
            inline const Dia::Core::StringCRC kSoftVelocity    { "soft.velocity"    };

            // ----------------------------------------------------------------
            // IK
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kIKChains      { "ik.chains"      };
            inline const Dia::Core::StringCRC kIKTargets     { "ik.targets"     };
            inline const Dia::Core::StringCRC kIKPoleVectors { "ik.pole_vectors" };
            inline const Dia::Core::StringCRC kIKLimits      { "ik.limits"      };
            inline const Dia::Core::StringCRC kIKConvergence { "ik.convergence" };

            // IK draw-class layer names (DiaIK2DVisualDebugger)
            inline const Dia::Core::StringCRC kIKBones   { "ik.bones"   };
            inline const Dia::Core::StringCRC kIKJoints  { "ik.joints"  };
            inline const Dia::Core::StringCRC kIKArrows  { "ik.arrows"  };
            inline const Dia::Core::StringCRC kIKReach   { "ik.reach"   };

            // ----------------------------------------------------------------
            // Geometry
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kGeoShapes      { "geometry.shapes"       };
            inline const Dia::Core::StringCRC kGeoAABB        { "geometry.aabb"          };
            inline const Dia::Core::StringCRC kGeoLabels      { "geometry.labels"        };
            inline const Dia::Core::StringCRC kGeoSpatialGrid { "geometry.spatial_grid"  };
            inline const Dia::Core::StringCRC kGeoHexGrid     { "geometry.hexgrid"       };
            inline const Dia::Core::StringCRC kGeoQuadtree    { "geometry.quadtree"      };
            inline const Dia::Core::StringCRC kGeoBVH         { "geometry.bvh"           };
            inline const Dia::Core::StringCRC kGeoContacts    { "geometry.contacts"      };

            // ----------------------------------------------------------------
            // Asset
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kAssetRuntime { "asset.runtime" };

            // ----------------------------------------------------------------
            // Animation
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kAnimSpring       { "anim.spring"        };
            inline const Dia::Core::StringCRC kAnimClipCursor   { "anim.clip_cursor"   };
            inline const Dia::Core::StringCRC kAnimBlendWeights { "anim.blend_weights" };

            // ----------------------------------------------------------------
            // Scene2D
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kScene2DOverview { "scene2d.overview" };

            // ----------------------------------------------------------------
            // Coord2D
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kCoord2DOrigin { "coord2d.origin" };
            inline const Dia::Core::StringCRC kCoord2DAxes   { "coord2d.axes"   };
            inline const Dia::Core::StringCRC kCoord2DGrid   { "coord2d.grid"   };
            inline const Dia::Core::StringCRC kCoord2DBounds { "coord2d.bounds" };
            inline const Dia::Core::StringCRC kCoord2DCursor { "coord2d.cursor" };

            // Stage tag used to register all coord2d layers (creates "Coord2D" console tab)
            inline const Dia::Core::StringCRC kCoord2DStageTag { "Coord2D" };

            // ----------------------------------------------------------------
            // Coord3D
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kCoord3DOrigin { "coord3d.origin" };
            inline const Dia::Core::StringCRC kCoord3DAxes   { "coord3d.axes"   };
            inline const Dia::Core::StringCRC kCoord3DGrid   { "coord3d.grid"   };
            inline const Dia::Core::StringCRC kCoord3DCamera { "coord3d.camera" };

            // Stage tag used to register all coord3d layers (creates "Coord3D" console tab)
            inline const Dia::Core::StringCRC kCoord3DStageTag { "Coord3D" };

            // ----------------------------------------------------------------
            // Entity
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kEntityLabels    { "entity.labels"    };
            inline const Dia::Core::StringCRC kEntityHierarchy { "entity.hierarchy" };
            inline const Dia::Core::StringCRC kEntityHighlight { "entity.highlight" };
            inline const Dia::Core::StringCRC kEntityPicking   { "entity.picking"   };
            inline const Dia::Core::StringCRC kEntityStats     { "entity.stats"     };
            inline const Dia::Core::StringCRC kEntityInspector { "entity.inspector" };

            // Stage tag used to register all entity layers (creates "Entity" console tab)
            inline const Dia::Core::StringCRC kEntityStageTag { "Entity" };

            // ----------------------------------------------------------------
            // Mesh 3D (priority tier 10–19)
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kMesh3DBounds  { "mesh3d.bounds"  };
            inline const Dia::Core::StringCRC kMesh3DOrigins { "mesh3d.origins" };
            inline const Dia::Core::StringCRC kMesh3DStats   { "mesh3d.stats"   };

            // ----------------------------------------------------------------
            // Lighting 3D (priority tier 10–19)
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kLightWidgets { "light3d.widgets"  };
            inline const Dia::Core::StringCRC kLightPathArc { "light3d.path_arc" };

            // ----------------------------------------------------------------
            // Utility AI (priority tier 50+)
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kUtilityAIScores { "utility_ai.scores" };

            // ----------------------------------------------------------------
            // Scalar Field (priority tier 0 — background/spatial data)
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kScalarFieldHeatmap  { "scalarfield.heatmap"  };
            inline const Dia::Core::StringCRC kScalarFieldGradient { "scalarfield.gradient" };

        } // namespace LayerNames
    } // namespace Debug
} // namespace Dia
