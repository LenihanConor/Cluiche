#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include "AnimClipDef.h"

namespace Json { class Value; }

namespace Dia { namespace Animation2D {
    AnimClipDef LoadAnimClipDefFromJson(const Json::Value& root);
    AnimClipDef LoadAnimClipDefFromSpine(const Json::Value& spineRoot, const Dia::Core::StringCRC& animationName);
} }
