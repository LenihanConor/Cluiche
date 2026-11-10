#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <Modules/VisualDebuggerModule.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>

#include <string>
#include <vector>

// debug.panel_command: VisualDebuggerModule::HandlePanelCommand validates and enqueues into the same queue the panel's
// DebugPanelCommand stream feeds; DrainPendingCommands (run each tick) dispatches to the domain's OnCommand.

namespace
{
	using Cluiche::AppFlow::VisualDebuggerModule;
	using Dia::Core::StringCRC;
	using Dia::VisualDebugger::IDebugDomain;

	class RecordingDomain : public IDebugDomain
	{
	public:
		StringCRC GetDomainId() const override { return StringCRC("PanelCmdTest"); }
		const char* GetDisplayName() const override { return "PanelCmdTest"; }
		const char* GetDescription() const override { return "fake"; }
		StringCRC GetGroup() const override { return StringCRC("Test"); }
		Dia::Core::RGBA GetAccentColour() const override { return Dia::Core::RGBA(); }
		bool HasWorldDrawers() const override { return false; }
		void GetJSONState(Json::Value& out) override { out = Json::Value(Json::objectValue); }
		void OnCommand(StringCRC cmd, const Json::Value& args) override
		{
			cmds.push_back(cmd);
			argList.push_back(args);
		}
		std::vector<StringCRC> cmds;
		std::vector<Json::Value> argList;
	};

	class TestableVisualDebugger : public VisualDebuggerModule
	{
	public:
		TestableVisualDebugger() : VisualDebuggerModule(StringCRC("VDTest")) {}
		using VisualDebuggerModule::DrainPendingCommands;
		using VisualDebuggerModule::kCommandQueueCapacity;
	};

	Json::Value Req(const char* domain, const char* cmd, const Json::Value* args = nullptr)
	{
		Json::Value v(Json::objectValue);
		if (domain) v["domain"] = domain;
		if (cmd) v["cmd"] = cmd;
		if (args) v["args"] = *args;
		return v;
	}

	struct PanelCommandFixture : ::testing::Test
	{
		TestableVisualDebugger vd;
		RecordingDomain domain;
		void SetUp() override { vd.RegisterDomain(domain); }
		void TearDown() override { vd.UnregisterDomain(domain); }
	};
}

TEST_F(PanelCommandFixture, AcceptedCommandIsDispatchedToDomainOnNextDrainWithArgs)
{
	Json::Value a(Json::objectValue);
	a["key"] = "landFraction";
	a["value"] = 0.5;
	const Json::Value r = vd.HandlePanelCommand(Req("PanelCmdTest", "setScale", &a));
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_TRUE(r["queued"].asBool());
	EXPECT_TRUE(domain.cmds.empty());   // queued, not applied inline

	vd.DrainPendingCommands();
	ASSERT_EQ(domain.cmds.size(), 1u);
	EXPECT_EQ(domain.cmds[0], StringCRC("setScale"));
	EXPECT_EQ(domain.argList[0]["key"].asString(), "landFraction");
	EXPECT_DOUBLE_EQ(domain.argList[0]["value"].asDouble(), 0.5);

	vd.DrainPendingCommands();
	EXPECT_EQ(domain.cmds.size(), 1u);   // drained once
}

TEST_F(PanelCommandFixture, ArgsAreOptionalAndNullArgsAccepted)
{
	EXPECT_TRUE(vd.HandlePanelCommand(Req("PanelCmdTest", "clearCell"))["success"].asBool());
	const Json::Value nul;
	EXPECT_TRUE(vd.HandlePanelCommand(Req("PanelCmdTest", "clearCell", &nul))["success"].asBool());
	vd.DrainPendingCommands();
	ASSERT_EQ(domain.cmds.size(), 2u);
	EXPECT_TRUE(domain.argList[0].isObject() && domain.argList[0].empty());
}

TEST_F(PanelCommandFixture, UnknownDomainRejectedAndNothingQueued)
{
	const Json::Value r = vd.HandlePanelCommand(Req("NoSuchDomain", "setScale"));
	EXPECT_FALSE(r["success"].asBool());
	EXPECT_TRUE(r.isMember("error"));
	vd.DrainPendingCommands();
	EXPECT_TRUE(domain.cmds.empty());
}

TEST_F(PanelCommandFixture, MissingOrWrongTypedFieldsRejected)
{
	EXPECT_FALSE(vd.HandlePanelCommand(Req("PanelCmdTest", nullptr))["success"].asBool());
	EXPECT_FALSE(vd.HandlePanelCommand(Req("PanelCmdTest", ""))["success"].asBool());
	EXPECT_FALSE(vd.HandlePanelCommand(Req(nullptr, "setScale"))["success"].asBool());
	EXPECT_FALSE(vd.HandlePanelCommand(Req("", "setScale"))["success"].asBool());
	EXPECT_FALSE(vd.HandlePanelCommand(Json::Value(Json::nullValue))["success"].asBool());

	Json::Value wrong = Req("PanelCmdTest", "setScale");
	wrong["cmd"] = 5;
	EXPECT_FALSE(vd.HandlePanelCommand(wrong)["success"].asBool());
	wrong = Req("PanelCmdTest", "setScale");
	wrong["domain"] = 7;
	EXPECT_FALSE(vd.HandlePanelCommand(wrong)["success"].asBool());
	const Json::Value badArgs("not an object");
	EXPECT_FALSE(vd.HandlePanelCommand(Req("PanelCmdTest", "setScale", &badArgs))["success"].asBool());

	vd.DrainPendingCommands();
	EXPECT_TRUE(domain.cmds.empty());
}

TEST_F(PanelCommandFixture, OversizedArgsRejected)
{
	Json::Value a(Json::objectValue);
	a["blob"] = std::string(600, 'x');
	const Json::Value r = vd.HandlePanelCommand(Req("PanelCmdTest", "setScale", &a));
	EXPECT_FALSE(r["success"].asBool());
	vd.DrainPendingCommands();
	EXPECT_TRUE(domain.cmds.empty());
}

TEST_F(PanelCommandFixture, QueueFullRejectsExtraThenAcceptsAgainAfterDrain)
{
	const int cap = TestableVisualDebugger::kCommandQueueCapacity;
	for (int i = 0; i < cap; ++i)
		ASSERT_TRUE(vd.HandlePanelCommand(Req("PanelCmdTest", "setScale"))["success"].asBool()) << i;

	const Json::Value full = vd.HandlePanelCommand(Req("PanelCmdTest", "setScale"));
	EXPECT_FALSE(full["success"].asBool());
	EXPECT_TRUE(full.isMember("error"));

	vd.DrainPendingCommands();
	EXPECT_EQ(static_cast<int>(domain.cmds.size()), cap);   // the rejected one was never queued
	EXPECT_TRUE(vd.HandlePanelCommand(Req("PanelCmdTest", "setScale"))["success"].asBool());
}

TEST_F(PanelCommandFixture, WithoutSimQueryHostNothingRegistersAndModuleStillWorks)
{
	EXPECT_FALSE(vd.PanelCommandRegistered());
	EXPECT_TRUE(vd.HandlePanelCommand(Req("PanelCmdTest", "x"))["success"].asBool());
}

#endif // DIA_DEBUG
