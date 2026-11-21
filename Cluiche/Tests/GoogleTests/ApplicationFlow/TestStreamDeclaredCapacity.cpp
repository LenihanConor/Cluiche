////////////////////////////////////////////////////////////////////////////////
// Filename: TestStreamDeclaredCapacity.cpp
// GoogleTest suite - manifest-declared EventStream capacity / overflow policy
//
// Covers:
//   - A stream declared with capacity N holds exactly N events per reader ring
//     (drop-oldest drops the oldest on N+1), through the real Application path
//   - Undeclared capacity (0) keeps EventStreamStore<T>::kDefaultCapacity
//   - Declared overflow policy reaches the store
//   - A connector that declares nothing leaves defaults untouched
//
// Module types prefixed "Cap_" to avoid ODR collisions with other test files.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaStreams/EventStreamStore.h>
#include <DiaStreams/EventStreamWriter.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaStreams/IStreamConnector.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <vector>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

namespace
{
    struct Cap_Module : SimModule
    {
        using SimModule::SimModule;
        static const StringCRC kTypeId;

        EventStreamWriter<int> mDeclWriter{this, StringCRC("Cap_Declared")};
        EventStreamReader<int> mDeclReader{this, StringCRC("Cap_Declared")};
        EventStreamWriter<int> mDefWriter{this, StringCRC("Cap_Default")};
        EventStreamReader<int> mDefReader{this, StringCRC("Cap_Default")};

        void OnConnectStreams(Application& app) override
        {
            mDeclWriter.Connect(app);
            mDeclReader.Connect(app);
            mDefWriter.Connect(app);
            mDefReader.Connect(app);
        }
        StartResult DoStart() override { return StartResult::kReady; }
        void DoUpdate(const Dia::SimTime::SimTimeContext&) override {}
        StopResult DoStop() override { return StopResult::kDone; }
    };
    const StringCRC Cap_Module::kTypeId("Cap_Module");

    Cap_Module* gCapModule = nullptr;

    Module* CreateCapModule(const StringCRC& id)
    {
        auto* m = new Cap_Module(id);
        gCapModule = m;
        return m;
    }

    ApplicationManifestV3 MakeManifest(unsigned int declaredCapacity, OverflowPolicy policy)
    {
        ApplicationManifestV3 manifest;
        manifest.version = 2;

        StageDeclaration boot;
        boot.name = StringCRC("Boot");
        manifest.stages.Add(boot);
        manifest.initialStage = StringCRC("Boot");

        StreamDeclaration declared;
        declared.id             = StringCRC("Cap_Declared");
        declared.kind           = StringCRC("EventStream");
        declared.capacity       = declaredCapacity;
        declared.overflowPolicy = policy;
        manifest.streams.Add(declared);

        StreamDeclaration undeclared;
        undeclared.id   = StringCRC("Cap_Default");
        undeclared.kind = StringCRC("EventStream");
        manifest.streams.Add(undeclared);

        ProcessingUnitDeclaration pu;
        pu.instanceId      = StringCRC("MainPU");
        pu.frequencyHz     = 60.0f;
        pu.dedicatedThread = false;

        ModuleDeclaration mod;
        mod.instanceId     = StringCRC("capModule");
        mod.typeId         = Cap_Module::kTypeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(StringCRC("Boot"));
        pu.modules.Add(mod);

        manifest.processingUnits.Add(pu);
        return manifest;
    }

    // Connector that declares nothing: stores must keep defaults.
    class Cap_NoDeclConnector : public IStreamConnector
    {
    public:
        IStreamStore* RegisterOrFindStreamStore(Dia::Core::UniquePtr<IStreamStore> newStore) override
        {
            for (auto& existing : mStores)
            {
                if (existing->GetId() == newStore->GetId())
                    return existing.Get();
            }
            IStreamStore* raw = newStore.Get();
            mStores.push_back(std::move(newStore));
            return raw;
        }
    private:
        std::vector<Dia::Core::UniquePtr<IStreamStore>> mStores;
    };
}

TEST(StreamDeclaredCapacity, DeclaredCapacityHoldsExactlyNAndDropsOldest)
{
    gCapModule = nullptr;
    TypeRegistry reg;
    reg.Register(Cap_Module::kTypeId, CreateCapModule);

    ApplicationManifestV3 manifest = MakeManifest(4u, OverflowPolicy::kDropOldest);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_NE(gCapModule, nullptr);
    ASSERT_TRUE(gCapModule->mDeclReader.IsConnected());

    for (int i = 0; i < 5; ++i)
        (void)gCapModule->mDeclWriter.Send(i);

    DynamicArrayC<Event<int>, 512> out;
    gCapModule->mDeclReader.Consume(out);

    ASSERT_EQ(out.Size(), 4u) << "declared capacity 4 must hold exactly 4 events";
    EXPECT_EQ(out[0].payload, 1) << "oldest (0) must be dropped on the N+1th send";
    EXPECT_EQ(out[3].payload, 4);
}

TEST(StreamDeclaredCapacity, UndeclaredCapacityKeepsDefault256)
{
    gCapModule = nullptr;
    TypeRegistry reg;
    reg.Register(Cap_Module::kTypeId, CreateCapModule);

    ApplicationManifestV3 manifest = MakeManifest(4u, OverflowPolicy::kDropOldest);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_NE(gCapModule, nullptr);

    const int total = static_cast<int>(EventStreamStore<int>::kDefaultCapacity) + 10;
    for (int i = 0; i < total; ++i)
        (void)gCapModule->mDefWriter.Send(i);

    DynamicArrayC<Event<int>, 512> out;
    gCapModule->mDefReader.Consume(out);

    ASSERT_EQ(out.Size(), EventStreamStore<int>::kDefaultCapacity);
    EXPECT_EQ(out[0].payload, 10) << "oldest 10 events dropped, default drop-oldest";
}

TEST(StreamDeclaredCapacity, DeclaredOverflowPolicyReachesStore)
{
    gCapModule = nullptr;
    TypeRegistry reg;
    reg.Register(Cap_Module::kTypeId, CreateCapModule);

    ApplicationManifestV3 manifest = MakeManifest(2u, OverflowPolicy::kDropNewest);
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_NE(gCapModule, nullptr);

    IStreamStore* store = app.FindStreamStore(StringCRC("Cap_Declared"));
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(store->GetOverflowPolicy(), OverflowPolicy::kDropNewest);

    for (int i = 0; i < 3; ++i)
        (void)gCapModule->mDeclWriter.Send(i);

    DynamicArrayC<Event<int>, 16> out;
    gCapModule->mDeclReader.Consume(out);
    ASSERT_EQ(out.Size(), 2u);
    EXPECT_EQ(out[0].payload, 0) << "drop-newest keeps the existing events";
    EXPECT_EQ(out[1].payload, 1);
}

TEST(StreamDeclaredCapacity, ConnectorWithoutDeclarationsKeepsDefaults)
{
    Cap_NoDeclConnector connector;
    EventStreamWriter<int> writer(nullptr, StringCRC("Cap_NoDecl"));
    EventStreamReader<int> reader(nullptr, StringCRC("Cap_NoDecl"));
    writer.Connect(connector);
    reader.Connect(connector);
    ASSERT_TRUE(reader.IsConnected());

    const int total = static_cast<int>(EventStreamStore<int>::kDefaultCapacity) + 1;
    for (int i = 0; i < total; ++i)
        (void)writer.Send(i);

    DynamicArrayC<Event<int>, 512> out;
    reader.Consume(out);
    EXPECT_EQ(out.Size(), EventStreamStore<int>::kDefaultCapacity);
}
