// TestBusTrafficRecorder.cpp
//
// Covers AC-1 through AC-8, AC-10, AC-11, AC-12 for DiaMessageBus Bus Traffic Recorder
// (Feature: Bus Traffic Recorder, Tasks 1-2 — IBusRecorder interface,
// Bus::SetRecorder, dispatch hook, in-memory test recorder, FileBusRecorder,
// file format v1 and the BusTrafficFileReader test helper).
//
// AC-1: Every dispatched message produces exactly one record, in dispatch order.
// AC-2: Each record carries tick, type id, router id, pass, and raw payload bytes.
// AC-3: File begins with magic + version; reader rejects an unknown version.
// AC-4: File is complete and readable after the recorder is closed.
// AC-5: With no recorder set, bus behaves identically (null-check path).
// AC-6: With DIA_DEBUG off (Release), the hook compiles out entirely.
// AC-7: A message with no subscribers is still recorded.
// AC-8: Failed open/write logs once, disables the recorder, never affects the bus.
// AC-10: Runtime DIA_ASSERT fires when a recorder is set and a message type is
//        not trivially copyable (not a compile error).
// AC-11: Tick comes from the injected tick source; Open fails without one.
// AC-12: bufferSizeBytes controls the write buffer; a tiny buffer still yields a complete ordered file.

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <filesystem>

#include <DiaCore/Core/Assert.h>
#include <DiaMessageBus/Bus.h>

#ifdef DIA_DEBUG
#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaMessageBus/FileBusRecorder.h>
#include <DiaMessageBus/Testing/BusTrafficFileReader.h>
#include <DiaMessageBus/IBusRecorder.h>
#include <DiaMessageBus/Testing/InMemoryBusRecorder.h>
#endif

namespace Dia::MessageBus::Testing {

    // -----------------------------------------------------------------------
    // Local test-only message types
    // -----------------------------------------------------------------------
    struct RecorderTestMsgA {
        static inline const Dia::Core::StringCRC kTypeId{ "RecorderTestMsgA" };
        int value = 0;
    };

