////////////////////////////////////////////////////////////////////////////////
// Filename: TestPanelController.cpp
// Description: Unit tests for PanelController. Pins the rule that JS callbacks
//              (Request*) never push to the page; only Process() does.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/PanelController.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>
#include <string>
#include <vector>

using namespace Dia::VisualDebugger;
using namespace Dia::VisualDebugger::Console;
using Dia::Core::StringCRC;

namespace
{
	// Display name differs from the domain id on purpose: the panel card key is the display name.
	class StubPanelDomain : public IDebugDomain
	{
	public:
		StringCRC GetDomainId() const override { return StringCRC("PanelCtlStubDomainId"); }
		const char* GetDisplayName() const override { return "Panel Ctl Stub"; }
		const char* GetDescription() const override { return "Stub domain for panel controller tests"; }
		StringCRC GetGroup() const override { return StringCRC("CoreDebug"); }
		Dia::Core::RGBA GetAccentColour() const override { return Dia::Core::RGBA(); }
		bool HasWorldDrawers() const override { return false; }
		void GetJSONState(Json::Value&) override {}
		void OnCommand(StringCRC, const Json::Value&) override {}
	};

	struct Push
	{
		std::string function;
		std::string json;
	};

	class FakePusher : public IPanelPusher
	{
	public:
		bool Push(const char* jsFunction, const char* json) override
		{
			if (insideCallback)
				ADD_FAILURE() << "Push called while inside a JS callback: " << jsFunction;
			pushes.push_back({ jsFunction, json });
			return true;
		}

		bool insideCallback = false;
		std::vector<::Push> pushes;
	};

	bool Parse(const std::string& text, Json::Value& root)
	{
		Json::Reader reader;
		return reader.parse(text, root);
	}

	bool RegisterCtlCommand(const char* name)
	{
		Dia::API::CommandInfo info;
		info.name = StringCRC(name);
		info.description = "panel controller test command";
		info.category = StringCRC("ctl");
		info.owner = "TestPanelController";
		info.version = "1.0.0";
		info.example = "";
		info.callback = [](const Dia::API::CommandArgs&) { return 0; };
		return Dia::API::RegisterCommand(info);
	}

	const char* kLongName = "ctl_long_qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq";	// 63 chars
}

class PanelControllerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!Dia::API::IsInitialized())
		{
			Dia::API::Initialize();
			mInitializedHere = true;
		}
		ASSERT_TRUE(RegisterCtlCommand("ctl_alpha"));
		ASSERT_TRUE(RegisterCtlCommand("ctl_beta"));
		ASSERT_EQ(std::strlen(kLongName), 63u);
		ASSERT_TRUE(RegisterCtlCommand(kLongName));
		mDomains.Register(mDomain);
	}

	void TearDown() override
	{
		mDomains.Clear();
		Dia::API::UnregisterCommand(StringCRC("ctl_alpha"));
		Dia::API::UnregisterCommand(StringCRC("ctl_beta"));
		Dia::API::UnregisterCommand(StringCRC(kLongName));
		if (mInitializedHere)
			Dia::API::Shutdown();
	}

	// Calls every Request* method several times with the pusher flagged as "inside a JS callback".
	void CallAllRequests()
	{
		mPusher.insideCallback = true;
		for (int i = 0; i < 3; ++i)
		{
			mPanel.RequestComplete("ctl_");
			mPanel.RequestHistory(true);
			mPanel.RequestHistory(false);
			mPanel.RequestPaletteOpen();
			mPanel.RequestPaletteSearch("ctl", 3);
			mPanel.RequestRejectedLine(7u + i);
		}
		mPusher.insideCallback = false;
	}

	bool mInitializedHere = false;
	StubPanelDomain mDomain;
	DiaDebugDomainRegistry mDomains;
	PanelController mPanel;
	FakePusher mPusher;
};

