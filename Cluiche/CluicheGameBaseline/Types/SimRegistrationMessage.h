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
//   {"register":{"name":"sim.echo","kind":"query"}}      kind: "query" | "command"
//   {"unregister":{"name":"sim.echo"}}
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
    Malformed       = 3    // requestId 0 but the payload is not a valid registration
};

struct SimRegistration
{
    SimRegistrationOp               op   = SimRegistrationOp::NotRegistration;
    char                            name[kSimRegistrationMaxName] = {};
    Dia::DebugServer::SimKind       kind = Dia::DebugServer::SimKind::Query;
};

// Fills 'out' as a registration reply.  Returns false (out untouched) if the name is empty or too long,
// or if op is not Register/Unregister.
inline bool BuildSimRegistrationReply(SimRegistrationOp op, const char* name,
                                      Dia::DebugServer::SimKind kind,
                                      Dia::DebugServer::SimReplyEvent& out)
{
    if (name == nullptr || name[0] == '\0' || std::strlen(name) >= kSimRegistrationMaxName)
        return false;
    if (op != SimRegistrationOp::Register && op != SimRegistrationOp::Unregister)
        return false;

    Json::Value body(Json::objectValue);
    body["name"] = name;
    Json::Value root(Json::objectValue);
    if (op == SimRegistrationOp::Register)
    {
        body["kind"] = (kind == Dia::DebugServer::SimKind::Command) ? "command" : "query";
        root["register"] = body;
    }
    else
    {
        root["unregister"] = body;
    }

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

    const bool isRegister = root.isMember("register");
    const bool isUnregister = root.isMember("unregister");
    if (isRegister == isUnregister)
        return result;   // neither, or both

    const Json::Value& body = root[isRegister ? "register" : "unregister"];
    if (!body.isObject() || !body.isMember("name") || !body["name"].isString())
        return result;

    const std::string name = body["name"].asString();
    if (name.empty() || name.size() >= kSimRegistrationMaxName)
        return result;

    if (isRegister)
    {
        if (!body.isMember("kind") || !body["kind"].isString())
            return result;
        const std::string kind = body["kind"].asString();
        if (kind == "query")
            result.kind = Dia::DebugServer::SimKind::Query;
        else if (kind == "command")
            result.kind = Dia::DebugServer::SimKind::Command;
        else
            return result;
    }

    std::memcpy(result.name, name.c_str(), name.size() + 1);
    result.op = isRegister ? SimRegistrationOp::Register : SimRegistrationOp::Unregister;
    return result;
}

} } // namespace Cluiche::AppFlow
