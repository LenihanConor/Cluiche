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
#include <DiaVisualDebugger/Domain/ISchemaKeyResolver.h>
#include <DiaVisualDebugger/Domain/SchemaControl.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/CellPickerTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/ViewValueSourceTestSupport.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaPathfinding/HexTopology.h>

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

	// Fake resolver: every key resolves, schema version 7.
	class FakeResolver : public ISchemaKeyResolver
	{
	public:
		bool Resolves(StringCRC) const override { return true; }
		unsigned int Version() const override { return 7; }
	};

	// Domain whose controls are built through the REAL SchemaControl helper.
	class SchemaContractDomain : public ContractDomain
	{
	public:
		SchemaContractDomain()
			: ContractDomain("CowEcology", "Ecology", "Ecology tuning", "Navigation", 0x60, 0xB0, 0x40) {}

		void GetJSONState(Json::Value& out) override
		{
			FakeResolver resolver;
			SchemaControl sc(&resolver);
			Json::Value& controls = out["controls"];
			sc.Add(controls, "Grazing rate", StringCRC("cow.ecology_params/grazing.rate"), 0.75f, "lush");
			sc.Add(controls, "Growth rate", StringCRC("cow.ecology_params/growth.rate"), 0.25f);
			Json::Value plain(Json::objectValue);
			plain["name"] = "Step";
			plain["type"] = "button";
			controls.append(plain);
			sc.AddSchemaVersion(out);
		}
	};

	// Fixture for the REAL CellLayerViewerDomain (DiaCellLayers Registry-and-Viewer, panel card).
	struct ViewerDraw : public Dia::Core::IDebugDraw
	{
		Dia::Maths::Vector2D mouse;
		void RequestDraw(const Dia::Maths::Vector2D&, float, Dia::Core::RGBA, Dia::Core::RGBA) override {}
		void RequestDraw(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, Dia::Core::RGBA) override {}
		void RequestDrawPoint(const Dia::Maths::Vector2D&, Dia::Core::RGBA) override {}
		void RequestDrawRect(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, Dia::Core::RGBA, Dia::Core::RGBA) override {}
		void RequestDrawArc(const Dia::Maths::Vector2D&, float, float, float, Dia::Core::RGBA) override {}
		void RequestDrawRay(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, float, Dia::Core::RGBA) override {}
		void RequestDraw(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, Dia::Core::RGBA, Dia::Core::RGBA) override {}
		void RequestDrawText(const Dia::Maths::Vector2D&, const char*, float, Dia::Core::RGBA) override {}
		void RequestDrawLine3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, Dia::Core::RGBA) override {}
		void RequestDrawRay3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, float, Dia::Core::RGBA) override {}
		void RequestDrawBox3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, Dia::Core::RGBA) override {}
		void RequestDrawSphere3D(const Dia::Maths::Vector3D&, float, Dia::Core::RGBA) override {}
		void RequestDrawArrow3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, float, float, Dia::Core::RGBA) override {}
		void RequestDrawCellField(const Dia::Core::CellFieldDrawRequest&) override {}
		uint32_t DroppedCount() const override { return 0; }
		const Dia::Maths::Vector2D& GetMousePixel() const override { return mouse; }
	};

	Dia::CellLayers::CellPickerView ViewerView()
	{
		Dia::CellLayers::CellPickerView v;
		v.viewportWidth = 1280;
		v.viewportHeight = 720;
		return v;
	}

	void ViewerPointAt(ViewerDraw& draw, int q, int r)
	{
		const Dia::Maths::Vector2D c = Dia::Pathfinding::HexTopology::CellCenter(Dia::Pathfinding::CellCoord{ q, r }, 1.0f);
		float sx, sy;
		Dia::CellLayers::Testing::WorldToScreen(ViewerView(), c.X(), c.Y(), sx, sy);
		draw.mouse = Dia::Maths::Vector2D(sx, sy);
	}

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

	// Every domain state carries its registry id (the page addresses commands by id, keyed by display name).
	ASSERT_TRUE(root.isMember("Message Bus"));
	EXPECT_EQ(root["Message Bus"]["_id"].asString(), "MessageBus");
	EXPECT_EQ(root["Flow Field"]["_id"].asString(), "NavFlowField");
	EXPECT_EQ(root["Physics2D"]["_id"].asString(), "Physics2D");
}

