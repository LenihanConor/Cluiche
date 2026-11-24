#include <gtest/gtest.h>

#include <Modules/StageResultsFile.h>
#include <DiaCore/Json/external/json/json.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <process.h>
#define DIA_TEST_PID _getpid()
#else
#include <unistd.h>
#define DIA_TEST_PID getpid()
#endif

using namespace Cluiche::AppFlow;
namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        static int counter = 0;
        std::ostringstream n;
        n << "StageResultsTest_" << DIA_TEST_PID << "_" << std::chrono::steady_clock::now().time_since_epoch().count() << "_" << counter++;
        path = fs::temp_directory_path() / n.str();
        fs::create_directories(path);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
    std::string File(const char* name) const { return (path / name).string(); }
};

void WriteText(const std::string& path, const std::string& text) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path, std::ios::binary);
    f << text;
}

std::string ReadText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

StageResult MakeResult(StageResultSeverity sev, uint64_t utc, const char* label = "lbl", const char* session = "s1", const char* log = "out/x.log") {
    StageResult r;
    r.severity = sev;
    r.utcMs = utc;
    r.SetLabel(label);
    r.SetSessionId(session);
    r.SetLogPath(log);
    return r;
}

std::string FileWith(const char* resultsJson, const char* format = "dia.stage_results", int version = 1) {
    std::ostringstream o;
    o << "{\"format\":\"" << format << "\",\"version\":" << version << ",\"results\":" << resultsJson << "}";
    return o.str();
}

const char* kDiskAB =
    "{\"A\":{\"severity\":\"ok\",\"label\":\"diskA\",\"utc\":10,\"session_id\":\"d\",\"log_path\":\"a.log\"},"
    "\"B\":{\"severity\":\"warn\",\"label\":\"diskB\",\"utc\":50,\"session_id\":\"d\",\"log_path\":\"b.log\"}}";

} // namespace

TEST(StageResultsTable, SetReplacesExistingAndFinds) {
    auto t = std::make_unique<StageResultsTable>();
    EXPECT_TRUE(t->Set("A", MakeResult(StageResultSeverity::kOk, 1)));
    EXPECT_TRUE(t->Set("A", MakeResult(StageResultSeverity::kFail, 2)));
    ASSERT_EQ(t->Count(), 1u);
    EXPECT_EQ((*t)[0].result.severity, StageResultSeverity::kFail);
    EXPECT_EQ(t->Find("A"), 0);
    EXPECT_EQ(t->Find("Z"), -1);
    EXPECT_STREQ((*t)[0].name.AsCStr(), "A");
    t->Clear();
    EXPECT_EQ(t->Count(), 0u);
}

TEST(StageResultsTable, CapacityIs64) {
    auto t = std::make_unique<StageResultsTable>();
    for (unsigned int i = 0; i < StageResultsTable::kMaxStages; ++i) {
        EXPECT_TRUE(t->Set(("S" + std::to_string(i)).c_str(), MakeResult(StageResultSeverity::kOk, i)));
    }
    EXPECT_FALSE(t->Set("Overflow", MakeResult(StageResultSeverity::kOk, 1)));
    EXPECT_TRUE(t->Set("S3", MakeResult(StageResultSeverity::kFail, 9)));
    EXPECT_EQ(t->Count(), 64u);
}

TEST(StageResultsTable, EventSetsByStage) {
    auto t = std::make_unique<StageResultsTable>();
    StageResultEvent e;
    e.stage = Dia::Core::StringCRC("Foo");
    e.result = MakeResult(StageResultSeverity::kWarn, 5);
    EXPECT_TRUE(t->Set(e));
    EXPECT_EQ(t->Find(Dia::Core::StringCRC("Foo")), 0);
}

TEST(StageResultsTable, SeverityNamesRoundTrip) {
    const StageResultSeverity all[] = { StageResultSeverity::kNone, StageResultSeverity::kOk, StageResultSeverity::kWarn, StageResultSeverity::kFail };
    for (StageResultSeverity s : all) {
        StageResultSeverity back = (s == StageResultSeverity::kFail) ? StageResultSeverity::kNone : StageResultSeverity::kFail;
        ASSERT_TRUE(StageResultSeverityFromName(StageResultSeverityName(s), back));
        EXPECT_EQ(back, s);
    }
    StageResultSeverity x = StageResultSeverity::kOk;
    EXPECT_FALSE(StageResultSeverityFromName("bogus", x));
    EXPECT_EQ(x, StageResultSeverity::kOk);
    EXPECT_STREQ(StageResultSeverityName(StageResultSeverity::kWarn), "warn");
}

TEST(StageResultsFile, ReadMissing) {
    TempDir d;
    auto t = std::make_unique<StageResultsTable>();
    t->Set("Old", MakeResult(StageResultSeverity::kOk, 1));
    EXPECT_EQ(StageResultsFile::Read(d.File("nope.json").c_str(), *t), StageResultsReadResult::kMissing);
    EXPECT_EQ(t->Count(), 0u);
}

