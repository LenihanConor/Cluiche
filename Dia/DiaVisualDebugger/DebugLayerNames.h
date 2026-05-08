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

#ifdef DIA_DEBUG

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
            inline const Dia::Core::StringCRC kGeoSpatialGrid { "geometry.spatial_grid"  };
            inline const Dia::Core::StringCRC kGeoQuadtree    { "geometry.quadtree"      };
            inline const Dia::Core::StringCRC kGeoBVH         { "geometry.bvh"           };
            inline const Dia::Core::StringCRC kGeoContacts    { "geometry.contacts"      };

            // ----------------------------------------------------------------
            // Animation
            // ----------------------------------------------------------------
            inline const Dia::Core::StringCRC kAnimSpring       { "anim.spring"        };
            inline const Dia::Core::StringCRC kAnimClipCursor   { "anim.clip_cursor"   };
            inline const Dia::Core::StringCRC kAnimBlendWeights { "anim.blend_weights" };

        } // namespace LayerNames
    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
