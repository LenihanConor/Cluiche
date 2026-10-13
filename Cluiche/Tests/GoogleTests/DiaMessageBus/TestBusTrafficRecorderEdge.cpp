// TestBusTrafficRecorderEdge.cpp
//
// Edge-case and format-contract tests for the Bus Traffic Recorder
// (FileBusRecorder, InMemoryBusRecorder, BusTrafficFileReader, Bus::SetRecorder).
// Complements TestBusTrafficRecorder.cpp. Debug-only (DIA_DEBUG).
//
// Not covered: mid-run write failure (no portable, deterministic way to force an
// fwrite failure without OS-specific quota/handle tricks).

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <type_traits>

#include <DiaCore/Core/Assert.h>
#include <DiaMessageBus/Bus.h>

#ifdef DIA_DEBUG
#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaMessageBus/FileBusRecorder.h>
#include <DiaMessageBus/Testing/BusTrafficFileReader.h>
#include <DiaMessageBus/IBusRecorder.h>
#include <DiaMessageBus/Testing/InMemoryBusRecorder.h>

namespace Dia::MessageBus::Testing {

    namespace {
        struct EdgeMsgSmall {
            static inline const Dia::Core::StringCRC kTypeId{ "EdgeMsgSmall" };
            int value = 0;
        };

        struct EdgeMsgLarge {
            static inline const Dia::Core::StringCRC kTypeId{ "EdgeMsgLarge" };
            uint8_t bytes[100];
        };
        static_assert(std::is_trivially_copyable_v<EdgeMsgLarge>);

        struct EdgeMsgNonTrivial {
            static inline const Dia::Core::StringCRC kTypeId{ "EdgeMsgNonTrivial" };
            int value = 0;
            EdgeMsgNonTrivial() = default;
            EdgeMsgNonTrivial(const EdgeMsgNonTrivial& o) : value(o.value) {}
            EdgeMsgNonTrivial& operator=(const EdgeMsgNonTrivial& o) { value = o.value; return *this; }
        };
        static_assert(!std::is_trivially_copyable_v<EdgeMsgNonTrivial>);

        using Reader = BusTrafficFileReader;
        using Rec = BusTrafficFileReader::Record;

        std::string EdgeTempPath(const char* name) {
            return (std::filesystem::temp_directory_path() / name).string();
        }

        struct EdgeTempFile {
            explicit EdgeTempFile(const char* name) : path(EdgeTempPath(name)) { std::remove(path.c_str()); }
            ~EdgeTempFile() { std::remove(path.c_str()); }
            std::string path;
        };

        uint64_t EdgeCountingTick(void* ctx) { return (*static_cast<uint64_t*>(ctx))++; }

        FileBusRecorder::Config MakeCfg(uint64_t* tick, uint32_t bufferSize = 64 * 1024) {
            FileBusRecorder::Config cfg;
            cfg.tickSource = &EdgeCountingTick;
            cfg.tickContext = tick;
            cfg.bufferSizeBytes = bufferSize;
            return cfg;
        }

        std::vector<uint8_t> ReadAllBytes(const std::string& path) {
            std::ifstream in(path, std::ios::binary);
            return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }

        void WriteAllBytes(const std::string& path, const std::vector<uint8_t>& bytes, size_t count) {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(count));
        }

