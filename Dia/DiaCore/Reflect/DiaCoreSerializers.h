#pragma once
// =============================================================================
// DiaCoreSerializers.h
// serialize() free functions for DiaCore String types and PathStoreConfig.
//
// MODULE BOUNDARY NOTE:
//   This header bridges DiaCore String/FilePath types with the DiaReflect
//   archive system. Include it explicitly in any translation unit that needs
//   to serialize these types.
//
// STRING TYPES:
//   String<N> and all concrete subtypes (String8, String32, String64,
//   String128, String256, String512, String1024) are handled natively by
//   the JsonArchive and BinaryArchive via an AsCStr() concept check.
//   No serialize() free functions are needed — including this header ensures
//   the string type headers are available so the concept check can match.
//
// PATHSTORECONFIG TYPES:
//   AliasPathConfigTuple, AliasAppendPathConfig, PathStoreConfigFragment,
//   and PathStoreConfig all have DIA_SERIALIZE blocks defined below.
//   PathStoreConfig.h has template friend declarations for these functions.
//
// ADL NOTE:
//   All serialize() functions are placed in namespace Dia::Core (same
//   namespace as the types), so Argument-Dependent Lookup finds them
//   automatically when an archive calls serialize(ar, value, version).
//
// USAGE:
//   #include "DiaCore/Reflect/Reflect.h"
//   #include "DiaCore/Reflect/JsonArchive.h"
//   #include "DiaCore/Reflect/DiaCoreSerializers.h"
// =============================================================================

#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String8.h"
#include "DiaCore/Strings/String32.h"
#include "DiaCore/Strings/String64.h"
#include "DiaCore/Strings/String128.h"
#include "DiaCore/Strings/String256.h"
#include "DiaCore/Strings/String512.h"
#include "DiaCore/Strings/String1024.h"
#include "DiaCore/FilePath/PathStoreConfig.h"

namespace Dia::Core {

// -----------------------------------------------------------------------------
// StringCRC  — serialized as a string (stores original text, max 64 chars).
//   On write: emits AsChar() as a JSON string.
//   On read:  reconstructs via operator=(const char*) through a String64 relay.
// -----------------------------------------------------------------------------
template<class Archive>
void serialize(Archive& ar, StringCRC& obj, unsigned /*version*/ = 1u) {
    Dia::Core::Containers::String64 relay;
    if (ar.IsWriting()) {
        relay = obj.AsChar();
    }
    ar & Dia::Reflect::named("value", relay);
    if (ar.IsReading()) {
        obj = relay.AsCStr();
    }
}

// -----------------------------------------------------------------------------
// AliasPathConfigTuple  — mAlias (String32), mPath (Path::String = String256)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(AliasPathConfigTuple, 1)
    DIA_FIELD(mAlias)
    DIA_FIELD(mPath)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// AliasAppendPathConfig — mAlias, mBaseAlias (String32), mPathAppend (String256)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(AliasAppendPathConfig, 1)
    DIA_FIELD(mAlias)
    DIA_FIELD(mBaseAlias)
    DIA_FIELD(mPathAppend)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// PathStoreConfigFragment — mBaseAlias, mFileName (String32), mPathAppend (String256)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(PathStoreConfigFragment, 1)
    DIA_FIELD(mBaseAlias)
    DIA_FIELD(mFileName)
    DIA_FIELD(mPathAppend)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// PathStoreConfig — three DynamicArrayC members of the above types
// -----------------------------------------------------------------------------
DIA_SERIALIZE(PathStoreConfig, 1)
    DIA_FIELD(mAliasPathTupleArray)
    DIA_FIELD(mAliasAppendPathArray)
    DIA_FIELD(mPathStoreConfigFragmentArray)
DIA_SERIALIZE_END

}  // namespace Dia::Core
