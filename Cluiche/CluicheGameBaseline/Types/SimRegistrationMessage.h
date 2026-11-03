#pragma once
////////////////////////////////////////////////////////////////////////////////
// SimRegistrationMessage.h
//
// Registration side channel of the Sim-Hosted Queries protocol.
//
// SimPU modules cannot call the Main-side DebugServer directly (no cross-PU
// direct calls), so a sim-hosted NAME is announced over the same SimQueryReply
// EventStream that carries query replies.  A reply with the reserved
// requestId 0 (never issued by SimPendingTable) is a registration message:
//
//   register     {"register":{"name":"sim.echo","kind":"query","info":{...}},"seq":7}   kind: "query" | "command"
//   unregister   {"unregister":{"name":"sim.echo"},"seq":8}
//   digest       {"digest":{"seq":8,"count":12,"hash":"0x1a2b3c4d"}}
//   resync_begin {"resync_begin":{"seq":8,"count":12}}
//   replay       {"register":{"name":"sim.echo","kind":"query","info":{...}},"replay":true}   (carries no seq)
//
// info (required on register and replay; a missing or invalid one makes the message Malformed):
//   {"description":"..","category":"..","owner":"..","noArgs":false,
//    "params":[{"name":"..","type":"..","required":false,"description":"..","unit":"..","min":0,"max":1,"enum":"a|b"}]}
// description is mandatory and non-empty.  category/owner/noArgs/params and a param's description/unit/min+max/enum
// are omitted when empty or default; min and max appear together, only for a param with a range.  Limits are the
// kSimHelpMax* constants (SimHelpInfo.h).  Anything over a limit, or a message that would not fit the 16 KB event,
// is rejected, never truncated.  Help is NOT part of the name-set hash (SimNameSetEntryHash stays name+kind only).
//
// seq is a per-host uint32_t starting at 1, +1 per register/unregister, wrapping.  A register or
// unregister without seq that is not marked replay:true is Malformed (hard cutover).  hash is a hex
// string "0x%08x".  The set hash is the uint32 wrapping SUM of SimNameSetEntryHash over every announced
// name, so equal sets hash equal regardless of order.
//
// DebugServerHostModule (Main) parses these with ParseSimRegistrationReply and
// calls DebugServer::RegisterSimHosted / UnregisterSimHosted; every other reply
// goes to DebugServer::CompleteSimReply.  SimQueryHostModule (SimPU) builds
// them with BuildSimRegistrationReply.
//
// Pure functions, no state, so they are unit tested without streams.
////////////////////////////////////////////////////////////////////////////////

#include <DiaCore/Json/external/json/json.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaDebugServer/SimHostedTypes.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace Cluiche { namespace AppFlow {

static constexpr uint32_t kSimRegistrationRequestId = 0;
static constexpr uint32_t kSimRegistrationMaxName   = 96;

enum class SimRegistrationOp : uint32_t
{
    NotRegistration = 0,   // an ordinary reply (requestId != 0)
    Register        = 1,
    Unregister      = 2,
    Malformed       = 3,   // requestId 0 but the payload is not a valid registration
    Digest          = 4,
    ResyncBegin     = 5,
    Replay          = 6    // register re-sent during a resync; carries no seq
};

struct SimRegistration
{
    SimRegistrationOp               op   = SimRegistrationOp::NotRegistration;
    char                            name[kSimRegistrationMaxName] = {};
    Dia::DebugServer::SimKind       kind = Dia::DebugServer::SimKind::Query;
    uint32_t                        seq   = 0;   // Register / Unregister / Digest / ResyncBegin
    uint32_t                        count = 0;   // Digest / ResyncBegin
    uint32_t                        hash  = 0;   // Digest
    bool                            hasInfo = false;   // Register / Replay: a valid info block was present
    // The decoded help (~7 KB) is deliberately NOT a member: ParseSimRegistrationReply writes it to an optional
    // caller-owned SimHelpStored so no 7 KB value is copied around with every SimRegistration.
};

// Per-entry hash of (name, kind); the implementation lives in Dia (DiaDebugServer/SimHostedTypes.h) so the
// DebugServer digest and the sim host agree byte for byte.
using Dia::DebugServer::SimNameSetEntryHash;

namespace Detail
{
    inline bool NameValid(const char* name)
    {
        return name != nullptr && name[0] != '\0' && std::strlen(name) < kSimRegistrationMaxName;
    }