TEST_F(PanelContractTest, DomainStatesWithSchemaKeys)
{
	// Separate registry so domain_states.json stays byte-identical.
	DiaDebugDomainRegistry registry;
	SchemaContractDomain eco;
	registry.Register(eco);
	Json::Value root(Json::objectValue);
	BuildDomainStatesRoot(registry, root);
	Json::FastWriter writer;
	writer.omitEndingLineFeed();
	Check("domain_states_schema.json", writer.write(root));
	registry.Clear();
}

// DiaCellLayers viewer card (AC 16, 17): the REAL CellLayerViewerDomain state through the REAL BuildDomainStatesRoot.
namespace
{
	struct ViewerRig
	{
		Dia::Graphics::CellFieldRegistry cellField;
		Dia::CellLayers::Testing::UploaderSchemaFixture fx;
		Dia::CellLayers::Testing::TestGrid gridA{ 4, 3 };
		Dia::CellLayers::Testing::TestGrid gridB{ 5, 2 };
		Dia::CellLayers::Testing::StubProvider biome, temp, rain, soil;
		ViewerDraw draw;
		Dia::CellLayers::CellLayerRegistry* registry = nullptr;
		Dia::CellLayers::LayerUploader* uploader = nullptr;
		Dia::CellLayers::CellLayerViewerDomain* domain = nullptr;

		static Dia::ValueSchema::ValueInfo Float(const char* key, bool defaultBase, const char* unit, const char* desc, const char* label, const char* hotkey = nullptr)
		{
			auto i = Dia::CellLayers::Testing::UploaderSchemaFixture::FloatEntry(key, -10.0, 10.0);
			if (defaultBase) { i.mUi["default_base"] = true; }
			if (hotkey) { i.mUi["hotkey"] = hotkey; }
			Dia::ValueSchema::SetText(i.mUnit, unit);
			Dia::ValueSchema::SetText(i.mDescription, desc);
			Dia::ValueSchema::SetText(i.mLabel, label);
			return i;
		}

		void Build(bool withSchema)
		{
			using namespace Dia::CellLayers;
			Testing::FakePuId() = 1;
			biome.values = { 0, 1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
			temp.values = { 5, -2, 7, 3, 1, 0, 4, 9, 2, 2, 2, 2 };
			rain.values = std::vector<float>(12, 1.0f);
			soil.values = std::vector<float>(10, 3.0f);
			const Dia::ValueSchema::ValueSchemaService* svc = nullptr;
			if (withSchema)
			{
				fx.Add(Float("temp", true, "C", "Air temperature", "Temperature", "t"));
				fx.Add(Float("rain", false, "mm", "Rainfall", "Rainfall"));
				fx.Add(Float("soil", false, "kg", "Soil mass", "Soil"));
				auto b = Testing::UploaderSchemaFixture::EnumEntry("biome", 3);
				b.mUi["hotkey"] = "b";
				Dia::ValueSchema::SetText(b.mLabel, "Biome");
				Dia::ValueSchema::SetText(b.mDescription, "Dominant biome");
				fx.Add(b);
				svc = &fx.Publish();
			}
			registry = new CellLayerRegistry(svc, &Testing::FakePuIdFn);
			uploader = new LayerUploader(cellField, svc);
			domain = new CellLayerViewerDomain(*registry, *uploader, svc, CellLayerViewerDomain::ViewFn(&ViewerView));
		}

		void Add(Dia::CellLayers::CellLayerDesc d, const char* group)
		{
			d.group.Append(group);
			EXPECT_EQ(registry->Register(d), Dia::CellLayers::CellRegisterResult::Ok);
		}

		~ViewerRig()
		{
			delete domain;
			delete uploader;
			delete registry;
		}
	};

