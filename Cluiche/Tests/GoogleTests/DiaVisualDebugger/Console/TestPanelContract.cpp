////////////////////////////////////////////////////////////////////////////////
// Filename: TestPanelContract.cpp
// Description: Golden-file contract between the REAL C++ JSON builders and the
//              debug panel page. Builds one deterministic fixture world, produces
//              every JSON payload the page receives, and compares each byte-for-byte
//              with the committed file in Cluiche/Tests/Contracts/debugpanel/.
//              The page tests (CluicheTest/UI/src/test/debugpanel/contract.test.ts)
//              feed the SAME files into debug-panel.html.
//
//              Regenerate (then review the diff):  set DIA_UPDATE_GOLDEN=1
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/ConsoleExecutor.h>
#include <DiaVisualDebugger/Console/ConsoleJson.h>
#include <DiaVisualDebugger/Console/ConsoleLogRing.h>
#include <DiaVisualDebugger/Console/PanelController.h>
#include <DiaVisualDebugger/Console/PanelJson.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace Dia::VisualDebugger;
using namespace Dia::VisualDebugger::Console;
using Dia::Core::StringCRC;
using Dia::Observation::Log::LogLevel;

namespace
{
	// Configurable stub domain. id and display name are independent on purpose.
	class ContractDomain : public IDebugDomain
	{
	public:
		ContractDomain(const char* id, const char* display, const char* desc, const char* group,
					   unsigned char r, unsigned char g, unsigned char b)
			: mId(id), mDisplay(display), mDesc(desc), mGroup(group), mColour(r, g, b, 255) {}

		StringCRC GetDomainId() const override { return mId; }
		const char* GetDisplayName() const override { return mDisplay; }
		const char* GetDescription() const override { return mDesc; }
		StringCRC GetGroup() const override { return mGroup; }
		Dia::Core::RGBA GetAccentColour() const override { return mColour; }
		bool HasWorldDrawers() const override { return false; }
		void OnCommand(StringCRC, const Json::Value&) override {}

		void GetJSONState(Json::Value& out) override
		{
			for (const auto& d : drawers)
			{
				Json::Value v(Json::objectValue);
				v["name"] = d.first;
				v["enabled"] = d.second;
				out["drawers"].append(v);
			}
			for (const auto& s : statsInt)
				out["stats"][s.first] = s.second;
			for (const auto& s : statsStr)
				out["stats"][s.first] = s.second;
			for (const auto& c : controls)
			{
				Json::Value v(Json::objectValue);
				v["name"] = c;
				v["type"] = "button";
				out["controls"].append(v);
			}
		}

		std::vector<std::pair<std::string, bool>> drawers;
		std::vector<std::pair<std::string, int>> statsInt;
		std::vector<std::pair<std::string, std::string>> statsStr;
		std::vector<std::string> controls;

	private:
		StringCRC mId;
		const char* mDisplay;
		const char* mDesc;
		StringCRC mGroup;
		Dia::Core::RGBA mColour;
	};

	bool RegisterContractCommand(const char* name, const char* description, const char* category,
								 const char* example, int exitCode)
	{
		Dia::API::CommandInfo info;
		info.name = StringCRC(name);
		info.description = description;
		info.category = StringCRC(category);
		info.owner = "TestPanelContract";
		info.version = "1.0.0";
		info.example = example;
		info.callback = [exitCode](const Dia::API::CommandArgs&) { return exitCode; };
		return Dia::API::RegisterCommand(info);
	}

	struct Push
	{
		std::string function;
		std::string json;
	};

	class RecordingPusher : public IPanelPusher
	{
	public:
		bool Push(const char* jsFunction, const char* json) override
		{
			pushes.push_back({ jsFunction, json });
			return true;
		}
		std::vector<::Push> pushes;
	};

