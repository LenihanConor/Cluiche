#pragma once
// =============================================================================
// DiaRig2DSerializers.h
// serialize() free functions for DiaRig2D types.
//
// USAGE:
//   #include "DiaRig2D/DiaRig2DSerializers.h"
//
// NOTE: Bone::metadata (DynamicArrayC<MetadataEntry>) is not serialized here.
//   MetadataValue contains a union + enum discriminator that is not yet
//   covered by DiaReflect. Metadata survives only in hand-authored assets
//   via the existing ISkeletonSerializer path.
//
// ADL NOTE:
//   All serialize() functions are in namespace Dia::Rig2D.
// =============================================================================

#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaCore/Reflect/DiaCoreSerializers.h"
#include "DiaMaths/DiaMathsSerializers.h"
#include "DiaRig2D/BoneTransform.h"
#include "DiaRig2D/Bone.h"
#include "DiaRig2D/Skeleton.h"

namespace Dia::Rig2D {

// -----------------------------------------------------------------------------
// BoneTransform  — all public: Vector2D position, float rotation, Vector2D scale
// -----------------------------------------------------------------------------
DIA_SERIALIZE(BoneTransform, 1)
    DIA_FIELD(position)
    DIA_FIELD(rotation)
    DIA_FIELD(scale)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Bone  — all public fields serialized except metadata (union type, deferred)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Bone, 1)
    DIA_FIELD(name)
    DIA_FIELD(parentIndex)
    DIA_FIELD(localPosition)
    DIA_FIELD(localRotation)
    DIA_FIELD(localScale)
    DIA_FIELD(length)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// SkeletonDef  — all public: StringCRC id, DynamicArrayC<Bone, kMaxBones> bones
// -----------------------------------------------------------------------------
DIA_SERIALIZE(SkeletonDef, 1)
    DIA_FIELD(id)
    DIA_FIELD(bones)
DIA_SERIALIZE_END

}  // namespace Dia::Rig2D