	std::string ViewerJson(Dia::CellLayers::CellLayerViewerDomain& domain)
	{
		DiaDebugDomainRegistry registry;
		registry.Register(domain);
		Json::Value root(Json::objectValue);
		BuildDomainStatesRoot(registry, root);
		Json::FastWriter writer;
		writer.omitEndingLineFeed();
		const std::string out = writer.write(root);
		registry.Clear();
		return out;
	}
}

TEST_F(PanelContractTest, CellLayersViewerFloatBase)
{
	using namespace Dia::CellLayers;
	ViewerRig rig;
	rig.Build(true);
	rig.Add(rig.biome.MakeDesc("biome", &rig.gridA.desc, CellValueKind::Category, "cells/biome"), "Terrain");
	rig.Add(rig.temp.MakeDesc("temp", &rig.gridA.desc, CellValueKind::Float, "cells/temp"), "Ecology");
	rig.Add(rig.rain.MakeDesc("rain", &rig.gridA.desc, CellValueKind::Float, "cells/rain"), "Ecology");
	ASSERT_EQ(rig.registry->RegisterOverlay(Testing::MakeOverlay("wind", "Wind arrows", "Weather")), CellRegisterResult::Ok);
	ASSERT_EQ(rig.registry->RegisterOverlay(Testing::MakeOverlay("edges", "Region edges", "Terrain")), CellRegisterResult::Ok);
	ASSERT_TRUE(rig.domain->ToggleOverlay(StringCRC("wind")));
	ViewerPointAt(rig.draw, 2, 1);
	rig.domain->Update(rig.draw);
	const std::string json = ViewerJson(*rig.domain);
	Check("domain_states_celllayers.json", json);

	Json::Value parsed;
	ASSERT_TRUE(Json::Reader().parse(json, parsed));
	const Json::Value& s = parsed["Cell Layers"];
	EXPECT_EQ(s["_id"].asString(), "celllayers");
	EXPECT_FALSE(s.isMember("domainId"));	// replaced by the generic _id
	EXPECT_FALSE(s.isMember("noData"));
	std::string tempDesc, rainDesc;
	for (const Json::Value& l : s["layers"])
	{
		if (l["id"].asString() == "temp") { tempDesc = l["description"].asString(); }
		if (l["id"].asString() == "rain") { rainDesc = l["description"].asString(); }
	}
	EXPECT_EQ(tempDesc, "Air temperature");
	EXPECT_EQ(rainDesc, "Rainfall");
}

TEST_F(PanelContractTest, CellLayersViewerCategoryBaseMultiGrid)
{
	using namespace Dia::CellLayers;
	ViewerRig rig;
	rig.Build(true);
	rig.Add(rig.biome.MakeDesc("biome", &rig.gridA.desc, CellValueKind::Category, "cells/biome"), "Terrain");
	rig.Add(rig.temp.MakeDesc("temp", &rig.gridA.desc, CellValueKind::Float, "cells/temp"), "Ecology");
	rig.Add(rig.soil.MakeDesc("soil", &rig.gridB.desc, CellValueKind::Float, "cells/soil"), "Ecology");
	ASSERT_TRUE(rig.domain->SetBase(StringCRC("biome")));
	ViewerPointAt(rig.draw, 2, 0);
	rig.domain->Update(rig.draw);
	Check("domain_states_celllayers_category.json", ViewerJson(*rig.domain));
}

TEST_F(PanelContractTest, CellLayersViewerSelection)
{
	using namespace Dia::CellLayers;
	ViewerRig rig;
	rig.Build(true);
	rig.Add(rig.biome.MakeDesc("biome", &rig.gridA.desc, CellValueKind::Category, "cells/biome"), "Terrain");
	rig.Add(rig.temp.MakeDesc("temp", &rig.gridA.desc, CellValueKind::Float, "cells/temp"), "Ecology");
	ASSERT_TRUE(rig.domain->SetBase(StringCRC("temp")));
	ViewerPointAt(rig.draw, 2, 1);
	rig.domain->Update(rig.draw);
	ASSERT_NE(rig.domain->ActiveSelection(), nullptr);
	EXPECT_TRUE(SelectionTools::SelectBox(*rig.domain->ActiveSelection(), 0, 0, 1, 1).ok);
	rig.domain->Input().SetTool(SelectionInput::Tool::Pan);
	rig.domain->Update(rig.draw);
	const std::string json = ViewerJson(*rig.domain);
	Check("domain_states_celllayers_selection.json", json);

	Json::Value parsed;
	ASSERT_TRUE(Json::Reader().parse(json, parsed));
	const Json::Value& sel = parsed["Cell Layers"]["selection"];
	EXPECT_EQ(sel["count"].asUInt(), 4u);
	EXPECT_EQ(sel["tool"].asString(), "pan");
	EXPECT_EQ(sel["group"].asString(), "biome");
	EXPECT_FALSE(sel["tooLarge"].asBool());
}

TEST_F(PanelContractTest, CellLayersViewerInspectorPinnedSingleCell)
{
	using namespace Dia::CellLayers;
	ViewerRig rig;
	rig.Build(true);
	rig.Add(rig.biome.MakeDesc("biome", &rig.gridA.desc, CellValueKind::Category, "cells/biome"), "Terrain");
	rig.Add(rig.temp.MakeDesc("temp", &rig.gridA.desc, CellValueKind::Float, "cells/temp"), "Ecology");
	rig.Add(rig.rain.MakeDesc("rain", &rig.gridA.desc, CellValueKind::Float, "cells/rain"), "Ecology");
	ViewerPointAt(rig.draw, 2, 1);
	rig.domain->Update(rig.draw);
	ASSERT_NE(rig.domain->ActiveSelection(), nullptr);
	EXPECT_TRUE(SelectionTools::SelectBox(*rig.domain->ActiveSelection(), 0, 0, 1, 1).ok);
	for (int i = 0; i < 3; ++i) { rig.domain->Update(rig.draw); }
	Json::Value freeze(Json::objectValue);
	freeze["freeze"] = true;
	rig.domain->OnCommand(StringCRC("select_pin"), freeze);
	EXPECT_TRUE(SelectionTools::SelectCell(*rig.domain->ActiveSelection(), 2, 1).ok);
	for (int i = 0; i < 3; ++i) { rig.domain->Update(rig.draw); }
	const std::string json = ViewerJson(*rig.domain);
	Check("domain_states_celllayers_inspector.json", json);

	Json::Value parsed;
	ASSERT_TRUE(Json::Reader().parse(json, parsed));
	const Json::Value& insp = parsed["Cell Layers"]["inspector"];
	EXPECT_TRUE(insp["hasSelection"].asBool());
	EXPECT_TRUE(insp["pinned"]["frozen"].asBool());
	EXPECT_EQ(insp["pinned"]["count"].asUInt(), 4u);
	EXPECT_EQ(insp["cell"]["q"].asUInt(), 2u);
	EXPECT_EQ(insp["cell"]["r"].asUInt(), 1u);
}

TEST_F(PanelContractTest, CellLayersViewerEmptyState)
{
	ViewerRig rig;
	rig.Build(false);
	rig.domain->Update(rig.draw);
	Check("domain_states_celllayers_empty.json", ViewerJson(*rig.domain));
}

TEST_F(PanelContractTest, CellLayersViewerNoData)
{
	using namespace Dia::CellLayers;
	Testing::StubViewValueSource source;	// declared first: outlives the rig
	ViewerRig rig;
	rig.Build(true);
	rig.Add(rig.temp.MakeDesc("temp", &rig.gridA.desc, CellValueKind::Float, "cells/temp"), "Ecology");
	rig.Add(rig.rain.MakeDesc("rain", &rig.gridA.desc, CellValueKind::Float, "cells/rain"), "Ecology");
	source.live = false;
	source.hasData = false;
	ASSERT_TRUE(rig.registry->SetViewValueSource(&source));
	ViewerPointAt(rig.draw, 2, 1);
	rig.domain->Update(rig.draw);
	const std::string json = ViewerJson(*rig.domain);
	Check("domain_states_celllayers_nodata.json", json);

	Json::Value parsed;
	ASSERT_TRUE(Json::Reader().parse(json, parsed));
	const Json::Value& s = parsed["Cell Layers"];
	EXPECT_TRUE(s["noData"].asBool());
	EXPECT_FALSE(s["empty"].asBool());
	EXPECT_FALSE(s["legend"]["available"].asBool());
	EXPECT_FALSE(s["hover"]["valid"].asBool());
}

TEST_F(PanelContractTest, PaletteResults)
{
	Check("palette_results_all.json", Palette("", 3));
	Check("palette_results_commands.json", Palette("debug", 1));
	Check("palette_results_domains.json", Palette("bus", 2));
}

TEST_F(PanelContractTest, PaletteResultsCommandWithParams)
{
	// Registered here (not in SetUp) so the existing all/commands/domains goldens stay byte-identical.
	static const Dia::API::ValueParam kParams[] = {
		{ "target", "string", true, "Entity <name> to spawn", nullptr, false, 0.0f, 0.0f, nullptr },
		{ "radius", "float", false, "Spawn radius", "m", true, 0.5f, 10.0f, nullptr },
		{ "mode", "enum", false, nullptr, nullptr, false, 0.0f, 0.0f, "fast|slow" },
	};
	Dia::API::CommandInfo info;
	info.name = StringCRC("zworld.spawn");
	info.description = "Spawn an entity";
	info.category = StringCRC("zworld");
	info.owner = "TestPanelContract";
	info.version = "1.0.0";
	info.example = "";
	info.params = kParams;
	info.paramCount = 3;
	info.callback = [](const Dia::API::CommandArgs&) { return 0; };
	ASSERT_TRUE(Dia::API::RegisterCommand(info));

	Check("palette_results_commands_with_params.json", Palette("zworld", 1));
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