TEST_F(PanelControllerTest, RequestsNeverPushFromInsideACallback)
{
	mPanel.AddToHistory("one");
	CallAllRequests();
	EXPECT_EQ(mPusher.pushes.size(), 0u);

	mPanel.Process(mPusher, &mDomains);
	EXPECT_GT(mPusher.pushes.size(), 0u);
}

TEST_F(PanelControllerTest, ProcessWithNothingPendingPushesNothing)
{
	mPanel.Process(mPusher, &mDomains);
	EXPECT_EQ(mPusher.pushes.size(), 0u);
}

TEST_F(PanelControllerTest, ProcessConsumesRequestsOnlyOnce)
{
	CallAllRequests();
	mPanel.Process(mPusher, &mDomains);
	const size_t first = mPusher.pushes.size();
	EXPECT_GT(first, 0u);
	mPanel.Process(mPusher, &mDomains);
	EXPECT_EQ(mPusher.pushes.size(), first);
}

TEST_F(PanelControllerTest, HistoryRequestsPushInOrderThenRestoreDraft)
{
	mPanel.AddToHistory("one");
	mPanel.AddToHistory("two");
	mPanel.RequestHistory(true);
	mPanel.RequestHistory(true);
	mPanel.RequestHistory(false);
	mPanel.RequestHistory(false);
	mPanel.Process(mPusher, &mDomains);

	ASSERT_EQ(mPusher.pushes.size(), 4u);
	const char* expectedText[4] = { "two", "one", "two", "" };
	const bool expectedRestore[4] = { false, false, false, true };
	for (int i = 0; i < 4; ++i)
	{
		EXPECT_EQ(mPusher.pushes[i].function, "consoleSetInput");
		Json::Value v;
		ASSERT_TRUE(Parse(mPusher.pushes[i].json, v));
		EXPECT_EQ(v["text"].asString(), expectedText[i]);
		EXPECT_EQ(v["restore"].asBool(), expectedRestore[i]);
	}
}

TEST_F(PanelControllerTest, PreviousOnEmptyHistoryPushesNothing)
{
	mPanel.RequestHistory(true);
	mPanel.Process(mPusher, &mDomains);
	EXPECT_EQ(mPusher.pushes.size(), 0u);
}

TEST_F(PanelControllerTest, HistoryRequestsAreCappedAtEightPerFrame)
{
	mPanel.AddToHistory("one");
	mPanel.AddToHistory("two");
	for (int i = 0; i < 20; ++i)
		mPanel.RequestHistory(true);
	mPanel.Process(mPusher, &mDomains);
	EXPECT_EQ(mPusher.pushes.size(), 8u);
}

TEST_F(PanelControllerTest, CompletionPushesLiveCommands)
{
	mPanel.RequestComplete("ctl_");
	mPanel.Process(mPusher, &mDomains);
	ASSERT_EQ(mPusher.pushes.size(), 1u);
	EXPECT_EQ(mPusher.pushes[0].function, "consoleCompletions");
	EXPECT_NE(mPusher.pushes[0].json.find("ctl_alpha"), std::string::npos);
	EXPECT_NE(mPusher.pushes[0].json.find("ctl_beta"), std::string::npos);

	Dia::API::UnregisterCommand(StringCRC("ctl_beta"));
	mPanel.RequestComplete("ctl_");
	mPanel.Process(mPusher, &mDomains);
	ASSERT_EQ(mPusher.pushes.size(), 2u);
	EXPECT_NE(mPusher.pushes[1].json.find("ctl_alpha"), std::string::npos);
	EXPECT_EQ(mPusher.pushes[1].json.find("ctl_beta"), std::string::npos);
}

TEST_F(PanelControllerTest, LatestCompletionRequestWins)
{
	mPanel.RequestComplete("ctl_a");
	mPanel.RequestComplete("ctl_b");
	mPanel.Process(mPusher, &mDomains);
	ASSERT_EQ(mPusher.pushes.size(), 1u);
	EXPECT_NE(mPusher.pushes[0].json.find("ctl_beta"), std::string::npos);
	EXPECT_EQ(mPusher.pushes[0].json.find("ctl_alpha"), std::string::npos);
}

