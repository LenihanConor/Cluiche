#include <gtest/gtest.h>
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/ServiceKeyResolver.h>
#include <DiaVisualDebugger/Domain/SchemaControl.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <DiaVisualDebugger/Console/PanelJson.h>
#include <DiaObservation/Testing/MockSink.h>
#include <DiaObservation/Log/Logger.h>

using namespace Dia::VisualDebugger;
using Dia::Core::StringCRC;

namespace
{
    struct FakeService
    {
        StringCRC    known;
        unsigned int version;
        FakeService(const char* key, unsigned int v) : known(key), version(v) {}
        const void*  Find(StringCRC key) const { return key == known ? this : nullptr; }
        unsigned int Version() const { return version; }
    };

    typedef ServiceKeyResolver<FakeService> FakeKeyResolver;

    unsigned int VscWarnings(const Dia::Observation::Log::MockSink& sink)
    {
        Dia::Observation::Log::Logger::Instance().FlushSync();
        unsigned int n = 0;
        for (unsigned int i = 0; i < sink.Count(); ++i)
        {
            if (sink.entries[i].entry.level == Dia::Observation::Log::LogLevel::kWarning &&
                sink.entries[i].entry.channel == StringCRC("VSC"))
            {
                ++n;
            }
        }
        return n;
    }
}

TEST(DiaVisualDebugger_ServiceKeyResolver, NoServiceIsUnavailableAndResolvesNothing)
{
    FakeKeyResolver r;
    EXPECT_FALSE(r.IsAvailable());
    EXPECT_FALSE(r.Resolves(StringCRC("a/b")));
    EXPECT_EQ(0u, r.Version());
}

TEST(DiaVisualDebugger_ServiceKeyResolver, ResolvesKnownAndUnknownKeys)
{
    FakeService svc("a/b", 5);
    FakeKeyResolver r(&svc);
    EXPECT_TRUE(r.IsAvailable());
    EXPECT_TRUE(r.Resolves(StringCRC("a/b")));
    EXPECT_FALSE(r.Resolves(StringCRC("a/c")));
}

TEST(DiaVisualDebugger_ServiceKeyResolver, ForwardsVersion)
{
    FakeService svc("a/b", 9);
    FakeKeyResolver r(&svc);
    EXPECT_EQ(9u, r.Version());
    svc.version = 10;
    EXPECT_EQ(10u, r.Version());
}

TEST(DiaVisualDebugger_ServiceKeyResolver, RepointingChangesResultsAndNullMakesUnavailable)
{
    FakeService first("a/b", 1);
    FakeService second("c/d", 2);
    FakeKeyResolver r(&first);
    EXPECT_TRUE(r.Resolves(StringCRC("a/b")));

    r.SetService(&second);
    EXPECT_FALSE(r.Resolves(StringCRC("a/b")));
    EXPECT_TRUE(r.Resolves(StringCRC("c/d")));
    EXPECT_EQ(2u, r.Version());

    r.SetService(nullptr);
    EXPECT_FALSE(r.IsAvailable());
    EXPECT_EQ(0u, r.Version());
}

TEST(DiaVisualDebugger_ServiceKeyResolver, SchemaControlWarnsOnceForUnresolvedKey)
{
    Dia::Observation::Log::MockSink sink;
    Dia::Observation::Log::Logger::Instance().RegisterThreadBuffer();
    Dia::Observation::Log::Logger::Instance().RegisterSink(&sink);

    FakeService svc("a/b", 3);
    FakeKeyResolver r(&svc);
    SchemaControl sc(&r);
    Json::Value controls(Json::arrayValue);
    sc.Add(controls, "ok", StringCRC("a/b"), 1.0f);
    sc.Add(controls, "bad", StringCRC("a/zzz"), 1.0f);
    sc.Add(controls, "bad again", StringCRC("a/zzz"), 2.0f);

    EXPECT_EQ(1u, sc.UnresolvedCount());
    EXPECT_EQ(1u, VscWarnings(sink));

    Dia::Observation::Log::Logger::Instance().UnregisterSink(&sink);
    Dia::Observation::Log::Logger::Instance().UnregisterThreadBuffer();
}

TEST(DiaVisualDebugger_ServiceKeyResolver, SchemaControlWritesServiceVersion)
{
    FakeService svc("a/b", 42);
    FakeKeyResolver r(&svc);
    SchemaControl sc(&r);
    Json::Value state(Json::objectValue);
    sc.AddSchemaVersion(state);
    ASSERT_TRUE(state.isMember("schemaVersion"));
    EXPECT_EQ(42u, state["schemaVersion"].asUInt());
}

TEST(DiaVisualDebugger_ServiceKeyResolver, SchemaControlBehavesAsNoResolverWhenServiceAbsent)
{
    Dia::Observation::Log::MockSink sink;
    Dia::Observation::Log::Logger::Instance().RegisterThreadBuffer();
    Dia::Observation::Log::Logger::Instance().RegisterSink(&sink);

    FakeKeyResolver r;   // no service
    SchemaControl sc(&r);
    EXPECT_FALSE(sc.HasResolver());
    Json::Value controls(Json::arrayValue);
    sc.Add(controls, "x", StringCRC("a/zzz"), 1.0f);
    Json::Value state(Json::objectValue);
    sc.AddSchemaVersion(state);

    EXPECT_EQ(1u, controls.size());
    EXPECT_EQ(0u, sc.UnresolvedCount());
    EXPECT_EQ(0u, VscWarnings(sink));
    EXPECT_FALSE(state.isMember("schemaVersion"));

    Dia::Observation::Log::Logger::Instance().UnregisterSink(&sink);
    Dia::Observation::Log::Logger::Instance().UnregisterThreadBuffer();
}

namespace
{
    class ControlsDomain : public IDebugDomain
    {
    public:
        StringCRC   GetDomainId() const override { return StringCRC("TestEco"); }
        const char* GetDisplayName() const override { return "Eco"; }
        const char* GetDescription() const override { return "Eco"; }
        StringCRC   GetGroup() const override { return StringCRC("Navigation"); }
        Dia::Core::RGBA GetAccentColour() const override { return Dia::Core::RGBA(0, 0, 0, 255); }
        void GetJSONState(Json::Value& out) override { out["controls"] = Json::Value(Json::arrayValue); }
        void OnCommand(StringCRC, const Json::Value&) override {}
    };
}

TEST(DiaVisualDebugger_ServiceKeyResolver, DomainStatesRootStampsSchemaVersionWhenResolverAvailable)
{
    // Page reads schemaVersion from a domain state (vsScan), so the host stamps it per domain.
    FakeService svc("a/b", 7);
    FakeKeyResolver r(&svc);
    DiaDebugDomainRegistry registry;
    ControlsDomain domain;
    registry.Register(domain);

    Json::Value withVer(Json::objectValue);
    Console::BuildDomainStatesRoot(registry, withVer, &r);
    EXPECT_EQ(7u, withVer["Eco"]["schemaVersion"].asUInt());

    Json::Value without(Json::objectValue);
    Console::BuildDomainStatesRoot(registry, without, nullptr);
    EXPECT_FALSE(without["Eco"].isMember("schemaVersion"));

    FakeKeyResolver absent;
    Json::Value unavailable(Json::objectValue);
    Console::BuildDomainStatesRoot(registry, unavailable, &absent);
    EXPECT_FALSE(unavailable["Eco"].isMember("schemaVersion"));
    registry.Clear();
}

#endif // DIA_DEBUG
