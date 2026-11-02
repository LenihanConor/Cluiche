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
//   register     {"register":{"name":"sim.echo","kind":"query"},"seq":7}   kind: "query" | "command"
//   unregister   {"unregister":{"name":"sim.echo"},"seq":8}
//   digest       {"digest":{"seq":8,"count":12,"hash":"0x1a2b3c4d"}}
//   resync_begin {"resync_begin":{"seq":8,"count":12}}
//   replay       {"register":{"name":"sim.echo","kind":"query"},"replay":true}   (carries no seq)
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
};

// Per-entry hash of (name, kind): FNV-1a over the name bytes, then the kind value, then an avalanche.
// Deterministic; shared by the sim host and the DebugServer.  Set hash = uint32 wrapping sum of entry hashes.
inline uint32_t SimNameSetEntryHash(const char* name, Dia::DebugServer::SimKind kind)
{
    uint32_t h = 2166136261u;
    for (const char* p = (name != nullptr) ? name : ""; *p != '\0'; ++p)
    {
        h ^= static_cast<uint8_t>(*p);
        h *= 16777619u;
    }
    h ^= static_cast<uint32_t>(kind) + 1u;
    h *= 16777619u;
    h ^= h >> 16; h *= 0x85ebca6bu; h ^= h >> 13; h *= 0xc2b2ae35u; h ^= h >> 16;
    return h;
}

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
}

// Fills 'out' as a register/unregister reply carrying 'seq'.  Returns false (out untouched) if the name is
// empty or too long, or if op is not Register/Unregister.
inline bool BuildSimRegistrationReply(SimRegistrationOp op, const char* name,
                                      Dia::DebugServer::SimKind kind, uint32_t seq,
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

// {"register":{"name":..,"kind":..},"replay":true}  (carries no seq)
inline bool BuildSimReplayReply(const char* name, Dia::DebugServer::SimKind kind, Dia::DebugServer::SimReplyEvent& out)
{
    if (!Detail::NameValid(name))
        return false;
    Json::Value body(Json::objectValue);
    body["name"] = name;
    body["kind"] = Detail::KindText(kind);
    Json::Value root(Json::objectValue);
    root["register"] = body;
    root["replay"]   = true;
    return Detail::Finish(root, out);
}

// Classifies a reply read from the SimQueryReply stream.
inline SimRegistration ParseSimRegistrationReply(const Dia::DebugServer::SimReplyEvent& reply)
{
    SimRegistration result;
    if (reply.requestId != kSimRegistrationRequestId)
        return result;   // NotRegistration

    result.op = SimRegistrationOp::Malformed;

    const char* begin = reply.json;
    const size_t len  = strnlen(begin, Dia::DebugServer::kSimPayloadBytes);
    Json::Value root;
    Json::Reader reader;
    if (len == 0 || !reader.parse(begin, begin + len, root) || !root.isObject())
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

    std::memcpy(result.name, name.c_str(), name.size() + 1);
    if (replay)
        result.op = SimRegistrationOp::Replay;
    else
        result.op = isRegister ? SimRegistrationOp::Register : SimRegistrationOp::Unregister;
    return result;
}

} } // namespace Cluiche::AppFlow