TEST(StageResultsFile, ReadMalformed) {
    TempDir d;
    WriteText(d.File("r.json"), "{ not json");
    auto t = std::make_unique<StageResultsTable>();
    t->Set("Old", MakeResult(StageResultSeverity::kOk, 1));
    EXPECT_EQ(StageResultsFile::Read(d.File("r.json").c_str(), *t), StageResultsReadResult::kMalformed);
    EXPECT_EQ(t->Count(), 0u);
}

TEST(StageResultsFile, ReadWrongFormat) {
    TempDir d;
    WriteText(d.File("r.json"), FileWith(kDiskAB, "something.else"));
    auto t = std::make_unique<StageResultsTable>();
    t->Set("Old", MakeResult(StageResultSeverity::kOk, 1));
    EXPECT_EQ(StageResultsFile::Read(d.File("r.json").c_str(), *t), StageResultsReadResult::kWrongFormat);
    EXPECT_EQ(t->Count(), 0u);
}

TEST(StageResultsFile, ReadUnsupportedVersion) {
    TempDir d;
    WriteText(d.File("r.json"), FileWith(kDiskAB, "dia.stage_results", 2));
    auto t = std::make_unique<StageResultsTable>();
    t->Set("Old", MakeResult(StageResultSeverity::kOk, 1));
    EXPECT_EQ(StageResultsFile::Read(d.File("r.json").c_str(), *t), StageResultsReadResult::kUnsupportedVersion);
    EXPECT_EQ(t->Count(), 0u);
}

TEST(StageResultsFile, ReadToleratesUnknownKeys) {
    TempDir d;
    WriteText(d.File("r.json"), FileWith("{\"A\":{\"severity\":\"fail\",\"label\":\"x\",\"utc\":7,\"session_id\":\"s\",\"log_path\":\"l\",\"extra\":[1,2],\"more\":{\"a\":1}}}"));
    auto t = std::make_unique<StageResultsTable>();
    ASSERT_EQ(StageResultsFile::Read(d.File("r.json").c_str(), *t), StageResultsReadResult::kOk);
    ASSERT_EQ(t->Count(), 1u);
    EXPECT_EQ((*t)[0].result.severity, StageResultSeverity::kFail);
    EXPECT_EQ((*t)[0].result.utcMs, 7u);
}

TEST(StageResultsFile, RoundTripAllFields) {
    TempDir d;
    const std::string longLog(255, 'p');
    auto in = std::make_unique<StageResultsTable>();
    in->Set("Alpha", MakeResult(StageResultSeverity::kWarn, 1700000000123ull, "0123456789abcdef", "sess-1", longLog.c_str()));
    in->Set("Beta", MakeResult(StageResultSeverity::kNone, 0, "", "", ""));
    ASSERT_TRUE(StageResultsFile::Write(d.File("r.json").c_str(), *in));

    auto out = std::make_unique<StageResultsTable>();
    ASSERT_EQ(StageResultsFile::Read(d.File("r.json").c_str(), *out), StageResultsReadResult::kOk);
    ASSERT_EQ(out->Count(), 2u);
    const int a = out->Find("Alpha");
    ASSERT_GE(a, 0);
    const StageResult& r = (*out)[a].result;
    EXPECT_EQ(r.severity, StageResultSeverity::kWarn);
    EXPECT_STREQ(r.label.AsCStr(), "0123456789abcdef");
    EXPECT_EQ(r.utcMs, 1700000000123ull);
    EXPECT_STREQ(r.sessionId.AsCStr(), "sess-1");
    EXPECT_EQ(std::string(r.logPath.AsCStr()), longLog);
    const int b = out->Find("Beta");
    ASSERT_GE(b, 0);
    EXPECT_EQ((*out)[b].result.severity, StageResultSeverity::kNone);
}

TEST(StageResultsFile, WrittenJsonHasDocumentedShape) {
    TempDir d;
    auto in = std::make_unique<StageResultsTable>();
    in->Set("A", MakeResult(StageResultSeverity::kFail, 42, "lab", "sid", "p.log"));
    ASSERT_TRUE(StageResultsFile::Write(d.File("r.json").c_str(), *in));
    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(ReadText(d.File("r.json")), root));
    EXPECT_EQ(root["format"].asString(), "dia.stage_results");
    EXPECT_EQ(root["version"].asInt(), 1);
    const Json::Value& e = root["results"]["A"];
    EXPECT_EQ(e["severity"].asString(), "fail");
    EXPECT_EQ(e["label"].asString(), "lab");
    EXPECT_EQ(e["utc"].asUInt64(), 42u);
    EXPECT_EQ(e["session_id"].asString(), "sid");
    EXPECT_EQ(e["log_path"].asString(), "p.log");
}

