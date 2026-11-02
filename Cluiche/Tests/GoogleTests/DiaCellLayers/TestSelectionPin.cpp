#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/Testing/ViewerCommands.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaAPI/DiaAPI.h>

#include <sstream>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

// cells.select.pin (optional {"freeze": bool}) and cells.select.unpin act on the viewer's InspectorState.

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;
typedef LogCapture::Level Level;

namespace
{
	struct Rig
	{
		TestGrid grid;
		CellFieldRegistry cellField;
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		StubProvider terrain, temp;
		FakeDebugDraw draw;
		CellLayerRegistry registry;
		LayerUploader uploader;
		CellLayerViewerDomain domain;
		ViewerCommands commands{ domain };

		static const Dia::ValueSchema::ValueSchemaService* Build(UploaderSchemaFixture& f)
		{
			Dia::ValueSchema::ValueInfo t = Dia::ValueSchema::Testing::MakeValueInfo("cells", "terrain");
			t.mType = Dia::ValueSchema::ValueType::Enum;
			t.mEnumValues.Reserve(2);
			for (const char* id : { "grass", "water" })
			{
				Dia::ValueSchema::EnumValue v;
				Dia::ValueSchema::SetText(v.mId, id);
				t.mEnumValues.Add(v);
			}
			f.Add(t);
			f.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10));
			return &f.Publish();
		}

		Rig(uint16_t w, uint16_t h)
			: grid(w, h)
			, service(Build(fx))
			, registry(nullptr, &FakePuIdFn)
			, uploader(cellField, nullptr)
			, domain(registry, uploader, service)
		{
			terrain.values.resize(static_cast<size_t>(w) * h);
			for (size_t i = 0; i < terrain.values.size(); ++i) { terrain.values[i] = static_cast<float>(i % 2); }
			temp.values = std::vector<float>(static_cast<size_t>(w) * h, 1.0f);
			registry.Register(terrain.MakeDesc("terrain", &grid.desc, CellValueKind::Category, "cells/terrain"));
			registry.Register(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
			domain.Update(draw);
		}

		void Settle() { for (int i = 0; i < 3; ++i) { domain.Update(draw); } }
	};

	Json::Value Parse(const char* text)
	{
		Json::Value v;
		Json::CharReaderBuilder b;
		std::istringstream in(text);
		std::string errs;
		Json::parseFromStream(b, in, &v, &errs);
		return v;
	}

	Json::Value Exec(const char* cmd, const char* params) { return ViewerCommands::Exec(cmd, Parse(params)); }

	struct SelectionPinTest : ::testing::Test
	{
		void SetUp() override { FakePuId() = 1; Dia::API::Initialize(); }
		void TearDown() override { Dia::API::Shutdown(); }
	};
}

TEST_F(SelectionPinTest, PinStoresCurrentSelectionAsA)
{
	Rig rig(20, 20);
	Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":2,\"r1\":2}");
	const Json::Value r = Exec("cells.select.pin", "{}");
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_TRUE(r["data"]["ok"].asBool());
	EXPECT_EQ(r["data"]["count"].asUInt(), 9u);
	EXPECT_FALSE(r["data"]["frozen"].asBool());
	EXPECT_TRUE(rig.domain.Inspector().IsPinned());
	EXPECT_FALSE(rig.domain.Inspector().IsFrozen());
	EXPECT_EQ(rig.domain.Inspector().PinnedCount(), 9u);
	Exec("cells.select.clear", "{}");
	Exec("cells.select.cell", "{\"q\":5,\"r\":5}");
	rig.Settle();
	ASSERT_TRUE(rig.domain.Inspector().StatsForA(StringCRC("temp")) != nullptr);
	EXPECT_EQ(rig.domain.Inspector().StatsForA(StringCRC("temp"))->count, 9u);
	EXPECT_EQ(rig.domain.Inspector().StatsFor(StringCRC("temp"))->count, 1u);
}

TEST_F(SelectionPinTest, PinFreezeStoresEveryLayer)
{
	Rig rig(20, 20);
	Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":1,\"r1\":1}");
	const Json::Value r = Exec("cells.select.pin", "{\"freeze\":true}");
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_TRUE(r["data"]["frozen"].asBool());
	EXPECT_EQ(r["data"]["layers"].asUInt(), 2u);
	EXPECT_TRUE(rig.domain.Inspector().IsFrozen());
	EXPECT_EQ(rig.domain.Inspector().FrozenLayerCount(), 2u);
	EXPECT_TRUE(rig.domain.Inspector().FrozenAvailable(StringCRC("terrain")));
	EXPECT_TRUE(rig.domain.Inspector().FrozenAvailable(StringCRC("temp")));
}

