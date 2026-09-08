#pragma once
// =============================================================================
// DiaAnimation2DSerializers.h
// serialize() free functions for DiaAnimation2D value types.
//
// TYPES COVERED:
//   SpringNodeDef, SpringChainDef, BoneMask,
//   Keyframe, KeyframeTrack, AnimClipDef
//   All structs are fully public — no friend declarations required.
//
// ADL NOTE:
//   All serialize() functions are placed in namespace Dia::Animation2D (same
//   namespace as the types), so Argument-Dependent Lookup finds them
//   automatically when an archive calls serialize(ar, value, version).
//
// USAGE:
//   #include "DiaCore/Reflect/Reflect.h"
//   #include "DiaCore/Reflect/JsonArchive.h"
//   #include "DiaAnimation2D/DiaAnimation2DSerializers.h"
// =============================================================================

#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaCore/Reflect/DiaCoreSerializers.h"
#include "DiaAnimation2D/SpringNodeDef.h"
#include "DiaAnimation2D/SpringChainDef.h"
#include "DiaAnimation2D/BoneMask.h"
#include "DiaAnimation2D/Keyframe.h"
#include "DiaAnimation2D/KeyframeTrack.h"
#include "DiaAnimation2D/AnimClipDef.h"

namespace Dia { namespace Animation2D {

// -----------------------------------------------------------------------------
// SpringNodeDef — stiffness, damping, maxAngularVelocity (float)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(SpringNodeDef, 1)
    DIA_FIELD(stiffness)
    DIA_FIELD(damping)
    DIA_FIELD(maxAngularVelocity)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// SpringChainDef — id, rootBoneId (StringCRC); boneIds, nodeOverrides (arrays);
//                  defaultNode (SpringNodeDef); gravityDirection (Vector2D);
//                  gravityStrength (float)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(SpringChainDef, 1)
    DIA_FIELD(id)
    DIA_FIELD(rootBoneId)
    DIA_FIELD(boneIds)
    DIA_FIELD(defaultNode)
    DIA_FIELD(nodeOverrides)
    DIA_FIELD(gravityDirection)
    DIA_FIELD(gravityStrength)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// BoneMask — id (StringCRC); boneIds (DynamicArrayC<StringCRC>)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(BoneMask, 1)
    DIA_FIELD(id)
    DIA_FIELD(boneIds)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Keyframe — time, rotation (float); position, scale (Vector2D)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Keyframe, 1)
    DIA_FIELD(time)
    DIA_FIELD(rotation)
    DIA_FIELD(position)
    DIA_FIELD(scale)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// KeyframeTrack — boneId (StringCRC); rotationOnly (bool);
//                 keyframes (DynamicArrayC<Keyframe>)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(KeyframeTrack, 1)
    DIA_FIELD(boneId)
    DIA_FIELD(rotationOnly)
    DIA_FIELD(keyframes)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// AnimClipDef — id (StringCRC); duration (float);
//               tracks (DynamicArrayC<KeyframeTrack>)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(AnimClipDef, 1)
    DIA_FIELD(id)
    DIA_FIELD(duration)
    DIA_FIELD(tracks)
DIA_SERIALIZE_END

} }  // namespace Dia::Animation2D