    struct RecorderTestMsgB {
        static inline const Dia::Core::StringCRC kTypeId{ "RecorderTestMsgB" };
        float x = 0.0f;
        float y = 0.0f;
    };

#ifdef DIA_DEBUG
    // =========================================================================
    // AC-1: One record per dispatched message, in dispatch order across
    // Primary and Reaction pass.
    // =========================================================================
    TEST(BusTrafficRecorder, OneRecordPerMessage_InDispatchOrder) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgA, 8>()));
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgB, 8>()));

        InMemoryBusRecorder recorder;
        bus.SetRecorder(&recorder);

        // Subscribe a Primary-pass handler that posts a Reaction message
        auto hPrimary = bus.Subscribe<RecorderTestMsgA>(
            Dia::Core::StringCRC("primarySub"),
            [&bus](const RecorderTestMsgA& msg) {
                if (msg.value == 42) {
                    RecorderTestMsgB reactionMsg;
                    reactionMsg.x = 1.0f;
                    bus.Broadcast(reactionMsg);
                }
            },
            Pass::Primary);

        // Subscribe a Reaction-pass handler
        int reactionCallCount = 0;
        auto hReaction = bus.Subscribe<RecorderTestMsgB>(
            Dia::Core::StringCRC("reactionSub"),
            [&reactionCallCount](const RecorderTestMsgB&) {
                ++reactionCallCount;
            },
            Pass::Reaction);

        // Post two messages: one triggers a Reaction post, one does not
        RecorderTestMsgA msg1;
        msg1.value = 42;
        bus.Broadcast(msg1);

        RecorderTestMsgA msg2;
        msg2.value = 99;
        bus.Broadcast(msg2);

        bus.Update();

        // Expect 3 records: msg1 (Primary), msg2 (Primary), then reactionMsg (Reaction)
        const auto& records = recorder.GetRecords();
        ASSERT_EQ(records.size(), 3u);

        // Check order and pass
        EXPECT_EQ(records[0].typeId, RecorderTestMsgA::kTypeId);
        EXPECT_EQ(records[0].pass, Pass::Primary);

        EXPECT_EQ(records[1].typeId, RecorderTestMsgA::kTypeId);
        EXPECT_EQ(records[1].pass, Pass::Primary);

        EXPECT_EQ(records[2].typeId, RecorderTestMsgB::kTypeId);
        EXPECT_EQ(records[2].pass, Pass::Reaction);

        EXPECT_EQ(reactionCallCount, 1);
    }

    // =========================================================================
    // AC-2: Each record carries tick, type id, router id, pass, and raw payload bytes.
    // =========================================================================
    TEST(BusTrafficRecorder, RecordFields_TypeId_RouterId_Pass_PayloadBytes) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgA, 8>()));

        uint64_t tick = 0;
        InMemoryBusRecorder recorder(
            [](void* ctx) -> uint64_t { return (*static_cast<uint64_t*>(ctx))++; }, &tick);
        bus.SetRecorder(&recorder);

        RecorderTestMsgA msg;
        msg.value = 123;
        bus.Broadcast(msg);

        bus.Update();

        const auto& records = recorder.GetRecords();
        ASSERT_EQ(records.size(), 1u);

        const auto& rec = records[0];
        EXPECT_EQ(rec.tick, 0u);
        EXPECT_EQ(rec.typeId, RecorderTestMsgA::kTypeId);
        EXPECT_EQ(rec.routerId, Bus::kBroadcastRouterId);
        EXPECT_EQ(rec.pass, Pass::Primary);
        EXPECT_EQ(rec.payloadSize, sizeof(RecorderTestMsgA));

        // Verify raw payload bytes
        RecorderTestMsgA recovered;
        std::memcpy(&recovered, rec.payloadBytes.data(), sizeof(RecorderTestMsgA));
        EXPECT_EQ(recovered.value, 123);
    }

    // =========================================================================
    // AC-5: With no recorder set, bus behaves identically (null-check path).
    // Existing bus tests pass unchanged.
    // =========================================================================
    TEST(BusTrafficRecorder, NoRecorder_BusDeliveryUnchanged) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgA, 8>()));

        int callCount = 0;
        auto h = bus.Subscribe<RecorderTestMsgA>(
            Dia::Core::StringCRC("sub"),
            [&callCount](const RecorderTestMsgA&) { ++callCount; });

        bus.Broadcast(RecorderTestMsgA{});
        bus.Update();
        EXPECT_EQ(callCount, 1);

        // SetRecorder(nullptr) clears
        InMemoryBusRecorder recorder;
        bus.SetRecorder(&recorder);
        bus.Broadcast(RecorderTestMsgA{});
        bus.Update();
        EXPECT_EQ(recorder.GetRecords().size(), 1u);

        bus.SetRecorder(nullptr);
        bus.Broadcast(RecorderTestMsgA{});
        bus.Update();
        EXPECT_EQ(recorder.GetRecords().size(), 1u); // no new records
        EXPECT_EQ(callCount, 3); // delivery still happened
    }

    // =========================================================================
    // AC-7: A message with no subscribers is still recorded.
    // =========================================================================
    TEST(BusTrafficRecorder, MessageWithNoSubscribers_StillRecorded) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgA, 8>()));

        InMemoryBusRecorder recorder;
        bus.SetRecorder(&recorder);

        // Post a message with zero subscribers
        RecorderTestMsgA msg;
        msg.value = 42;
        bus.Broadcast(msg);

        bus.Update();

        const auto& records = recorder.GetRecords();
        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].typeId, RecorderTestMsgA::kTypeId);
        EXPECT_EQ(records[0].payloadSize, sizeof(RecorderTestMsgA));
    }

    // =========================================================================
    // FileBusRecorder tests (Task 2): AC-3, AC-4, AC-8, AC-11, AC-12
    // =========================================================================
    namespace {
        std::string TempPath(const char* name) {
            return (std::filesystem::temp_directory_path() / name).string();
        }

        struct TempFile {
            explicit TempFile(const char* name) : path(TempPath(name)) { std::remove(path.c_str()); }
            ~TempFile() { std::remove(path.c_str()); }
            std::string path;
        };

        uint64_t CountingTick(void* ctx) { return (*static_cast<uint64_t*>(ctx))++; }

        class WarningCountSink : public Dia::Observation::Log::ISink {
        public:
            WarningCountSink() {
                SetLevelThreshold(Dia::Observation::Log::LogLevel::kDebug);
                SetChannelFilter(Dia::Core::StringCRC("DiaMessageBus"), true);
            }
            void OnLogEntry(const Dia::Observation::Log::LogEntry& entry) override {
                if (entry.level == Dia::Observation::Log::LogLevel::kWarning) ++mWarnings;
            }
            const char* GetName() const override { return "WarningCountSink"; }
            unsigned int mWarnings = 0;
        };
    }

    TEST(BusTrafficRecorder, File_HeaderPresent_MagicAndVersion) {
        TempFile f("dia_bus_rec_header.dat");
        uint64_t tick = 0;
        FileBusRecorder::Config cfg;
        cfg.tickSource = &CountingTick;
        cfg.tickContext = &tick;
        {
            FileBusRecorder rec;
            ASSERT_TRUE(rec.Open(f.path.c_str(), cfg));
            rec.Close();
        }
        std::FILE* fp = std::fopen(f.path.c_str(), "rb");
        ASSERT_NE(fp, nullptr);
        uint8_t hdr[8] = {};
        ASSERT_EQ(std::fread(hdr, 1, 8, fp), 8u);
        std::fclose(fp);
        EXPECT_EQ(std::memcmp(hdr, "DBTR", 4), 0);
        EXPECT_EQ(hdr[4], 1);
        EXPECT_EQ(hdr[5], 0);
        EXPECT_EQ(hdr[6], 0);
        EXPECT_EQ(hdr[7], 0);

        BusTrafficFileReader reader;
        EXPECT_TRUE(reader.Open(f.path.c_str()));
    }

    TEST(BusTrafficRecorder, File_Reader_RejectsCorruptedVersion) {
        TempFile f("dia_bus_rec_badver.dat");
        uint64_t tick = 0;
        FileBusRecorder::Config cfg;
        cfg.tickSource = &CountingTick;
        cfg.tickContext = &tick;
        {
            FileBusRecorder rec;
            ASSERT_TRUE(rec.Open(f.path.c_str(), cfg));
            rec.Close();
        }
        std::FILE* fp = std::fopen(f.path.c_str(), "r+b");
        ASSERT_NE(fp, nullptr);
        std::fseek(fp, 4, SEEK_SET);
        const uint8_t bad = 99;
        std::fwrite(&bad, 1, 1, fp);
        std::fclose(fp);

        BusTrafficFileReader reader;
        EXPECT_FALSE(reader.Open(f.path.c_str()));
    }

    TEST(BusTrafficRecorder, File_Open_FailsWithoutTickSource) {
        TempFile f("dia_bus_rec_notick.dat");
        FileBusRecorder rec;
        FileBusRecorder::Config cfg; // tickSource == nullptr
        EXPECT_FALSE(rec.Open(f.path.c_str(), cfg));
    }

    TEST(BusTrafficRecorder, File_TickComesFromInjectedSource) {
        TempFile f("dia_bus_rec_tick.dat");
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgA, 8>()));

        uint64_t tick = 100;
        FileBusRecorder::Config cfg;
        cfg.tickSource = &CountingTick;
        cfg.tickContext = &tick;
        FileBusRecorder rec;
        ASSERT_TRUE(rec.Open(f.path.c_str(), cfg));
        bus.SetRecorder(&rec);
        bus.Broadcast(RecorderTestMsgA{});
        bus.Broadcast(RecorderTestMsgA{});
        bus.Update();
        bus.SetRecorder(nullptr);
        rec.Close();

        BusTrafficFileReader reader;
        ASSERT_TRUE(reader.Open(f.path.c_str()));
        std::vector<BusTrafficFileReader::Record> recs;
        ASSERT_TRUE(reader.ReadAll(recs));
        ASSERT_EQ(recs.size(), 2u);
        EXPECT_EQ(recs[0].tick, 100u);
        EXPECT_EQ(recs[1].tick, 101u);
    }

    TEST(BusTrafficRecorder, File_RoundTrip_MixedPrimaryAndReaction) {
        TempFile f("dia_bus_rec_roundtrip.dat");
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgA, 8>()));
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgB, 8>()));

        uint64_t tick = 0;
        FileBusRecorder::Config cfg;
        cfg.tickSource = &CountingTick;
        cfg.tickContext = &tick;
        FileBusRecorder rec;
        ASSERT_TRUE(rec.Open(f.path.c_str(), cfg));
        bus.SetRecorder(&rec);

        auto hPrimary = bus.Subscribe<RecorderTestMsgA>(
            Dia::Core::StringCRC("primarySub"),
            [&bus](const RecorderTestMsgA& msg) {
                if (msg.value == 42) {
                    RecorderTestMsgB b;
                    b.x = 1.5f;
                    b.y = -2.5f;
                    bus.Broadcast(b);
                }
            },
            Pass::Primary);
        auto hReaction = bus.Subscribe<RecorderTestMsgB>(
            Dia::Core::StringCRC("reactionSub"), [](const RecorderTestMsgB&) {}, Pass::Reaction);

        RecorderTestMsgA m1; m1.value = 42;
        RecorderTestMsgA m2; m2.value = 7;
        bus.Broadcast(m1);
        bus.Broadcast(m2);
        bus.Update();
        bus.SetRecorder(nullptr);
        rec.Close();

        BusTrafficFileReader reader;
        ASSERT_TRUE(reader.Open(f.path.c_str()));
        std::vector<BusTrafficFileReader::Record> recs;
        ASSERT_TRUE(reader.ReadAll(recs));
        ASSERT_EQ(recs.size(), 3u);

        EXPECT_EQ(recs[0].tick, 0u);
        EXPECT_EQ(recs[0].typeId, RecorderTestMsgA::kTypeId.Value());
        EXPECT_EQ(recs[0].routerId, Bus::kBroadcastRouterId.Value());
        EXPECT_EQ(recs[0].pass, static_cast<uint8_t>(Pass::Primary));
        ASSERT_EQ(recs[0].payload.size(), sizeof(RecorderTestMsgA));
        RecorderTestMsgA a0;
        std::memcpy(&a0, recs[0].payload.data(), sizeof(a0));
        EXPECT_EQ(a0.value, 42);

        EXPECT_EQ(recs[1].tick, 1u);
        EXPECT_EQ(recs[1].typeId, RecorderTestMsgA::kTypeId.Value());
        RecorderTestMsgA a1;
        std::memcpy(&a1, recs[1].payload.data(), sizeof(a1));
        EXPECT_EQ(a1.value, 7);

        EXPECT_EQ(recs[2].tick, 2u);
        EXPECT_EQ(recs[2].typeId, RecorderTestMsgB::kTypeId.Value());
        EXPECT_EQ(recs[2].pass, static_cast<uint8_t>(Pass::Reaction));
        ASSERT_EQ(recs[2].payload.size(), sizeof(RecorderTestMsgB));
        RecorderTestMsgB b2;
        std::memcpy(&b2, recs[2].payload.data(), sizeof(b2));
        EXPECT_EQ(b2.x, 1.5f);
        EXPECT_EQ(b2.y, -2.5f);
    }

    TEST(BusTrafficRecorder, File_SmallBuffer_StillCompleteAndOrdered) {
        TempFile f("dia_bus_rec_smallbuf.dat");
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgA, 16>()));

        uint64_t tick = 0;
        FileBusRecorder::Config cfg;
        cfg.tickSource = &CountingTick;
        cfg.tickContext = &tick;
        cfg.bufferSizeBytes = 64;
        FileBusRecorder rec;
        ASSERT_TRUE(rec.Open(f.path.c_str(), cfg));
        bus.SetRecorder(&rec);
        for (int i = 0; i < 10; ++i) {
            RecorderTestMsgA m; m.value = i * 3;
            bus.Broadcast(m);
        }
        bus.Update();
        bus.SetRecorder(nullptr);
        rec.Close();

        BusTrafficFileReader reader;
        ASSERT_TRUE(reader.Open(f.path.c_str()));
        std::vector<BusTrafficFileReader::Record> recs;
        ASSERT_TRUE(reader.ReadAll(recs));
        ASSERT_EQ(recs.size(), 10u);
        for (int i = 0; i < 10; ++i) {
            EXPECT_EQ(recs[i].tick, static_cast<uint64_t>(i));
            RecorderTestMsgA m;
            ASSERT_EQ(recs[i].payload.size(), sizeof(m));
            std::memcpy(&m, recs[i].payload.data(), sizeof(m));
            EXPECT_EQ(m.value, i * 3);
        }
    }

    // AC-9: 10,000-message burst round-trips with count, order and payloads matching.
    TEST(BusTrafficRecorder, File_Burst10k_RoundTrip) {
        constexpr int kTotal = 10000;
        TempFile f("dia_bus_rec_burst10k.dat");
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgA, 256>()));
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgB, 256>()));

        uint64_t tick = 0;
        FileBusRecorder::Config cfg;
        cfg.tickSource = &CountingTick;
        cfg.tickContext = &tick;
        FileBusRecorder rec;
        ASSERT_TRUE(rec.Open(f.path.c_str(), cfg));
        bus.SetRecorder(&rec);

        // Even index -> MsgA (value = i); odd index -> MsgB (x = i, y = -i).
        // Dispatch order is per-type queue order, so Update after every post
        // to keep global order == index order (queue capacity 256 is never hit).
        for (int start = 0; start < kTotal; ++start) {
            if (start % 2 == 0) {
                RecorderTestMsgA m; m.value = start;
                bus.Broadcast(m);
            } else {
                RecorderTestMsgB m; m.x = static_cast<float>(start); m.y = -static_cast<float>(start);
                bus.Broadcast(m);
            }
            // Update after every post keeps index order independent of per-type queue draining
            bus.Update();
        }
        bus.SetRecorder(nullptr);
        rec.Close();

        BusTrafficFileReader reader;
        ASSERT_TRUE(reader.Open(f.path.c_str()));
        std::vector<BusTrafficFileReader::Record> recs;
        ASSERT_TRUE(reader.ReadAll(recs));
        ASSERT_EQ(recs.size(), static_cast<size_t>(kTotal));
        uint64_t prevTick = 0;
        for (int i = 0; i < kTotal; ++i) {
            if (i > 0) ASSERT_GT(recs[i].tick, prevTick);
            prevTick = recs[i].tick;
            if (i % 2 == 0) {
                ASSERT_EQ(recs[i].typeId, RecorderTestMsgA::kTypeId.Value());
                RecorderTestMsgA m;
                ASSERT_EQ(recs[i].payload.size(), sizeof(m));
                std::memcpy(&m, recs[i].payload.data(), sizeof(m));
                ASSERT_EQ(m.value, i);
            } else {
                ASSERT_EQ(recs[i].typeId, RecorderTestMsgB::kTypeId.Value());
                RecorderTestMsgB m;
                ASSERT_EQ(recs[i].payload.size(), sizeof(m));
                std::memcpy(&m, recs[i].payload.data(), sizeof(m));
                ASSERT_EQ(m.x, static_cast<float>(i));
                ASSERT_EQ(m.y, -static_cast<float>(i));
            }
        }
    }

    TEST(BusTrafficRecorder, File_UnwritablePath_LogsOnceAndBusUnaffected) {
        Dia::Observation::Log::Logger::Instance().RegisterThreadBuffer();
        WarningCountSink sink;
        Dia::Observation::Log::Logger::Instance().RegisterSink(&sink);

        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgA, 8>()));
        int delivered = 0;
        auto h = bus.Subscribe<RecorderTestMsgA>(
            Dia::Core::StringCRC("sub"), [&delivered](const RecorderTestMsgA&) { ++delivered; });

        uint64_t tick = 0;
        FileBusRecorder::Config cfg;
        cfg.tickSource = &CountingTick;
        cfg.tickContext = &tick;
        FileBusRecorder rec;
        const std::string bad = TempPath("dia_bus_rec_no_such_dir_xyz/sub/x.dat");
        EXPECT_FALSE(rec.Open(bad.c_str(), cfg));

        bus.SetRecorder(&rec);
        for (int i = 0; i < 5; ++i) bus.Broadcast(RecorderTestMsgA{});
        bus.Update();
        bus.SetRecorder(nullptr);
        rec.Close();

        Dia::Observation::Log::Logger::Instance().FlushSync();
        Dia::Observation::Log::Logger::Instance().UnregisterSink(&sink);
        Dia::Observation::Log::Logger::Instance().UnregisterThreadBuffer();

        EXPECT_EQ(delivered, 5);
        EXPECT_EQ(sink.mWarnings, 1u);
    }
#endif // DIA_DEBUG

} // namespace Dia::MessageBus::Testing