TEST_F(PanelControllerTest, CompletionTokenIsTruncatedAtSixtyThreeCharacters)
{
	// A token with a space completes to nothing. Here the space sits at index 63, so it only
	// disappears (and the 63-char command only matches) if the token is cut to 63 characters.
	const std::string token = std::string(kLongName) + " tail";
	mPanel.RequestComplete(token.c_str());
	mPanel.Process(mPusher, &mDomains);
	ASSERT_EQ(mPusher.pushes.size(), 1u);
	Json::Value v;
	ASSERT_TRUE(Parse(mPusher.pushes[0].json, v));
	ASSERT_EQ(v["items"].size(), 1u);
	EXPECT_EQ(v["items"][0u]["name"].asString(), kLongName);
}

TEST_F(PanelControllerTest, PaletteSearchBeforeOpenIsIgnored)
{
	mPanel.RequestPaletteSearch("ctl", 3);
	mPanel.Process(mPusher, &mDomains);
	EXPECT_EQ(mPusher.pushes.size(), 0u);
}

TEST_F(PanelControllerTest, PaletteOpenPushesFullListThenSearchPushesLast)
{
	// Open then search in separate frames, search after open.
	mPanel.RequestPaletteOpen();
	mPanel.Process(mPusher, &mDomains);
	ASSERT_EQ(mPusher.pushes.size(), 1u);
	EXPECT_EQ(mPusher.pushes[0].function, "paletteResults");

	mPanel.RequestPaletteSearch("ctl_alpha", 1);
	mPanel.Process(mPusher, &mDomains);
	ASSERT_EQ(mPusher.pushes.size(), 2u);
	EXPECT_EQ(mPusher.pushes[1].function, "paletteResults");
	Json::Value v;
	ASSERT_TRUE(Parse(mPusher.pushes[1].json, v));
	EXPECT_EQ(v["items"][0u]["name"].asString(), "ctl_alpha");
}

TEST_F(PanelControllerTest, SearchRequestedAfterOpenInSameFrameIsPushedLast)
{
	mPanel.RequestPaletteOpen();
	mPanel.RequestPaletteSearch("ctl_alpha", 1);
	mPanel.Process(mPusher, &mDomains);
	ASSERT_EQ(mPusher.pushes.size(), 2u);
	Json::Value openJson, searchJson;
	ASSERT_TRUE(Parse(mPusher.pushes[0].json, openJson));
	ASSERT_TRUE(Parse(mPusher.pushes[1].json, searchJson));
	EXPECT_GT(openJson["total"].asInt(), searchJson["total"].asInt());	// full list first, narrowed search last
	EXPECT_EQ(searchJson["items"][0u]["name"].asString(), "ctl_alpha");
}

TEST_F(PanelControllerTest, OpenDiscardsOlderPendingSearch)
{
	mPanel.RequestPaletteOpen();
	mPanel.Process(mPusher, &mDomains);
	mPusher.pushes.clear();

	mPanel.RequestPaletteSearch("ctl_alpha", 1);
	mPanel.RequestPaletteOpen();
	mPanel.Process(mPusher, &mDomains);
	ASSERT_EQ(mPusher.pushes.size(), 1u);	// only the open's full list
	Json::Value v;
	ASSERT_TRUE(Parse(mPusher.pushes[0].json, v));
	EXPECT_GT(v["total"].asInt(), 1);
}

TEST_F(PanelControllerTest, PaletteSearchForwardsTypeMask)
{
	mPanel.RequestPaletteOpen();
	mPanel.Process(mPusher, &mDomains);
	mPusher.pushes.clear();

	mPanel.RequestPaletteSearch("", 2);	// domains only
	mPanel.Process(mPusher, &mDomains);
	ASSERT_EQ(mPusher.pushes.size(), 1u);
	Json::Value v;
	ASSERT_TRUE(Parse(mPusher.pushes[0].json, v));
	ASSERT_GE(v["items"].size(), 1u);
	for (Json::ArrayIndex i = 0; i < v["items"].size(); ++i)
		EXPECT_EQ(v["items"][i]["type"].asString(), "domain");
}