        void AppendU32(std::vector<uint8_t>& v, uint32_t x) {
            for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>(x >> (8 * i)));
        }
        void AppendU64(std::vector<uint8_t>& v, uint64_t x) {
            for (int i = 0; i < 8; ++i) v.push_back(static_cast<uint8_t>(x >> (8 * i)));
        }

        class EdgeWarningSink : public Dia::Observation::Log::ISink {
        public:
            EdgeWarningSink() {
                SetLevelThreshold(Dia::Observation::Log::LogLevel::kDebug);
                SetChannelFilter(Dia::Core::StringCRC("DiaMessageBus"), true);
            }
            void OnLogEntry(const Dia::Observation::Log::LogEntry& entry) override {
                if (entry.level == Dia::Observation::Log::LogLevel::kWarning) ++mWarnings;
            }
            const char* GetName() const override { return "EdgeWarningSink"; }
            unsigned int mWarnings = 0;
        };

        // RAII: registers a warning-counting sink and thread log buffer.
        struct WarningCapture {
            WarningCapture() {
                Dia::Observation::Log::Logger::Instance().RegisterThreadBuffer();
                Dia::Observation::Log::Logger::Instance().RegisterSink(&sink);
            }
            unsigned int Count() {
                Dia::Observation::Log::Logger::Instance().FlushSync();
                return sink.mWarnings;
            }
            ~WarningCapture() {
                Dia::Observation::Log::Logger::Instance().FlushSync();
                Dia::Observation::Log::Logger::Instance().UnregisterSink(&sink);
                Dia::Observation::Log::Logger::Instance().UnregisterThreadBuffer();
            }
            EdgeWarningSink sink;
        };

        // Writes `count` records (value = first + i, Primary pass) with a 4-byte payload each.
        void WriteSmallRecords(FileBusRecorder& rec, int first, int count) {
            for (int i = first; i < first + count; ++i) {
                EdgeMsgSmall m; m.value = i;
                rec.OnDispatch(EdgeMsgSmall::kTypeId, Dia::Core::StringCRC("r"), Pass::Primary, &m, sizeof(m));
            }
        }

        int ValueOf(const Rec& r) {
            EdgeMsgSmall m;
            std::memcpy(&m, r.payload.data(), sizeof(m));
            return m.value;
        }

        bool ReadFile(const std::string& path, std::vector<Rec>& out) {
            Reader reader;
            if (!reader.Open(path.c_str())) return false;
            return reader.ReadAll(out);
        }

        // Writes a valid 2-record file (8-byte header, each record = 24 + 4) and returns its bytes.
        std::vector<uint8_t> MakeTwoRecordFile(const std::string& path) {
            uint64_t tick = 0;
            {
                FileBusRecorder rec;
                EXPECT_TRUE(rec.Open(path.c_str(), MakeCfg(&tick)));
                WriteSmallRecords(rec, 0, 2);
            }
            return ReadAllBytes(path);
        }

        // Counting assert handler for AC-10.
        int gAssertCount = 0;
        void CountingAssert(const char*, const char*, int, const char*, ...) { ++gAssertCount; }
    }

    // -----------------------------------------------------------------------
    // 1. Byte layout: exact on-disk contract, read with raw I/O
    // -----------------------------------------------------------------------
    TEST(BusTrafficRecorder_Edge, ByteLayout_ExactBytes) {
        // Arrange
        EdgeTempFile f("dia_bus_edge_layout.dat");
        uint64_t tick = 0x0102030405060708ull;
        FileBusRecorder::Config cfg;
        cfg.tickSource = [](void* ctx) -> uint64_t { return (*static_cast<uint64_t*>(ctx))++; };
        cfg.tickContext = &tick;
        const Dia::Core::StringCRC type1("EdgeLayoutType1"), router1("EdgeLayoutRouter1");
        const Dia::Core::StringCRC type2("EdgeLayoutType2"), router2("EdgeLayoutRouter2");
        const uint8_t payload1[3] = { 0xAA, 0xBB, 0xCC };
        const uint8_t payload2[5] = { 1, 2, 3, 4, 5 };

        // Act
        {
            FileBusRecorder rec;
            ASSERT_TRUE(rec.Open(f.path.c_str(), cfg));
            rec.OnDispatch(type1, router1, Pass::Primary, payload1, sizeof(payload1));
            rec.OnDispatch(type2, router2, Pass::Reaction, payload2, sizeof(payload2));
        }

        // Assert: expected bytes built by hand, little-endian
        std::vector<uint8_t> expected = { 'D', 'B', 'T', 'R' };
        AppendU32(expected, 1);
        AppendU64(expected, 0x0102030405060708ull);
        AppendU32(expected, type1.Value());
        AppendU32(expected, router1.Value());
        expected.push_back(0); expected.push_back(0); expected.push_back(0); expected.push_back(0);
        AppendU32(expected, 3);
        expected.insert(expected.end(), payload1, payload1 + 3);
        AppendU64(expected, 0x0102030405060709ull);
        AppendU32(expected, type2.Value());
        AppendU32(expected, router2.Value());
        expected.push_back(1); expected.push_back(0); expected.push_back(0); expected.push_back(0);
        AppendU32(expected, 5);
        expected.insert(expected.end(), payload2, payload2 + 5);

        const std::vector<uint8_t> actual = ReadAllBytes(f.path);
        ASSERT_EQ(actual.size(), expected.size());
        EXPECT_EQ(actual.size(), 8u + (24u + 3u) + (24u + 5u));
        EXPECT_EQ(actual, expected);
        // Spot-check LE ordering of the first tick independently of the helper
        EXPECT_EQ(actual[8], 0x08);
        EXPECT_EQ(actual[15], 0x01);
    }

    // -----------------------------------------------------------------------
    // 2. Record larger than buffer
    // -----------------------------------------------------------------------
    TEST(BusTrafficRecorder_Edge, RecordLargerThanBuffer_RoundTripsIntact) {
        EdgeTempFile f("dia_bus_edge_bigrec.dat");
        uint64_t tick = 0;
        EdgeMsgLarge a, b;
        for (int i = 0; i < 100; ++i) { a.bytes[i] = static_cast<uint8_t>(i); b.bytes[i] = static_cast<uint8_t>(255 - i); }
        {
            FileBusRecorder rec;
            ASSERT_TRUE(rec.Open(f.path.c_str(), MakeCfg(&tick, 16)));
            rec.OnDispatch(EdgeMsgLarge::kTypeId, Dia::Core::StringCRC("r"), Pass::Primary, &a, sizeof(a));
            rec.OnDispatch(EdgeMsgLarge::kTypeId, Dia::Core::StringCRC("r"), Pass::Reaction, &b, sizeof(b));
        }
        std::vector<Rec> recs;
        ASSERT_TRUE(ReadFile(f.path, recs));
        ASSERT_EQ(recs.size(), 2u);
        ASSERT_EQ(recs[0].payload.size(), 100u);
        ASSERT_EQ(recs[1].payload.size(), 100u);
        EXPECT_EQ(std::memcmp(recs[0].payload.data(), a.bytes, 100), 0);
        EXPECT_EQ(std::memcmp(recs[1].payload.data(), b.bytes, 100), 0);
        EXPECT_EQ(recs[1].pass, 1);
    }

    // -----------------------------------------------------------------------
    // 3. bufferSizeBytes == 0 is clamped
    // -----------------------------------------------------------------------
    TEST(BusTrafficRecorder_Edge, ZeroBufferSize_ClampedAndRoundTripsOrdered) {
        EdgeTempFile f("dia_bus_edge_zerobuf.dat");
        uint64_t tick = 0;
        {
            FileBusRecorder rec;
            ASSERT_TRUE(rec.Open(f.path.c_str(), MakeCfg(&tick, 0)));
            WriteSmallRecords(rec, 0, 20);
        }
        std::vector<Rec> recs;
        ASSERT_TRUE(ReadFile(f.path, recs));
        ASSERT_EQ(recs.size(), 20u);
        for (int i = 0; i < 20; ++i) {
            EXPECT_EQ(ValueOf(recs[i]), i);
            EXPECT_EQ(recs[i].tick, static_cast<uint64_t>(i));
        }
    }

    // -----------------------------------------------------------------------
    // 4. Zero-size payload
    // -----------------------------------------------------------------------
    TEST(BusTrafficRecorder_Edge, ZeroSizePayload_RecordedAndNextUnaffected) {
        EdgeTempFile f("dia_bus_edge_zeropayload.dat");
        uint64_t tick = 0;
        {
            FileBusRecorder rec;
            ASSERT_TRUE(rec.Open(f.path.c_str(), MakeCfg(&tick)));
            rec.OnDispatch(Dia::Core::StringCRC("EdgeEmpty"), Dia::Core::StringCRC("r"), Pass::Primary, nullptr, 0);
            WriteSmallRecords(rec, 77, 1);
        }
        std::vector<Rec> recs;
        ASSERT_TRUE(ReadFile(f.path, recs));
        ASSERT_EQ(recs.size(), 2u);
        EXPECT_EQ(recs[0].typeId, Dia::Core::StringCRC("EdgeEmpty").Value());
        EXPECT_TRUE(recs[0].payload.empty());
        ASSERT_EQ(recs[1].payload.size(), sizeof(EdgeMsgSmall));
        EXPECT_EQ(ValueOf(recs[1]), 77);
        EXPECT_EQ(recs[1].typeId, EdgeMsgSmall::kTypeId.Value());
    }

    // -----------------------------------------------------------------------
    // 5. Tick source called exactly once per record
    // -----------------------------------------------------------------------
    TEST(BusTrafficRecorder_Edge, TickSource_CalledExactlyOncePerRecord) {
        EdgeTempFile f("dia_bus_edge_tickcount.dat");
        uint64_t calls = 0;
        FileBusRecorder::Config cfg;
        cfg.tickSource = [](void* ctx) -> uint64_t { return ++(*static_cast<uint64_t*>(ctx)); };
        cfg.tickContext = &calls;
        FileBusRecorder rec;
        ASSERT_TRUE(rec.Open(f.path.c_str(), cfg));
        EXPECT_EQ(calls, 0u); // Open itself does not tick

        WriteSmallRecords(rec, 0, 3);
        EXPECT_EQ(calls, 3u);
        rec.OnDispatch(Dia::Core::StringCRC("EdgeEmpty"), Dia::Core::StringCRC("r"), Pass::Primary, nullptr, 0);
        EXPECT_EQ(calls, 4u);
        rec.Flush();
        rec.Close();
        EXPECT_EQ(calls, 4u);

        std::vector<Rec> recs;
        ASSERT_TRUE(ReadFile(f.path, recs));
        ASSERT_EQ(recs.size(), 4u);
        EXPECT_EQ(recs[3].tick, 4u);
    }

    // -----------------------------------------------------------------------
    // 6. Open twice on one recorder
    // -----------------------------------------------------------------------
    TEST(BusTrafficRecorder_Edge, OpenTwice_FirstFileCompleteSecondGetsSubsequentOnly) {
        EdgeTempFile fa("dia_bus_edge_open2_a.dat");
        EdgeTempFile fb("dia_bus_edge_open2_b.dat");
        uint64_t tick = 0;
        FileBusRecorder rec;
        ASSERT_TRUE(rec.Open(fa.path.c_str(), MakeCfg(&tick)));
        WriteSmallRecords(rec, 0, 3);
        ASSERT_TRUE(rec.Open(fb.path.c_str(), MakeCfg(&tick)));
        WriteSmallRecords(rec, 100, 2);
        rec.Close();

        std::vector<Rec> a, b;
        ASSERT_TRUE(ReadFile(fa.path, a));
        ASSERT_TRUE(ReadFile(fb.path, b));
        ASSERT_EQ(a.size(), 3u);
        ASSERT_EQ(b.size(), 2u);
        for (int i = 0; i < 3; ++i) EXPECT_EQ(ValueOf(a[i]), i);
        EXPECT_EQ(ValueOf(b[0]), 100);
        EXPECT_EQ(ValueOf(b[1]), 101);
    }

    // -----------------------------------------------------------------------
    // 7. Close idempotence, dispatch after Close, destructor flush
    // -----------------------------------------------------------------------
    TEST(BusTrafficRecorder_Edge, Close_RepeatedIsSafe_DispatchAfterCloseDroppedWithoutWarning) {
        EdgeTempFile f("dia_bus_edge_close.dat");
        WarningCapture capture;
        uint64_t tick = 0;
        FileBusRecorder rec;
        ASSERT_TRUE(rec.Open(f.path.c_str(), MakeCfg(&tick)));
        WriteSmallRecords(rec, 0, 2);

        rec.Close();
        rec.Close();
        rec.Close();
        EXPECT_FALSE(rec.IsOpen());

        WriteSmallRecords(rec, 50, 3); // must be dropped
        rec.Flush();                   // no-op when closed
        rec.Close();

        EXPECT_EQ(capture.Count(), 0u);
        std::vector<Rec> recs;
        ASSERT_TRUE(ReadFile(f.path, recs));
        EXPECT_EQ(recs.size(), 2u);
    }

    TEST(BusTrafficRecorder_Edge, Destructor_WithoutExplicitClose_YieldsCompleteFile) {
        EdgeTempFile f("dia_bus_edge_dtor.dat");
        uint64_t tick = 0;
        {
            FileBusRecorder rec;
            ASSERT_TRUE(rec.Open(f.path.c_str(), MakeCfg(&tick)));
            WriteSmallRecords(rec, 0, 5);
        }
        std::vector<Rec> recs;
        ASSERT_TRUE(ReadFile(f.path, recs));
        ASSERT_EQ(recs.size(), 5u);
        EXPECT_EQ(ValueOf(recs[4]), 4);
    }

    // -----------------------------------------------------------------------
    // 8. Flush mid-run
    // -----------------------------------------------------------------------
    TEST(BusTrafficRecorder_Edge, Flush_MidRun_ReaderSeesAllRecordsThenMoreAfterClose) {
        EdgeTempFile f("dia_bus_edge_flush.dat");
        uint64_t tick = 0;
        FileBusRecorder rec;
        ASSERT_TRUE(rec.Open(f.path.c_str(), MakeCfg(&tick)));
        WriteSmallRecords(rec, 0, 4);
        rec.Flush();
        EXPECT_TRUE(rec.IsOpen());

        {
            std::vector<Rec> mid;
            ASSERT_TRUE(ReadFile(f.path, mid));
            ASSERT_EQ(mid.size(), 4u);
        }

        WriteSmallRecords(rec, 4, 3);
        rec.Close();

        std::vector<Rec> all;
        ASSERT_TRUE(ReadFile(f.path, all));
        ASSERT_EQ(all.size(), 7u);
        for (int i = 0; i < 7; ++i) EXPECT_EQ(ValueOf(all[i]), i);
    }

    // -----------------------------------------------------------------------
    // 9/10. Open failures
    // -----------------------------------------------------------------------
    TEST(BusTrafficRecorder_Edge, Open_NullPath_ReturnsFalseAndLogsExactlyOneWarning) {
        WarningCapture capture;
        uint64_t tick = 0;
        FileBusRecorder rec;
        EXPECT_FALSE(rec.Open(nullptr, MakeCfg(&tick)));
        EXPECT_FALSE(rec.IsOpen());
        EXPECT_EQ(capture.Count(), 1u);
    }

    TEST(BusTrafficRecorder_Edge, Open_AfterFailedOpen_SucceedsAndRecordsNormally) {
        EdgeTempFile f("dia_bus_edge_reuse.dat");
        WarningCapture capture;
        uint64_t tick = 0;
        FileBusRecorder rec;
        const std::string bad = EdgeTempPath("dia_bus_edge_no_such_dir_xyz/sub/x.dat");
        EXPECT_FALSE(rec.Open(bad.c_str(), MakeCfg(&tick)));
        WriteSmallRecords(rec, 0, 2); // dropped, recorder not open

        ASSERT_TRUE(rec.Open(f.path.c_str(), MakeCfg(&tick)));
        WriteSmallRecords(rec, 10, 3);
        rec.Close();

        std::vector<Rec> recs;
        ASSERT_TRUE(ReadFile(f.path, recs));
        ASSERT_EQ(recs.size(), 3u);
        EXPECT_EQ(ValueOf(recs[0]), 10);
        EXPECT_EQ(capture.Count(), 1u);
    }

    // -----------------------------------------------------------------------
    // 11. Mid-run bus swap
    // -----------------------------------------------------------------------
    TEST(BusTrafficRecorder_Edge, BusRecorderSwap_EachRecorderSeesOnlyItsWindow) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<EdgeMsgSmall, 16>()));
        int delivered = 0;
        auto h = bus.Subscribe<EdgeMsgSmall>(
            Dia::Core::StringCRC("edgeSwapSub"), [&delivered](const EdgeMsgSmall&) { ++delivered; });

        InMemoryBusRecorder a, b;
        auto post = [&bus](int first, int count) {
            for (int i = first; i < first + count; ++i) { EdgeMsgSmall m; m.value = i; bus.Broadcast(m); }
            bus.Update();
        };

        bus.SetRecorder(&a);
        post(0, 2);
        EXPECT_EQ(delivered, 2);
        bus.SetRecorder(&b);
        post(10, 3);
        EXPECT_EQ(delivered, 5);
        bus.SetRecorder(nullptr);
        post(20, 4);
        EXPECT_EQ(delivered, 9);

        ASSERT_EQ(a.GetRecords().size(), 2u);
        ASSERT_EQ(b.GetRecords().size(), 3u);
        EdgeMsgSmall m;
        std::memcpy(&m, a.GetRecords()[0].payloadBytes.data(), sizeof(m));
        EXPECT_EQ(m.value, 0);
        std::memcpy(&m, b.GetRecords()[0].payloadBytes.data(), sizeof(m));
        EXPECT_EQ(m.value, 10);
    }

    // -----------------------------------------------------------------------
    // 12. Reader robustness
    // -----------------------------------------------------------------------
    TEST(BusTrafficRecorder_Edge, Reader_BadMagic_Rejected) {
        EdgeTempFile f("dia_bus_edge_badmagic.dat");
        std::vector<uint8_t> bytes = { 'X', 'B', 'T', 'R' };
        AppendU32(bytes, 1);
        WriteAllBytes(f.path, bytes, bytes.size());
        Reader reader;
        EXPECT_FALSE(reader.Open(f.path.c_str()));
    }

    TEST(BusTrafficRecorder_Edge, Reader_FileShorterThanHeader_Rejected) {
        EdgeTempFile f("dia_bus_edge_shorthdr.dat");
        std::vector<uint8_t> bytes = { 'D', 'B', 'T', 'R', 1, 0 };
        WriteAllBytes(f.path, bytes, bytes.size());
        Reader reader;
        EXPECT_FALSE(reader.Open(f.path.c_str()));

        WriteAllBytes(f.path, bytes, 0); // fully empty file
        EXPECT_FALSE(reader.Open(f.path.c_str()));
    }

    TEST(BusTrafficRecorder_Edge, Reader_NonexistentPath_OpenFails) {
        Reader reader;
        EXPECT_FALSE(reader.Open(EdgeTempPath("dia_bus_edge_does_not_exist.dat").c_str()));
        Rec r;
        EXPECT_FALSE(reader.ReadNext(r));
    }

    TEST(BusTrafficRecorder_Edge, Reader_HeaderOnlyFile_ZeroRecordsCleanEnd) {
        EdgeTempFile f("dia_bus_edge_headeronly.dat");
        uint64_t tick = 0;
        {
            FileBusRecorder rec;
            ASSERT_TRUE(rec.Open(f.path.c_str(), MakeCfg(&tick)));
        }
        EXPECT_EQ(ReadAllBytes(f.path).size(), 8u);
        Reader reader;
        ASSERT_TRUE(reader.Open(f.path.c_str()));
        Rec r;
        EXPECT_FALSE(reader.ReadNext(r));
        EXPECT_FALSE(reader.IsTruncated());

        std::vector<Rec> all;
        Reader reader2;
        ASSERT_TRUE(reader2.Open(f.path.c_str()));
        EXPECT_TRUE(reader2.ReadAll(all));
        EXPECT_TRUE(all.empty());
    }

    TEST(BusTrafficRecorder_Edge, Reader_TruncatedMidPayload_FlaggedAndEarlierRecordsReadable) {
        EdgeTempFile full("dia_bus_edge_trunc_src1.dat");
        EdgeTempFile cut("dia_bus_edge_trunc_cut1.dat");
        const std::vector<uint8_t> bytes = MakeTwoRecordFile(full.path);
        ASSERT_EQ(bytes.size(), 8u + 28u + 28u);
        WriteAllBytes(cut.path, bytes, 8 + 28 + 24 + 2); // 2 of 4 payload bytes

        Reader reader;
        ASSERT_TRUE(reader.Open(cut.path.c_str()));
        Rec r;
        ASSERT_TRUE(reader.ReadNext(r));
        EXPECT_EQ(ValueOf(r), 0);
        EXPECT_FALSE(reader.IsTruncated());
        EXPECT_FALSE(reader.ReadNext(r));
        EXPECT_TRUE(reader.IsTruncated());
    }

    TEST(BusTrafficRecorder_Edge, Reader_TruncatedMidRecordHeader_FlaggedAndEarlierRecordsReadable) {
        EdgeTempFile full("dia_bus_edge_trunc_src2.dat");
        EdgeTempFile cut("dia_bus_edge_trunc_cut2.dat");
        const std::vector<uint8_t> bytes = MakeTwoRecordFile(full.path);
        WriteAllBytes(cut.path, bytes, 8 + 28 + 10); // 10 of 24 header bytes

        Reader reader;
        ASSERT_TRUE(reader.Open(cut.path.c_str()));
        std::vector<Rec> recs;
        EXPECT_FALSE(reader.ReadAll(recs));
        EXPECT_TRUE(reader.IsTruncated());
        ASSERT_EQ(recs.size(), 1u);
        EXPECT_EQ(ValueOf(recs[0]), 0);
    }

    // -----------------------------------------------------------------------
    // 14. AC-10 (runtime assert on non-trivially-copyable message)
    // -----------------------------------------------------------------------