    inline bool Finish(const Json::Value& root, Dia::DebugServer::SimReplyEvent& out)
    {
        Json::FastWriter writer;
        writer.omitEndingLineFeed();
        const std::string text = writer.write(root);
        if (text.size() >= Dia::DebugServer::kSimPayloadBytes)
            return false;
        out.requestId = kSimRegistrationRequestId;
        out.status    = static_cast<uint32_t>(Dia::DebugServer::SimReplyStatus::Ok);
        std::memcpy(out.json, text.c_str(), text.size() + 1);
        return true;
    }

    inline const char* KindText(Dia::DebugServer::SimKind kind)
    {
        return (kind == Dia::DebugServer::SimKind::Command) ? "command" : "query";
    }

    // Strict uint32 read: member must exist and be a non-negative integer <= 0xFFFFFFFF.
    inline bool ReadU32(const Json::Value& obj, const char* key, uint32_t& out)
    {
        if (!obj.isObject() || !obj.isMember(key))
            return false;
        const Json::Value& v = obj[key];
        if (!v.isUInt())
            return false;
        out = v.asUInt();
        return true;
    }

    inline bool ReadKind(const Json::Value& body, Dia::DebugServer::SimKind& out)
    {
        if (!body.isMember("kind") || !body["kind"].isString())
            return false;
        const std::string kind = body["kind"].asString();
        if (kind == "query")        out = Dia::DebugServer::SimKind::Query;
        else if (kind == "command") out = Dia::DebugServer::SimKind::Command;
        else return false;
        return true;
    }

    // "0x" followed by 1..8 hex digits.
    inline bool ReadHash(const Json::Value& obj, uint32_t& out)
    {
        if (!obj.isMember("hash") || !obj["hash"].isString())
            return false;
        const std::string s = obj["hash"].asString();
        if (s.size() < 3 || s.size() > 10 || s[0] != '0' || (s[1] != 'x' && s[1] != 'X'))
            return false;
        uint64_t v = 0;
        for (size_t i = 2; i < s.size(); ++i)
        {
            const char c = s[i];
            uint32_t d;
            if (c >= '0' && c <= '9')      d = static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') d = static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') d = static_cast<uint32_t>(c - 'A' + 10);
            else return false;
            v = (v << 4) | d;
        }
        out = static_cast<uint32_t>(v);
        return true;
    }

    inline bool Finite(float f) { return f == f && f - f == 0.0f; }

    // Encodes 'info'.  False if the description is empty, paramCount is over the limit, a param has no name or a range is not finite.
    inline bool EncodeInfo(const Dia::DebugServer::SimHelpStored& info, Json::Value& out)
    {
        if (info.description[0] == '\0' || info.paramCount > Dia::DebugServer::kSimHelpMaxParams)
            return false;
        out = Json::Value(Json::objectValue);
        out["description"] = info.description;
        if (info.category[0] != '\0') out["category"] = info.category;
        if (info.owner[0] != '\0')    out["owner"]    = info.owner;
        if (info.noArgs)              out["noArgs"]   = true;
        if (info.paramCount > 0)
        {
            Json::Value params(Json::arrayValue);
            for (uint32_t i = 0; i < info.paramCount; ++i)
            {
                const Dia::DebugServer::SimParamStored& p = info.params[i];
                if (p.name[0] == '\0')
                    return false;
                Json::Value jp(Json::objectValue);
                jp["name"]     = p.name;
                jp["type"]     = p.type;
                jp["required"] = p.required;
                if (p.description[0] != '\0') jp["description"] = p.description;
                if (p.unit[0] != '\0')        jp["unit"]        = p.unit;
                if (p.hasRange)
                {
                    if (!Finite(p.min) || !Finite(p.max))
                        return false;
                    jp["min"] = static_cast<double>(p.min);
                    jp["max"] = static_cast<double>(p.max);
                }
                if (p.enumValues[0] != '\0')  jp["enum"]        = p.enumValues;
                params.append(jp);
            }
            out["params"] = params;
        }
        return true;
    }

    // String member: absent -> empty (invalid when 'required'); present -> must be a string of at most maxLen bytes
    // (non-empty when 'required').  dst may be null (validate only); otherwise cap is its size including the NUL.
    inline bool ReadText(const Json::Value& obj, const char* key, uint32_t maxLen, bool required, char* dst, size_t cap)
    {
        if (!obj.isMember(key))
        {
            if (required) return false;
            if (dst) dst[0] = '\0';
            return true;
        }
        const Json::Value& v = obj[key];
        if (!v.isString())
            return false;
        const std::string text = v.asString();
        if (text.size() > maxLen || text.size() >= cap || (required && text.empty()))
            return false;
        if (dst) std::memcpy(dst, text.c_str(), text.size() + 1);
        return true;
    }

    // Decodes and validates an info object.  'out' may be null (validate only).  Call it with null first and only
    // then with a buffer, so a Malformed message never leaves the caller's buffer half-filled.
    inline bool DecodeInfo(const Json::Value& info, Dia::DebugServer::SimHelpStored* out)
    {
        using namespace Dia::DebugServer;
        if (!info.isObject())
            return false;
        SimHelpStored* o = out;
        if (!ReadText(info, "description", kSimHelpMaxDescription, true,  o ? o->description : nullptr, sizeof(o->description)) ||
            !ReadText(info, "category",    kSimHelpMaxCategory,    false, o ? o->category : nullptr,    sizeof(o->category)) ||
            !ReadText(info, "owner",       kSimHelpMaxOwner,       false, o ? o->owner : nullptr,       sizeof(o->owner)))
            return false;

        bool noArgs = false;
        if (info.isMember("noArgs"))
        {
            if (!info["noArgs"].isBool()) return false;
            noArgs = info["noArgs"].asBool();
        }

        uint32_t count = 0;
        if (info.isMember("params"))
        {
            const Json::Value& params = info["params"];
            if (!params.isArray() || params.size() > kSimHelpMaxParams)
                return false;
            count = static_cast<uint32_t>(params.size());
            for (uint32_t i = 0; i < count; ++i)
            {
                const Json::Value& p = params[i];
                if (!p.isObject() || !p.isMember("type") || !p.isMember("required") || !p["required"].isBool())
                    return false;
                SimParamStored* d = o ? &o->params[i] : nullptr;
                if (!ReadText(p, "name",        kSimHelpMaxParamName,        true,  d ? d->name : nullptr,        sizeof(d->name)) ||
                    !ReadText(p, "type",        kSimHelpMaxParamType,        false, d ? d->type : nullptr,        sizeof(d->type)) ||
                    !ReadText(p, "description", kSimHelpMaxParamDescription, false, d ? d->description : nullptr, sizeof(d->description)) ||
                    !ReadText(p, "unit",        kSimHelpMaxParamUnit,        false, d ? d->unit : nullptr,        sizeof(d->unit)) ||
                    !ReadText(p, "enum",        kSimHelpMaxParamEnum,        false, d ? d->enumValues : nullptr,  sizeof(d->enumValues)))
                    return false;
                const bool hasMin = p.isMember("min"), hasMax = p.isMember("max");
                if (hasMin != hasMax || (hasMin && (!p["min"].isNumeric() || !p["max"].isNumeric())))
                    return false;
                if (d)
                {
                    d->required = p["required"].asBool();
                    d->hasRange = hasMin;
                    d->min      = hasMin ? static_cast<float>(p["min"].asDouble()) : 0.0f;
                    d->max      = hasMin ? static_cast<float>(p["max"].asDouble()) : 0.0f;
                }
            }
        }
        if (o)
        {
            o->paramCount = count;
            o->noArgs     = noArgs;
        }
        return true;
    }
}