TEST(StageResultsFile, WriteMergesNewerWins) {
    TempDir d;
    WriteText(d.File("r.json"), FileWith(kDiskAB));
    auto t = std::make_unique<StageResultsTable>();
    t->Set("A", MakeResult(StageResultSeverity::kFail, 20, "tableA"));
    t->Set("C", MakeResult(StageResultSeverity::kOk, 5, "tableC"));
    ASSERT_TRUE(StageResultsFile::Write(d.File("r.json").c_str(), *t));
    EXPECT_EQ(t->Count(), 2u);  // table unchanged

    auto out = std::make_unique<StageResultsTable>();
    ASSERT_EQ(StageResultsFile::Read(d.File("r.json").c_str(), *out), StageResultsReadResult::kOk);
    ASSERT_EQ(out->Count(), 3u);
    EXPECT_EQ((*out)[out->Find("A")].result.utcMs, 20u);
    EXPECT_STREQ((*out)[out->Find("A")].result.label.AsCStr(), "tableA");
    EXPECT_EQ((*out)[out->Find("B")].result.utcMs, 50u);
    EXPECT_STREQ((*out)[out->Find("B")].result.label.AsCStr(), "diskB");
    EXPECT_GE(out->Find("C"), 0);
}

TEST(StageResultsFile, WriteKeepsDiskWhenTableOlder) {
    TempDir d;
    WriteText(d.File("r.json"), FileWith(kDiskAB));
    auto t = std::make_unique<StageResultsTable>();
    t->Set("B", MakeResult(StageResultSeverity::kFail, 49, "older"));
    ASSERT_TRUE(StageResultsFile::Write(d.File("r.json").c_str(), *t));
    auto out = std::make_unique<StageResultsTable>();
    ASSERT_EQ(StageResultsFile::Read(d.File("r.json").c_str(), *out), StageResultsReadResult::kOk);
    EXPECT_STREQ((*out)[out->Find("B")].result.label.AsCStr(), "diskB");
    EXPECT_EQ((*out)[out->Find("B")].result.utcMs, 50u);
}

TEST(StageResultsFile, WriteTieGoesToTable) {
    TempDir d;
    WriteText(d.File("r.json"), FileWith(kDiskAB));
    auto t = std::make_unique<StageResultsTable>();
    t->Set("B", MakeResult(StageResultSeverity::kFail, 50, "tie"));
    ASSERT_TRUE(StageResultsFile::Write(d.File("r.json").c_str(), *t));
    auto out = std::make_unique<StageResultsTable>();
    ASSERT_EQ(StageResultsFile::Read(d.File("r.json").c_str(), *out), StageResultsReadResult::kOk);
    EXPECT_STREQ((*out)[out->Find("B")].result.label.AsCStr(), "tie");
}

TEST(StageResultsFile, WriteOverCorruptDiskFileReplacesIt) {
    TempDir d;
    WriteText(d.File("r.json"), "garbage{");
    auto t = std::make_unique<StageResultsTable>();
    t->Set("A", MakeResult(StageResultSeverity::kOk, 3));
    ASSERT_TRUE(StageResultsFile::Write(d.File("r.json").c_str(), *t));
    auto out = std::make_unique<StageResultsTable>();
    EXPECT_EQ(StageResultsFile::Read(d.File("r.json").c_str(), *out), StageResultsReadResult::kOk);
    EXPECT_EQ(out->Count(), 1u);
}

TEST(StageResultsFile, TmpLeftoverOverwrittenAndRemoved) {
    TempDir d;
    WriteText(d.File("r.json.tmp"), "stale junk");
    auto t = std::make_unique<StageResultsTable>();
    t->Set("A", MakeResult(StageResultSeverity::kOk, 3));
    ASSERT_TRUE(StageResultsFile::Write(d.File("r.json").c_str(), *t));
    EXPECT_FALSE(fs::exists(d.File("r.json.tmp")));
    auto out = std::make_unique<StageResultsTable>();
    EXPECT_EQ(StageResultsFile::Read(d.File("r.json").c_str(), *out), StageResultsReadResult::kOk);
}

TEST(StageResultsFile, CreatesParentDirectory) {
    TempDir d;
    const std::string p = (d.path / "a" / "b" / "r.json").string();
    auto t = std::make_unique<StageResultsTable>();
    t->Set("A", MakeResult(StageResultSeverity::kOk, 3));
    ASSERT_TRUE(StageResultsFile::Write(p.c_str(), *t));
    EXPECT_TRUE(fs::exists(p));
}

TEST(StageResultsFile, WriteFailureLeavesTableUnchanged) {
    TempDir d;
    // Parent "directory" is a regular file, so the write cannot succeed.
    WriteText(d.File("blocker"), "x");
    const std::string p = (d.path / "blocker" / "r.json").string();
    auto t = std::make_unique<StageResultsTable>();
    t->Set("A", MakeResult(StageResultSeverity::kOk, 3));
    EXPECT_FALSE(StageResultsFile::Write(p.c_str(), *t));
    EXPECT_EQ(t->Count(), 1u);
}
