////////////////////////////////////////////////////////////////////////////////
// Filename: TestModuleProfileSubject.cpp
// GoogleTest suite — Profiling A1b: module.update / pu.update scopes carry the
// owning instance id as their profile subject (spec AA1).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaObservation/Profile/Profiler.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Testing/ProfileFixture.h>
#include <DiaCore/CRC/StringCRC.h>

#include <chrono>
#include <fstream>
#include <string>
#include <vector>
#include <thread>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using Dia::Observation::Profile::Profiler;

namespace {

    struct SubjectProbeModule : MainModule
    {
        using MainModule::MainModule;
        static const StringCRC kTypeId;
        StartResult DoStart() override { return StartResult::kReady; }
        StopResult  DoStop()  override { return StopResult::kDone; }
        void DoUpdate(const Dia::SimTime::MainTimeContext&) override
        {
            DIA_PROFILE_SCOPE("subject_probe_inner", Dia::Observation::Profile::Category::kDiaApplicationFlow);
        }
    };
    const StringCRC SubjectProbeModule::kTypeId("SubjectProbeModule");

    ApplicationManifestV3 BuildTwoModuleManifest()
    {
        ApplicationManifestV3 manifest;
        manifest.version = 3;

        StageDeclaration stage;
        stage.name = StringCRC("Boot");
        manifest.stages.Add(stage);
        manifest.initialStage = StringCRC("Boot");

        ProcessingUnitDeclaration pu;
        pu.instanceId      = StringCRC("MainPU");
        pu.frequencyHz     = 60.0f;
        pu.dedicatedThread = false;

        const char* ids[2] = { "probeModA", "probeModB" };
        for (const char* id : ids)
        {
            ModuleDeclaration mod;
            mod.instanceId     = StringCRC(id);
            mod.typeId         = SubjectProbeModule::kTypeId;
            mod.startTimeoutMs = 10000.0f;
            mod.stopTimeoutMs  = 5000.0f;
            mod.stages.Add(StringCRC("Boot"));
            pu.modules.Add(mod);
        }
        manifest.processingUnits.Add(pu);
        return manifest;
    }

    std::string ReadFile(const char* path)
    {
        std::ifstream f(path);
        return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }

    // All jsonl lines whose scope_name equals the CRC of `name`.
    std::vector<std::string> LinesFor(const std::string& content, const char* name)
    {
        std::vector<std::string> out;
        const std::string key = "\"scope_name\":" + std::to_string(StringCRC(name).Value()) + ",";
        size_t pos = 0;
        while (pos < content.size())
        {
            size_t end = content.find('\n', pos);
            if (end == std::string::npos) end = content.size();
            std::string line = content.substr(pos, end - pos);
            if (line.find(key) != std::string::npos)
                out.push_back(line);
            pos = end + 1;
        }
        return out;
    }

    std::string SubjectOf(const std::string& line)
    {
        const std::string key = "\"subject\":\"";
        size_t p = line.find(key);
        if (p == std::string::npos) return {};
        p += key.size();
        return line.substr(p, line.find('"', p) - p);
    }
}

TEST(ModuleProfileSubject, AA1_ModuleUpdateSubjectIsInstanceId)
{
    Dia::Observation::Testing::ProfileFixture fixture;

    TypeRegistry reg;
    reg.Register(SubjectProbeModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new SubjectProbeModule(id); });

    ApplicationManifestV3 manifest = BuildTwoModuleManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    for (int i = 0; i < 10; ++i)
        app.Update(1.0f / 60.0f);

    Profiler::Instance().BeginFrame();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Profiler::Instance().Stop();

    const std::string content = ReadFile(fixture.mJsonlPath);

    // Expected text is whatever the table resolves (name, or hex if the process-wide
    // table was exhausted by an earlier test).
    char bufA[64], bufB[64], bufP[64];
    const std::string expA = Profiler::ResolveSubjectName(StringCRC("probeModA").Value(), bufA, sizeof(bufA));
    const std::string expB = Profiler::ResolveSubjectName(StringCRC("probeModB").Value(), bufB, sizeof(bufB));
    const std::string expPu = Profiler::ResolveSubjectName(StringCRC("MainPU").Value(), bufP, sizeof(bufP));
    EXPECT_NE(expA, expB);

    bool sawA = false, sawB = false, sawInnerA = false, sawInnerB = false, sawPu = false;
    for (const std::string& l : LinesFor(content, "module.update"))
    {
        const std::string s = SubjectOf(l);
        if (s == expA) sawA = true;
        if (s == expB) sawB = true;
    }
    for (const std::string& l : LinesFor(content, "subject_probe_inner"))
    {
        const std::string s = SubjectOf(l);
        if (s == expA) sawInnerA = true;
        if (s == expB) sawInnerB = true;
    }
    for (const std::string& l : LinesFor(content, "pu.update"))
        if (SubjectOf(l) == expPu) sawPu = true;

    EXPECT_TRUE(sawA)      << "module.update for probeModA must carry subject probeModA";
    EXPECT_TRUE(sawB)      << "module.update for probeModB must carry subject probeModB";
    EXPECT_TRUE(sawInnerA) << "inner scope must inherit probeModA";
    EXPECT_TRUE(sawInnerB) << "inner scope must inherit probeModB";
    EXPECT_TRUE(sawPu)     << "pu.update must carry the PU instance id";
}