// Fills 'out' as a register/unregister reply carrying 'seq'.  Returns false (out untouched) if the name is
// empty or too long, if op is not Register/Unregister, or (Register) if 'info' is null/invalid or the encoded
// message does not fit the event.  'info' is required for Register and ignored for Unregister.
inline bool BuildSimRegistrationReply(SimRegistrationOp op, const char* name,
                                      Dia::DebugServer::SimKind kind, uint32_t seq,
                                      const Dia::DebugServer::SimHelpStored* info,
                                      Dia::DebugServer::SimReplyEvent& out)
{
    if (!Detail::NameValid(name))
        return false;
    if (op != SimRegistrationOp::Register && op != SimRegistrationOp::Unregister)
        return false;

    Json::Value body(Json::objectValue);
    body["name"] = name;
    Json::Value root(Json::objectValue);
    if (op == SimRegistrationOp::Register)
    {
        body["kind"] = Detail::KindText(kind);
        if (info == nullptr || !Detail::EncodeInfo(*info, body["info"]))
            return false;
        root["register"] = body;
    }
    else
    {
        root["unregister"] = body;
    }
    root["seq"] = static_cast<Json::UInt>(seq);
    return Detail::Finish(root, out);
}

// {"digest":{"seq":..,"count":..,"hash":"0x%08x"}}
inline bool BuildSimDigestReply(uint32_t seq, uint32_t count, uint32_t hash, Dia::DebugServer::SimReplyEvent& out)
{
    char hex[16];
    std::snprintf(hex, sizeof(hex), "0x%08x", hash);
    Json::Value body(Json::objectValue);
    body["seq"]   = static_cast<Json::UInt>(seq);
    body["count"] = static_cast<Json::UInt>(count);
    body["hash"]  = hex;
    Json::Value root(Json::objectValue);
    root["digest"] = body;
    return Detail::Finish(root, out);
}