	// Finds <ancestor of this file>/Contracts/debugpanel, independent of the process cwd.
	std::filesystem::path FindGoldenDir()
	{
		namespace fs = std::filesystem;
		std::error_code ec;
		fs::path dir = fs::absolute(fs::path(__FILE__), ec).parent_path();
		for (int i = 0; i < 12 && !dir.empty(); ++i)
		{
			const fs::path candidate = dir / "Contracts" / "debugpanel";
			if (fs::is_directory(candidate, ec))
				return candidate;
			if (dir == dir.parent_path())
				break;
			dir = dir.parent_path();
		}
		return fs::path();
	}

	bool ReadFile(const std::filesystem::path& p, std::string& out)
	{
		std::ifstream f(p, std::ios::binary);
		if (!f)
			return false;
		std::ostringstream ss;
		ss << f.rdbuf();
		out = ss.str();
		return true;
	}

	ConsoleLogLine MakeLine(uint64_t seq, uint64_t ns, LogLevel level, const char* channel, const char* msg)
	{
		ConsoleLogLine l = {};
		l.sequence = seq;
		l.timestampNs = ns;
		l.level = level;
		l.channel = StringCRC(channel);
		std::strncpy(l.message, msg, ConsoleLogLine::kMaxMessageLength);
		return l;
	}
}

class PanelContractTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Start from a known-empty command registry so commands from other tests never leak in.
		mWasInitialized = Dia::API::IsInitialized();
		if (mWasInitialized)
			Dia::API::Shutdown();
		Dia::API::Initialize();

		ASSERT_TRUE(RegisterContractCommand("debug.layer.enable", "Enable a debug layer", "debug", "debug.layer.enable Entities", 0));
		ASSERT_TRUE(RegisterContractCommand("debug.layer.disable", "Disable a debug layer", "debug", "debug.layer.disable Entities", 0));
		ASSERT_TRUE(RegisterContractCommand("debug.stats.reset", "Reset <b>all</b> stats", "debug", "", 0));
		ASSERT_TRUE(RegisterContractCommand("asset.compile", "Compile \"model\" from C:\\assets\\raw", "asset", "asset.compile model.fbx --format=gltf", 0));
		ASSERT_TRUE(RegisterContractCommand("sim.pause", "Pause the simulation", "sim", "sim.pause", 0));

		mPhysics = new ContractDomain("Physics2D", "Physics2D", "Rigid bodies and contacts", "Physics", 0xE0, 0x70, 0x30);
		mPhysics->drawers = { { "Bodies", true }, { "Contacts", false } };
		mPhysics->statsInt = { { "Bodies", 42 } };
		mPhysics->controls = { "Step" };

		mBus = new ContractDomain("MessageBus", "Message Bus", "Cross-PU message ledger", "CoreDebug", 0x30, 0xA0, 0xE0);
		mBus->drawers = { { "Ledger", false } };
		mBus->statsStr = { { "Queued", "7 msgs" } };

		mNav = new ContractDomain("NavFlowField", "Flow Field", "Navigation flow fields", "Navigation", 0x40, 0xC0, 0x60);
		mNav->drawers = { { "Vectors", true }, { "Costs", true } };
		mNav->statsInt = { { "Fields", 3 } };

		mDomains.Register(*mPhysics);
		mDomains.Register(*mBus);
		mDomains.Register(*mNav);
	}

	void TearDown() override
	{
		mDomains.Clear();
		delete mPhysics;
		delete mBus;
		delete mNav;
		Dia::API::Shutdown();
		if (mWasInitialized)
			Dia::API::Initialize();
	}

	void Check(const char* fileName, const std::string& produced)
	{
		const std::filesystem::path dir = FindGoldenDir();
		ASSERT_FALSE(dir.empty()) << "Golden folder Contracts/debugpanel not found above " << __FILE__;

		const char* update = std::getenv("DIA_UPDATE_GOLDEN");
		const std::filesystem::path file = dir / fileName;
		if (update != nullptr && std::strcmp(update, "1") == 0)
		{
			std::ofstream f(file, std::ios::binary | std::ios::trunc);
			ASSERT_TRUE(f.good()) << "cannot write " << file.string();
			f << produced;
			return;
		}

		std::string expected;
		ASSERT_TRUE(ReadFile(file, expected))
			<< "golden file " << fileName << " is missing (" << file.string()
			<< "); regenerate with DIA_UPDATE_GOLDEN=1 and review the diff";
		EXPECT_EQ(expected, produced)
			<< "golden file " << fileName << " differs from the real builder output; "
			<< "regenerate with DIA_UPDATE_GOLDEN=1 and review the diff";
	}

	std::string Palette(const char* query, int mask)
	{
		PanelController panel;
		RecordingPusher pusher;
		panel.RequestPaletteOpen();
		panel.Process(pusher, &mDomains);
		pusher.pushes.clear();
		panel.RequestPaletteSearch(query, mask);
		panel.Process(pusher, &mDomains);
		EXPECT_EQ(pusher.pushes.size(), 1u);
		return pusher.pushes.empty() ? std::string() : pusher.pushes[0].json;
	}

	std::string Result(uint32_t seq, const char* line)
	{
		const ConsoleResult r = ExecuteConsoleCommand(line);
		char buf[2048];
		EXPECT_TRUE(ConsoleJson::BuildConsoleResult(seq, r.exitCode, static_cast<unsigned>(r.parseError), line, buf, sizeof(buf)));
		return buf;
	}

	bool mWasInitialized = false;
	ContractDomain* mPhysics = nullptr;
	ContractDomain* mBus = nullptr;
	ContractDomain* mNav = nullptr;
	DiaDebugDomainRegistry mDomains;
};

