#include <gtest/gtest.h>

#include <Shared/Debug/RecordsCommandHelper.h>
#include <Shared/Debug/Testing/FakeSimCommandHost.h>

namespace CoW {
namespace Debug {
namespace Testing {

namespace {

struct Rig
{
    RecordsCommandHelper helper{ "ecology" };
    std::vector<std::string> ids{ "wolf", "deer" };
    bool nextOk = true;
    uint32_t queued = 0;
    bool failing = false;
    std::string healthText;
    uint32_t reloads = 0;

    Rig()
    {
        helper.SetListFn([this]() { return ids; });
        helper.SetReloadEnqueue([this]() { ++queued; return true; });
        helper.SetHealthFn([this](bool f, const std::string& t) { failing = f; healthText = t; });
        helper.SetReloadFn([this]() {
            ++reloads;
            RecordReloadResult r;
            if (nextOk) { ids = { "wolf", "deer", "fox" }; r.ids = ids; return r; }
            r.ok = false;
            r.error = { "species.json", "wolf", "diet", "unknown food 'moss'" };
            return r;   // previous ids kept
        });
    }

    static Json::Value Args(const char* action)
    {
        Json::Value a(Json::objectValue);
        a["action"] = action;
        return a;
    }
};

}  // namespace

TEST(RecordsCommandHelper, CommandNameIsSystemRecords)
{
    RecordsCommandHelper h("ecology");
    EXPECT_STREQ(h.CommandName(), "cow.ecology.records");
}

TEST(RecordsCommandHelper, ListReturnsIds)
{
    Rig rig;
    const Json::Value r = rig.helper.Handle(Rig::Args("list"));
    ASSERT_TRUE(r["success"].asBool());
    ASSERT_EQ(r["records"].size(), 2u);
    EXPECT_EQ(r["records"][0].asString(), "wolf");
    EXPECT_EQ(r["records"][1].asString(), "deer");
}

TEST(RecordsCommandHelper, ReloadIsQueuedAndNothingChangesUntilApply)
{
    Rig rig;
    const Json::Value r = rig.helper.Handle(Rig::Args("reload"));
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_TRUE(r["queued"].asBool());
    EXPECT_EQ(rig.queued, 1u);
    EXPECT_EQ(rig.reloads, 0u);
    EXPECT_EQ(rig.helper.Handle(Rig::Args("list"))["records"].size(), 2u);
    EXPECT_TRUE(rig.helper.ApplyReload());
    EXPECT_EQ(rig.helper.Handle(Rig::Args("list"))["records"].size(), 3u);
}

TEST(RecordsCommandHelper, BadRecordReportsFailingNamingFileRecordFieldAndKeepsPrevious)
{
    Rig rig;
    rig.nextOk = false;
    EXPECT_FALSE(rig.helper.ApplyReload());
    EXPECT_TRUE(rig.failing);
    EXPECT_NE(rig.healthText.find("species.json"), std::string::npos);
    EXPECT_NE(rig.healthText.find("wolf"), std::string::npos);
    EXPECT_NE(rig.healthText.find("diet"), std::string::npos);
    EXPECT_EQ(rig.helper.Handle(Rig::Args("list"))["records"].size(), 2u);
}

TEST(RecordsCommandHelper, GoodReloadAfterBadClearsFailing)
{
    Rig rig;
    rig.nextOk = false;
    rig.helper.ApplyReload();
    rig.nextOk = true;
    EXPECT_TRUE(rig.helper.ApplyReload());
    EXPECT_FALSE(rig.failing);
}

TEST(RecordsCommandHelper, UnwiredSeamsAndBadActionsReplyWithError)
{
    RecordsCommandHelper h("ecology");
    EXPECT_FALSE(h.Handle(Rig::Args("list"))["success"].asBool());
    EXPECT_FALSE(h.Handle(Rig::Args("reload"))["success"].asBool());
    EXPECT_FALSE(h.ApplyReload());
    Rig rig;
    EXPECT_FALSE(rig.helper.Handle(Rig::Args("bogus"))["success"].asBool());
    EXPECT_FALSE(rig.helper.Handle(Json::Value(Json::objectValue))["success"].asBool());
}

TEST(RecordsCommandHelper, RegisteredThroughSimCommandSetAndRolledBackWithTheSet)
{
    Rig rig;
    FakeSimCommandHost host;
    host.failOnNth = 2;
    SimCommandEntry entries[2];
    entries[0] = rig.helper.BuildEntry();
    entries[1] = rig.helper.BuildEntry();
    entries[1].name = "cow.ecology.other";
    SimCommandSet set;
    EXPECT_FALSE(set.RegisterAll(host, entries, 2, "RecordsTest"));
    EXPECT_EQ(host.regCount, 1u);
    EXPECT_EQ(host.unregCount, 1u);
    EXPECT_EQ(set.Count(), 0u);

    host.failOnNth = 0;
    EXPECT_TRUE(set.RegisterAll(host, entries, 1, "RecordsTest"));
    EXPECT_EQ(host.regNames[host.regCount - 1], Dia::Core::StringCRC("cow.ecology.records"));
}

}  // namespace Testing
}  // namespace Debug
}  // namespace CoW