// {"resync_begin":{"seq":..,"count":..}}
inline bool BuildSimResyncBeginReply(uint32_t seq, uint32_t count, Dia::DebugServer::SimReplyEvent& out)
{
    Json::Value body(Json::objectValue);
    body["seq"]   = static_cast<Json::UInt>(seq);
    body["count"] = static_cast<Json::UInt>(count);
    Json::Value root(Json::objectValue);
    root["resync_begin"] = body;
    return Detail::Finish(root, out);
}

// {"register":{"name":..,"kind":..,"info":{..}},"replay":true}  (carries no seq; info is required)
inline bool BuildSimReplayReply(const char* name, Dia::DebugServer::SimKind kind,
                                const Dia::DebugServer::SimHelpStored* info,
                                Dia::DebugServer::SimReplyEvent& out)
{
    if (!Detail::NameValid(name) || info == nullptr)
        return false;
    Json::Value body(Json::objectValue);
    body["name"] = name;
    body["kind"] = Detail::KindText(kind);
    if (!Detail::EncodeInfo(*info, body["info"]))
        return false;
    Json::Value root(Json::objectValue);
    root["register"] = body;
    root["replay"]   = true;
    return Detail::Finish(root, out);
}

// Classifies a reply read from the SimQueryReply stream.  For Register / Replay the decoded help is written to
// *infoOut when it is non-null (left untouched otherwise, and on Malformed); the info is validated either way.
inline SimRegistration ParseSimRegistrationReply(const Dia::DebugServer::SimReplyEvent& reply,
                                                 Dia::DebugServer::SimHelpStored* infoOut = nullptr)
{
    SimRegistration result;
    if (reply.requestId != kSimRegistrationRequestId)
        return result;   // NotRegistration

    result.op = SimRegistrationOp::Malformed;

    const char* begin = reply.json;
    const size_t len  = strnlen(begin, Dia::DebugServer::kSimPayloadBytes);
    Json::Value root;
    Json::Reader reader;
    bool parsed = false;
    try { parsed = len != 0 && reader.parse(begin, begin + len, root); }
    catch (...) { parsed = false; }   // jsoncpp throws on pathological nesting (stackLimit): junk is Malformed, never an exception
    if (!parsed || !root.isObject())
        return result;

    const bool isRegister   = root.isMember("register");
    const bool isUnregister = root.isMember("unregister");
    const bool isDigest     = root.isMember("digest");
    const bool isBegin      = root.isMember("resync_begin");
    if ((isRegister ? 1 : 0) + (isUnregister ? 1 : 0) + (isDigest ? 1 : 0) + (isBegin ? 1 : 0) != 1)
        return result;   // none, or more than one

    if (isDigest)
    {
        SimRegistration d;
        const Json::Value& body = root["digest"];
        if (!Detail::ReadU32(body, "seq", d.seq) || !Detail::ReadU32(body, "count", d.count) ||
            !Detail::ReadHash(body, d.hash))
            return result;
        d.op = SimRegistrationOp::Digest;
        return d;
    }
    if (isBegin)
    {
        SimRegistration b;
        const Json::Value& body = root["resync_begin"];
        if (!Detail::ReadU32(body, "seq", b.seq) || !Detail::ReadU32(body, "count", b.count))
            return result;
        b.op = SimRegistrationOp::ResyncBegin;
        return b;
    }

    const Json::Value& body = root[isRegister ? "register" : "unregister"];
    if (!body.isObject() || !body.isMember("name") || !body["name"].isString())
        return result;

    const std::string name = body["name"].asString();
    if (name.empty() || name.size() >= kSimRegistrationMaxName)
        return result;

    bool replay = false;
    if (root.isMember("replay"))
    {
        if (!root["replay"].isBool() || !isRegister)
            return result;
        replay = root["replay"].asBool();
    }

    if (isRegister && !Detail::ReadKind(body, result.kind))
        return result;

    if (!replay && !Detail::ReadU32(root, "seq", result.seq))
        return result;   // hard cutover: non-replay register/unregister must carry seq

    if (isRegister)
    {
        // Validate first (no writes), then decode, so infoOut is never left half-filled by a Malformed message.
        if (!body.isMember("info") || !Detail::DecodeInfo(body["info"], nullptr))
            return result;
        if (infoOut != nullptr)
            Detail::DecodeInfo(body["info"], infoOut);
        result.hasInfo = true;
    }

    std::memcpy(result.name, name.c_str(), name.size() + 1);
    if (replay)
        result.op = SimRegistrationOp::Replay;
    else
        result.op = isRegister ? SimRegistrationOp::Register : SimRegistrationOp::Unregister;
    return result;
}

} } // namespace Cluiche::AppFlow
