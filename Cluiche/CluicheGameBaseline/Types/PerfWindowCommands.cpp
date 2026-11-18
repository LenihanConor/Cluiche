#include "Types/PerfWindowCommands.h"

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/PerfWindowPacker.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace Cluiche { namespace AppFlow {

using Dia::ApplicationFlow::Event;
using Dia::Core::StringCRC;
using namespace Dia::Observation::Profile;

namespace
{
    const StringCRC kWindowName("perf.window");
    const StringCRC kFrameName("perf.frame");
    const StringCRC kJobsName("perf.jobs");

    Json::Value Fail(const char* reason)
    {
        Json::Value v(Json::objectValue);
        v["error"] = reason;
        return v;
    }

    // crc -> display name, built per command call from the application's inspectable view.
    struct NameTable
    {
        std::vector<std::pair<uint32_t, std::string>> entries;

        const std::string* Find(uint32_t crc) const
        {
            for (const auto& e : entries)
                if (e.first == crc) return &e.second;
            return nullptr;
        }
        void Add(const StringCRC& id)
        {
            if (!Find(id.Value()))
                entries.emplace_back(id.Value(), std::string(id.AsChar()));
        }
    };

    // The packer takes a plain function pointer, so the table in use is passed via a pointer that is
    // only set for the duration of one pack call on the calling (Main) thread.
    thread_local const NameTable* tActiveNames = nullptr;

    struct ScopedNames
    {
        explicit ScopedNames(const NameTable* t) : previous(tActiveNames) { tActiveNames = t; }
        ~ScopedNames() { tActiveNames = previous; }
        const NameTable* previous;
    };

    const char* ResolveModuleName(uint32_t crc, char* buf, size_t bufSize)
    {
        const std::string* name = tActiveNames ? tActiveNames->Find(crc) : nullptr;
        if (!name || name->empty()) return nullptr;
        snprintf(buf, bufSize, "%s", name->c_str());
        return buf;
    }

    void BuildNames(Dia::ApplicationFlow::IApplicationInspectable* app, NameTable& units, NameTable& modules)
    {
        if (!app) return;
        Dia::Core::Containers::DynamicArrayC<StringCRC, 4> pus;
        app->GetProcessingUnits(pus);
        for (unsigned int p = 0; p < pus.Size(); ++p)
        {
            units.Add(pus[p]);
            auto active = std::make_unique<Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::ModuleStateInfo, 64>>();
            app->GetActiveModules(pus[p], *active);
            for (unsigned int m = 0; m < active->Size(); ++m)
                modules.Add((*active)[m].instanceId);
        }
    }

    void HexName(uint32_t crc, char (&buf)[16]) { snprintf(buf, sizeof(buf), "0x%08x", crc); }

    // Unit selector: display name, "0x%08x" crc, or decimal crc.
    template <typename Assembler>
    bool FindUnitCrc(const Assembler& assembler,const NameTable& units, const std::string& key, uint32_t& outCrc)
    {
        for (uint32_t i = 0; i < assembler.GetUnitCount(); ++i)
        {
            const uint32_t crc = assembler.GetUnitCrcAt(i);
            const std::string* name = units.Find(crc);
            char hex[16];
            HexName(crc, hex);
            if ((name && *name == key) || key == hex || key == std::to_string(crc))
            {
                outCrc = crc;
                return true;
            }
        }
        return false;
    }

    const char* UnitDisplayName(const NameTable& units, uint32_t crc, char (&hex)[16])
    {
        const std::string* name = units.Find(crc);
        if (name && !name->empty()) return name->c_str();
        HexName(crc, hex);
        return hex;
    }

    float UnitHzOrDefault(const CoarseChunkAssembler& assembler, uint32_t crc)
    {
        const float hz = assembler.GetUnitHz(crc);
        return hz > 0.0f ? hz : 60.0f;   // same fallback the assembler uses for the ring capacity
    }

    // True when the unit's newest chunk says per-job recording was on.
    bool JobUnitRecordingOn(const JobChunkAssembler& assembler, uint32_t crc)
    {
        const JobChunk* latest = assembler.GetLatest(crc);
        return latest && (latest->flags & JobChunk::kFlagPerJob) != 0;
    }

    // The packer returns JSON text.  The command registry replies with a Json::Value (it wraps it in
    // {"success":true,"data":...}), so there is no pre-serialized path: parse the text into the reply.
    // The text is a handful of big base64 strings, so parsing is cheap.
    Json::Value ParsePacked(const std::string& text)
    {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(text, root))
            return Fail("internal error: packed reply did not parse");
        return root;
    }

    const Dia::API::ValueParam kWindowParams[] = {
        { "seconds", "float", false, "Window length in seconds, default 60, clamped to 1..60", "s", true, 1.0f, 60.0f, nullptr },
    };
    const Dia::API::ValueParam kFrameParams[] = {
        { "unit",  "string",  true, "Processing unit name (as in perf.window units[].unit), or its crc as 0x%08x / decimal", nullptr, false, 0.0f, 0.0f, nullptr },
        { "frame", "int",     true, "Absolute coarse entry index of that unit (the window's firstEntry + i)", nullptr, false, 0.0f, 0.0f, nullptr },
    };
    const Dia::API::ValueParam kJobsParams[] = {
        { "unit",   "string", true, "Processing unit name (as in perf.window jobs.units[].unit), or its crc as 0x%08x / decimal", nullptr, false, 0.0f, 0.0f, nullptr },
        { "fromNs", "int",    true, "Range start in nanoseconds on the job clock (the records' startNs)", "ns", false, 0.0f, 0.0f, nullptr },
        { "toNs",   "int",    true, "Range end in nanoseconds (>= fromNs); jobs overlapping [fromNs, toNs] are returned", "ns", false, 0.0f, 0.0f, nullptr },
    };
}

PerfWindowCommands::PerfWindowCommands(Dia::ApplicationFlow::EventStreamStore<CoarseChunk>* store,
                                       Dia::ApplicationFlow::IApplicationInspectable* app,
                                       Dia::ApplicationFlow::EventStreamStore<JobChunk>* jobsStore)
    : mStore(store)
    , mApp(app)
    , mReader(-1)
    , mJobsStore(jobsStore)
    , mJobsReader(-1)
    , mRegistered(false)
{
    if (mJobsStore)
    {
        mJobsReader = mJobsStore->RegisterReader();
        if (mJobsReader < 0)
            DIA_LOG_WARNING("PerfWindow", "No free reader on $perfjobs; perf.jobs will have no data");
        else
            mJobsScratch = std::make_unique<JobDrainBuffer>();
    }
    if (mStore)
    {
        mReader = mStore->RegisterReader();
        if (mReader < 0)
            DIA_LOG_WARNING("PerfWindow", "No free reader on $perfcoarse; perf.window will have no data");
        else
            mScratch = std::make_unique<DrainBuffer>();
    }
}

PerfWindowCommands::~PerfWindowCommands()
{
    Unregister();
}

void PerfWindowCommands::Register()
{
    if (mRegistered) return;
    mRegistered = true;

    {
        Dia::API::CommandInfoJson c;
        c.name        = kWindowName;
        c.description = "Packed timing window (per unit: unit and per-module ms as base64 uint16 arrays, 0.01 ms steps) from the long coarse ring";
        c.category    = StringCRC("perf");
        c.owner       = "CluicheGameBaseline";
        c.params      = kWindowParams;
        c.paramCount  = 1;
        c.callback    = [this](const Json::Value& args) { return HandleWindow(args); };
        Dia::API::RegisterCommandJson(c);
    }
    {
        Dia::API::CommandInfoJson c;
        c.name        = kFrameName;
        c.description = "Breakdown of one coarse frame of a unit: unit and per-module ms; sections report whether available";
        c.category    = StringCRC("perf");
        c.owner       = "CluicheGameBaseline";
        c.params      = kFrameParams;
        c.paramCount  = 2;
        c.callback    = [this](const Json::Value& args) { return HandleFrame(args); };
        Dia::API::RegisterCommandJson(c);
    }
    {
        Dia::API::CommandInfoJson c;
        c.name        = kJobsName;
        c.description = "Raw job records of a unit overlapping [fromNs, toNs] as packed arrays (worker, start, duration, owner, tag, range), at most 2000, with a truncated flag";
        c.category    = StringCRC("perf");
        c.owner       = "CluicheGameBaseline";
        c.params      = kJobsParams;
        c.paramCount  = 3;
        c.callback    = [this](const Json::Value& args) { return HandleJobs(args); };
        Dia::API::RegisterCommandJson(c);
    }
    DIA_LOG_INFO("PerfWindow", "Registered perf.window, perf.frame and perf.jobs");
}

void PerfWindowCommands::Unregister()
{
    if (!mRegistered) return;
    mRegistered = false;
    Dia::API::UnregisterCommandJson(kWindowName);
    Dia::API::UnregisterCommandJson(kFrameName);
    Dia::API::UnregisterCommandJson(kJobsName);
    DIA_LOG_INFO("PerfWindow", "Unregistered perf.window, perf.frame and perf.jobs");
}

void PerfWindowCommands::Drain()
{
    if (mJobsStore && mJobsReader >= 0 && mJobsScratch)
    {
        for (int pass = 0; pass < 32; ++pass)
        {
            mJobsScratch->RemoveAll();
            mJobsStore->Consume(mJobsReader, *mJobsScratch);
            if (mJobsScratch->Size() == 0) break;
            for (unsigned int i = 0; i < mJobsScratch->Size(); ++i)
                mJobsAssembler.Apply((*mJobsScratch)[i].payload);
        }
    }
    if (!mStore || mReader < 0 || !mScratch) return;
    // Bounded: at most the stream capacity worth of chunks per tick; leftovers wait for the next tick.
    for (int pass = 0; pass < 32; ++pass)
    {
        mScratch->RemoveAll();
        mStore->Consume(mReader, *mScratch);
        if (mScratch->Size() == 0) return;
        for (unsigned int i = 0; i < mScratch->Size(); ++i)
            mAssembler.Apply((*mScratch)[i].payload);
    }
}

Json::Value PerfWindowCommands::HandleWindow(const Json::Value& args)
{
    float seconds = 60.0f;
    if (args.isObject() && args["seconds"].isNumeric())
        seconds = args["seconds"].asFloat();

    NameTable unitNames, moduleNames;
    BuildNames(mApp, unitNames, moduleNames);

    const uint32_t unitCount = mAssembler.GetUnitCount();
    std::vector<PerfWindowUnitInput> inputs(unitCount);
    std::vector<std::string> displayNames(unitCount);
    for (uint32_t i = 0; i < unitCount; ++i)
    {
        const uint32_t crc = mAssembler.GetUnitCrcAt(i);
        char hex[16];
        displayNames[i] = UnitDisplayName(unitNames, crc, hex);
        inputs[i].unitCrc = crc;
        inputs[i].name    = displayNames[i].c_str();
        inputs[i].unitHz  = UnitHzOrDefault(mAssembler, crc);
        inputs[i].ring    = mAssembler.GetRing(crc);
    }

    std::string text;
    {
        ScopedNames scope(&moduleNames);
        PackPerfWindow(inputs.data(), unitCount, seconds, &ResolveModuleName, text);
    }
    DIA_LOG_DEBUG("PerfWindow", "perf.window packed %u unit(s), %u bytes", unitCount, static_cast<unsigned>(text.size()));
    Json::Value reply = ParsePacked(text);
    if (reply.isMember("error"))
        return reply;

    // Tier 2 jobs section (an added key on the v1 reply; bucketed aggregates + the units' deferred-step summaries in "steps").
    const uint32_t jobUnitCount = mJobsAssembler.GetUnitCount();
    std::vector<std::vector<JobFrameAggregate>> aggStore(jobUnitCount);
    std::vector<std::vector<StepSummary>> stepStore(jobUnitCount);
    std::vector<std::string> jobNames(jobUnitCount);
    std::vector<PerfJobsUnitInput> jobInputs(jobUnitCount);
    for (uint32_t i = 0; i < jobUnitCount; ++i)
    {
        const uint32_t crc = mJobsAssembler.GetUnitCrcAt(i);
        char hex[16];
        jobNames[i] = UnitDisplayName(unitNames, crc, hex);
        const bool on = JobUnitRecordingOn(mJobsAssembler, crc);
        if (on)
        {
            aggStore[i].resize(mJobsAssembler.GetAggCount(crc));
            aggStore[i].resize(mJobsAssembler.CopyAggregates(crc, aggStore[i].data(), static_cast<uint32_t>(aggStore[i].size())));
        }
        stepStore[i].resize(mJobsAssembler.GetStepCount(crc));
        stepStore[i].resize(mJobsAssembler.CopySteps(crc, stepStore[i].data(), static_cast<uint32_t>(stepStore[i].size())));
        jobInputs[i] = { crc, jobNames[i].c_str(), UnitHzOrDefault(mAssembler, crc), on,
                         aggStore[i].data(), static_cast<uint32_t>(aggStore[i].size()),
                         stepStore[i].data(), static_cast<uint32_t>(stepStore[i].size()) };
    }
    std::string jobsText;
    {
        ScopedNames scope(&moduleNames);
        PackPerfWindowJobs(jobInputs.data(), jobUnitCount, seconds, &ResolveModuleName, jobsText);
    }
    DIA_LOG_DEBUG("PerfWindow", "perf.window jobs section %u unit(s), %u bytes", jobUnitCount, static_cast<unsigned>(jobsText.size()));
    Json::Value jobs = ParsePacked(jobsText);
    if (!jobs.isMember("error"))
        reply["jobs"] = jobs;
    return reply;
}

Json::Value PerfWindowCommands::HandleJobs(const Json::Value& args)
{
    if (!args.isObject() || !args["unit"].isString() || args["unit"].asString().empty())
        return Fail("missing or empty 'unit'");
    if (!args["fromNs"].isInt64() || args["fromNs"].asInt64() < 0)
        return Fail("missing or invalid 'fromNs' (expected non-negative integer)");
    if (!args["toNs"].isInt64() || args["toNs"].asInt64() < 0)
        return Fail("missing or invalid 'toNs' (expected non-negative integer)");
    const uint64_t fromNs = static_cast<uint64_t>(args["fromNs"].asInt64());
    const uint64_t toNs   = static_cast<uint64_t>(args["toNs"].asInt64());
    if (fromNs > toNs)
        return Fail("'fromNs' is after 'toNs'");

    NameTable unitNames, moduleNames;
    BuildNames(mApp, unitNames, moduleNames);

    uint32_t crc = 0;
    if (!FindUnitCrc(mJobsAssembler, unitNames, args["unit"].asString(), crc))
        return Fail("unknown unit");
    char hex[16];
    const std::string name = UnitDisplayName(unitNames, crc, hex);

    std::string text;
    if (!JobUnitRecordingOn(mJobsAssembler, crc))
    {
        // Kill switch (AC14): per-job records are not produced for this unit; do not replay stale ones.
        PackPerfJobsRecords(name.c_str(), crc, fromNs, toNs, nullptr, 0, false, false, text);
        return ParsePacked(text);
    }

    std::vector<JobRecordPacked> records(kMaxJobRecords);
    bool truncated = false;
    const uint32_t n = mJobsAssembler.CopyRaw(crc, fromNs, toNs, records.data(), kMaxJobRecords, &truncated);
    PackPerfJobsRecords(name.c_str(), crc, fromNs, toNs, records.data(), n, truncated, true, text);
    DIA_LOG_DEBUG("PerfWindow", "perf.jobs packed %u record(s)%s, %u bytes", n, truncated ? " (truncated)" : "", static_cast<unsigned>(text.size()));
    return ParsePacked(text);
}

Json::Value PerfWindowCommands::HandleFrame(const Json::Value& args)
{
    if (!args.isObject() || !args["unit"].isString() || args["unit"].asString().empty())
        return Fail("missing or empty 'unit'");
    if (!args["frame"].isInt64() || args["frame"].asInt64() < 0)
        return Fail("missing or invalid 'frame' (expected non-negative integer)");

    NameTable unitNames, moduleNames;
    BuildNames(mApp, unitNames, moduleNames);

    uint32_t crc = 0;
    if (!FindUnitCrc(mAssembler, unitNames, args["unit"].asString(), crc))
        return Fail("unknown unit");

    char hex[16];
    const std::string name = UnitDisplayName(unitNames, crc, hex);
    PerfWindowUnitInput input;
    input.unitCrc = crc;
    input.name    = name.c_str();
    input.unitHz  = UnitHzOrDefault(mAssembler, crc);
    input.ring    = mAssembler.GetRing(crc);

    std::string text;
    {
        ScopedNames scope(&moduleNames);
        PackPerfFrame(input, static_cast<uint64_t>(args["frame"].asInt64()), &ResolveModuleName, text);
    }
    return ParsePacked(text);
}

}} // namespace Cluiche::AppFlow