TEST_F(PanelContractTest, DomainStates)
{
	Json::Value root(Json::objectValue);
	BuildDomainStatesRoot(mDomains, root);
	Json::FastWriter writer;
	writer.omitEndingLineFeed();
	Check("domain_states.json", writer.write(root));
}

TEST_F(PanelContractTest, PaletteResults)
{
	Check("palette_results_all.json", Palette("", 3));
	Check("palette_results_commands.json", Palette("debug", 1));
	Check("palette_results_domains.json", Palette("bus", 2));
}

TEST_F(PanelContractTest, ConsoleResults)
{
	Check("console_result_ok.json", Result(1, "debug.layer.enable Entities"));
	Check("console_result_notfound.json", Result(2, "nonexistent.cmd"));
	Check("console_result_parse_error.json", Result(3, "debug.layer.enable \"unterminated"));
}

TEST_F(PanelContractTest, ConsoleCompletionsAndSetInput)
{
	{
		PanelController panel;
		RecordingPusher pusher;
		panel.RequestComplete("debug.");
		panel.Process(pusher, &mDomains);
		ASSERT_EQ(pusher.pushes.size(), 1u);
		EXPECT_EQ(pusher.pushes[0].function, "consoleCompletions");
		Check("console_completions.json", pusher.pushes[0].json);
	}
	{
		PanelController panel;
		RecordingPusher pusher;
		panel.AddToHistory("debug.layer.enable Entities");
		panel.RequestHistory(true);
		panel.RequestHistory(false);
		panel.Process(pusher, &mDomains);
		ASSERT_EQ(pusher.pushes.size(), 2u);
		EXPECT_EQ(pusher.pushes[0].function, "consoleSetInput");
		Check("console_set_input.json", pusher.pushes[0].json);
		Check("console_set_input_restore.json", pusher.pushes[1].json);
	}
}

TEST_F(PanelContractTest, AppendLogLines)
{
	ConsoleLogLine lines[4] = {
		MakeLine(0, 1500000000ull, LogLevel::kDebug, "Debug", "debug detail line"),
		MakeLine(1, 2250000000ull, LogLevel::kInfo, "Boot", "started \"app\" at C:\\games"),
		MakeLine(2, 3000000000ull, LogLevel::kWarning, "Physics", "<b>slow</b> step & more"),
		MakeLine(3, 4125000000ull, LogLevel::kError, "Assets", "failed to load <img src=x onerror=1>"),
	};
	char buf[8192];
	unsigned written = 0;
	ASSERT_TRUE(ConsoleJson::BuildAppendLogLines(lines, 4, buf, sizeof(buf), &written));
	EXPECT_EQ(written, 4u);
	Check("append_log_lines.json", buf);
}

#endif // DIA_DEBUG