#ifdef DEBUG
    TEST(BusTrafficRecorder_Edge, NonTriviallyCopyable_WithRecorder_TripsAssert) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<EdgeMsgNonTrivial, 8>()));
        InMemoryBusRecorder rec;
        bus.SetRecorder(&rec);

        const Dia::Core::DIA_ASSERT_FUNC previous = Dia::Core::g_pAssertFunc;
        Dia::Core::g_pAssertFunc = &CountingAssert;
        gAssertCount = 0;

        EdgeMsgNonTrivial m; m.value = 5;
        bus.Broadcast(m);
        bus.Update();

        Dia::Core::g_pAssertFunc = previous;
        bus.SetRecorder(nullptr);
        EXPECT_EQ(gAssertCount, 1);
    }

    TEST(BusTrafficRecorder_Edge, TriviallyCopyable_WithRecorder_DoesNotTripAssert) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<EdgeMsgSmall, 8>()));
        InMemoryBusRecorder rec;
        bus.SetRecorder(&rec);

        const Dia::Core::DIA_ASSERT_FUNC previous = Dia::Core::g_pAssertFunc;
        Dia::Core::g_pAssertFunc = &CountingAssert;
        gAssertCount = 0;

        bus.Broadcast(EdgeMsgSmall{});
        bus.Update();

        Dia::Core::g_pAssertFunc = previous;
        bus.SetRecorder(nullptr);
        EXPECT_EQ(gAssertCount, 0);
        EXPECT_EQ(rec.GetRecords().size(), 1u);
    }
#endif

    TEST(BusTrafficRecorder_Edge, NonTriviallyCopyable_WithoutRecorder_DispatchesNormally) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<EdgeMsgNonTrivial, 8>()));
        int received = -1;
        auto h = bus.Subscribe<EdgeMsgNonTrivial>(
            Dia::Core::StringCRC("edgeNonTrivialSub"), [&received](const EdgeMsgNonTrivial& m) { received = m.value; });

        const Dia::Core::DIA_ASSERT_FUNC previous = Dia::Core::g_pAssertFunc;
        Dia::Core::g_pAssertFunc = &CountingAssert;
        gAssertCount = 0;

        EdgeMsgNonTrivial m; m.value = 9;
        bus.Broadcast(m);
        bus.Update();

        Dia::Core::g_pAssertFunc = previous;
        EXPECT_EQ(received, 9);
        EXPECT_EQ(gAssertCount, 0);
    }

} // namespace Dia::MessageBus::Testing

#endif // DIA_DEBUG