TEST_F(SelectionPinTest, FreezeFalseIsLivePin)
{
	Rig rig(20, 20);
	Exec("cells.select.cell", "{\"q\":1,\"r\":1}");
	ASSERT_TRUE(Exec("cells.select.pin", "{\"freeze\":false}")["success"].asBool());
	EXPECT_TRUE(rig.domain.Inspector().IsPinned());
	EXPECT_FALSE(rig.domain.Inspector().IsFrozen());
}

TEST_F(SelectionPinTest, NonBoolFreezeIsAnErrorWithNoChange)
{
	Rig rig(20, 20);
	Exec("cells.select.cell", "{\"q\":1,\"r\":1}");
	const Json::Value r = Exec("cells.select.pin", "{\"freeze\":\"yes\"}");
	EXPECT_FALSE(r["success"].asBool());
	EXPECT_NE(r["error"].asString().find("freeze"), std::string::npos);
	EXPECT_FALSE(rig.domain.Inspector().IsPinned());
}

TEST_F(SelectionPinTest, PinEmptySelectionIsAnErrorAndChangesNothing)
{
	Rig rig(20, 20);
	const Json::Value r = Exec("cells.select.pin", "{}");
	EXPECT_FALSE(r["success"].asBool());
	EXPECT_NE(r["error"].asString().find("nothing selected"), std::string::npos);
	EXPECT_FALSE(rig.domain.Inspector().IsPinned());

	Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":1,\"r1\":0}");
	ASSERT_TRUE(Exec("cells.select.pin", "{}")["success"].asBool());
	Exec("cells.select.clear", "{}");
	EXPECT_FALSE(Exec("cells.select.pin", "{\"freeze\":true}")["success"].asBool());
	EXPECT_TRUE(rig.domain.Inspector().IsPinned());
	EXPECT_FALSE(rig.domain.Inspector().IsFrozen());
	EXPECT_EQ(rig.domain.Inspector().PinnedCount(), 2u);
}

TEST_F(SelectionPinTest, PinWithNoActiveGridIsAnError)
{
	CellFieldRegistry cf;
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	LayerUploader uploader(cf, nullptr);
	CellLayerViewerDomain domain(registry, uploader, nullptr);
	FakeDebugDraw draw;
	ViewerCommands commands(domain);
	domain.Update(draw);
	const Json::Value r = Exec("cells.select.pin", "{}");
	EXPECT_FALSE(r["success"].asBool());
	EXPECT_NE(r["error"].asString().find("no active grid"), std::string::npos);
}

TEST_F(SelectionPinTest, UnpinClearsAAndIsIdempotent)
{
	Rig rig(20, 20);
	Exec("cells.select.cell", "{\"q\":1,\"r\":1}");
	Exec("cells.select.pin", "{\"freeze\":true}");
	const Json::Value r = Exec("cells.select.unpin", "{}");
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_TRUE(r["data"]["ok"].asBool());
	EXPECT_FALSE(rig.domain.Inspector().IsPinned());
	EXPECT_EQ(rig.domain.Inspector().FrozenLayerCount(), 0u);
	EXPECT_TRUE(Exec("cells.select.unpin", "{}")["success"].asBool());
}

TEST_F(SelectionPinTest, RePinReplacesA)
{
	Rig rig(20, 20);
	Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":2,\"r1\":2}");
	Exec("cells.select.pin", "{\"freeze\":true}");
	Exec("cells.select.cell", "{\"q\":4,\"r\":4}");
	ASSERT_TRUE(Exec("cells.select.pin", "{}")["success"].asBool());
	EXPECT_EQ(rig.domain.Inspector().PinnedCount(), 1u);
	EXPECT_FALSE(rig.domain.Inspector().IsFrozen());
}

TEST_F(SelectionPinTest, PinUnpinAndFreezeLogOnCelAtInfo)
{
	Rig rig(20, 20);
	Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":1,\"r1\":0}");
	LogCapture cap;
	Exec("cells.select.pin", "{}");
	EXPECT_EQ(cap.Count(Level::kInfo, "cells.select.pin"), 1u);
	Exec("cells.select.pin", "{\"freeze\":true}");
	EXPECT_EQ(cap.Count(Level::kInfo, "froze 2 layers"), 1u);
	Exec("cells.select.unpin", "{}");
	EXPECT_EQ(cap.Count(Level::kInfo, "cells.select.unpin"), 1u);
}

TEST_F(SelectionPinTest, CommandsUnregisterWithDomain)
{
	Rig rig(20, 20);
	rig.commands.Unregister();
	EXPECT_FALSE(Exec("cells.select.pin", "{}")["success"].asBool());
	EXPECT_FALSE(Exec("cells.select.unpin", "{}")["success"].asBool());
}

#endif // DIA_DEBUG