TEST_F(PanelControllerTest, NullRegistryStillPushesValidPaletteResults)
{
	mPanel.RequestPaletteOpen();
	mPanel.Process(mPusher, nullptr);
	ASSERT_EQ(mPusher.pushes.size(), 1u);
	EXPECT_EQ(mPusher.pushes[0].function, "paletteResults");
	Json::Value v;
	ASSERT_TRUE(Parse(mPusher.pushes[0].json, v));
	EXPECT_TRUE(v.isMember("items"));
	EXPECT_TRUE(v.isMember("counts"));
}

TEST_F(PanelControllerTest, DomainIsRevealedByDisplayNameNotDomainId)
{
	mPanel.RequestPaletteOpen();
	mPanel.Process(mPusher, &mDomains);
	mPusher.pushes.clear();

	mPanel.RequestPaletteSearch("Panel Ctl Stub", 2);
	mPanel.Process(mPusher, &mDomains);
	ASSERT_EQ(mPusher.pushes.size(), 1u);
	Json::Value v;
	ASSERT_TRUE(Parse(mPusher.pushes[0].json, v));
	ASSERT_GE(v["items"].size(), 1u);
	const Json::Value& item = v["items"][0u];
	EXPECT_EQ(item["action"].asString(), "reveal");
	EXPECT_EQ(item["actionText"].asString(), "Panel Ctl Stub");
	EXPECT_EQ(item["name"].asString(), "Panel Ctl Stub");
}

TEST_F(PanelControllerTest, RejectedLinesPushTooLongResultWithSeq)
{
	mPanel.RequestRejectedLine(41);
	mPanel.RequestRejectedLine(42);
	mPanel.Process(mPusher, &mDomains);
	ASSERT_EQ(mPusher.pushes.size(), 2u);
	const unsigned expectedSeq[2] = { 41, 42 };
	for (int i = 0; i < 2; ++i)
	{
		EXPECT_EQ(mPusher.pushes[i].function, "consoleResult");
		Json::Value v;
		ASSERT_TRUE(Parse(mPusher.pushes[i].json, v));
		EXPECT_EQ(v["seq"].asUInt(), expectedSeq[i]);
		EXPECT_EQ(v["exitCode"].asInt(), 2);
		EXPECT_EQ(v["text"].asString(), "line too long");
	}
}

TEST_F(PanelControllerTest, RejectedLinesAreCappedAtFourPerFrame)
{
	for (unsigned i = 0; i < 6; ++i)
		mPanel.RequestRejectedLine(100 + i);
	mPanel.Process(mPusher, &mDomains);
	ASSERT_EQ(mPusher.pushes.size(), 4u);
	Json::Value v;
	ASSERT_TRUE(Parse(mPusher.pushes[3].json, v));
	EXPECT_EQ(v["seq"].asUInt(), 103u);
}

TEST_F(PanelControllerTest, ResetClearsPendingRequests)
{
	mPanel.AddToHistory("one");
	CallAllRequests();
	mPanel.Reset();
	mPanel.Process(mPusher, &mDomains);
	EXPECT_EQ(mPusher.pushes.size(), 0u);
}

TEST_F(PanelControllerTest, ResetDropsPaletteIndexSoSearchIsIgnoredUntilReopened)
{
	mPanel.RequestPaletteOpen();
	mPanel.Process(mPusher, &mDomains);
	mPusher.pushes.clear();

	mPanel.Reset();
	mPanel.RequestPaletteSearch("ctl", 3);
	mPanel.Process(mPusher, &mDomains);
	EXPECT_EQ(mPusher.pushes.size(), 0u);
}

#endif // DIA_DEBUG
