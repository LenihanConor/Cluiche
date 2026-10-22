#include <gtest/gtest.h>
#include "RampTestSupport.h"
#include <DiaValueSchema/ValueSchemaModule.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/ValueClamper.h>
#include <DiaValueSchema/RampLegend.h>
#include <DiaValueSchema/Testing/FakeCodeSideSource.h>
#include <DiaValueSchema/Testing/SchemaFixtureBuilder.h>
#include <DiaValueSchema/Testing/SchemaTempDirectory.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/FilePath/PathStore.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

// End-to-end: real shipped ramps.json + a fixture schema + fake code-side source go through ValueSchemaModule, and every
// entry is resolved through RampResolver / RampLegend / SampleRamp / ValueClamper. Also a registry golden dump,
// load-order independence, restart stability, and a lint of every shipped *.schema.json.

using namespace Dia::ApplicationFlow;
using namespace Dia::ValueSchema;
using Dia::Core::StringCRC;
using Dia::Core::PathStore;
using Dia::ValueSchema::Testing::FakeCodeSideSource;
using Dia::ValueSchema::Testing::SchemaFixtureBuilder;
using Dia::ValueSchema::Testing::SchemaTempDirectory;
namespace fs = std::filesystem;

namespace
{
	constexpr float kDt = 1.0f / 60.0f;

	struct IntConsumerModule : Module
	{
		static const StringCRC kTypeId;
		explicit IntConsumerModule(const StringCRC& id) : Module(id) {}
		ServiceStreamReader<ValueSchemaService> mReader{ this, StringCRC(ValueSchemaModule::kServiceStreamId) };
		void OnConnectStreams(Application& app) override { mReader.Connect(app); }
		StartResult DoStart() override { return StartResult::kReady; }
		void DoUpdate(float) override {}
		StopResult DoStop() override { return StopResult::kDone; }
	};
	const StringCRC IntConsumerModule::kTypeId("IntConsumerModule");

	struct TestVsc : ValueSchemaModule
	{
		using ValueSchemaModule::ValueSchemaModule;
		void CycleStop() { DoStop(); }
		void CycleStart() { DoStart(); }
	};

	TestVsc*				g_module = nullptr;
	FakeCodeSideSource*		g_source = nullptr;

	Module* CreateVsc(const StringCRC& id)
	{
		auto* m = new TestVsc(id);
		if (g_source) { m->SetCodeSideSource(g_source); }
		m->SetComponentDescriptors(nullptr, 0);	// exact-count tests: no ECS component fields
		g_module = m;
		return m;
	}

	ModuleDeclaration MakeModule(const StringCRC& typeId, const char* instanceId, const char* role, const char* configJson = "")
	{
		ModuleDeclaration mod;
		mod.instanceId = StringCRC(instanceId);
		mod.typeId = typeId;
		mod.startTimeoutMs = 10000.0f;
		mod.stopTimeoutMs = 5000.0f;
		mod.stages.Add(StringCRC("Boot"));
		ChannelBinding ch;
		ch.id = StringCRC(ValueSchemaModule::kServiceStreamId);
		ch.role = StringCRC(role);
		mod.channels.Add(ch);
		mod.configJson = configJson;
		return mod;
	}

	struct Harness
	{
		TypeRegistry					mReg;
		ApplicationManifestV3			mManifest;
		std::unique_ptr<Application>	mApp;
		std::string						mConfig;

		Harness(FakeCodeSideSource* source, const std::string& configJson) : mConfig(configJson)
		{
			g_module = nullptr;
			g_source = source;
			mReg.Register(ValueSchemaModule::kTypeId, CreateVsc);
			mReg.Register(IntConsumerModule::kTypeId, [](const StringCRC& id) -> Module* { return new IntConsumerModule(id); });
			ApplicationManifestV3& m = mManifest;
			m.version = 3;
			StageDeclaration boot;
			boot.name = StringCRC("Boot");
			m.stages.Add(boot);
			m.initialStage = StringCRC("Boot");
			StreamDeclaration svc;
			svc.id = StringCRC(ValueSchemaModule::kServiceStreamId);
			svc.kind = StringCRC("ServiceStream");
			svc.payloadType = StringCRC("ValueSchemaService");
			m.streams.Add(svc);
			ProcessingUnitDeclaration pu;
			pu.instanceId = StringCRC("MainPU");
			pu.frequencyHz = 60.0f;
			pu.dedicatedThread = false;
			pu.modules.Add(MakeModule(ValueSchemaModule::kTypeId, "valueSchema", "provides", mConfig.c_str()));
			pu.modules.Add(MakeModule(IntConsumerModule::kTypeId, "consumer", "consumes"));
			m.processingUnits.Add(pu);
			mApp.reset(new Application(mManifest, mReg));
		}
		bool Start()
		{
			if (!mApp->Start()) return false;
			for (int i = 0; i < 10; ++i) mApp->Update(kDt);
			return g_module != nullptr;
		}
		~Harness()
		{
			g_source = nullptr;
			if (!mApp) return;
			mApp->RequestShutdown();
			for (int guard = 0; guard < 1000 && mApp->Update(kDt); ++guard) {}
			mApp.reset();
		}
	};

	struct ScopedAlias
	{
		StringCRC mAlias;
		ScopedAlias(const char* alias, const std::string& path) : mAlias(alias)
		{
			PathStore::RegisterToStore(mAlias, Dia::Core::Path::String(path.c_str()));
		}
		~ScopedAlias() { PathStore::UnregisterFromStore(mAlias); }
	};

	// ---- fixture content ----

	struct RampCase { const char* mKey; const char* mRamp; double mMin; double mMax; const char* mUnit; };
	const RampCase kRampCases[] = {
		{ "ramp_green",   "sequential.green",   0,       100,    "%" },
		{ "ramp_blue",    "sequential.blue",    -50,     50,     "" },
		{ "ramp_heat",    "sequential.heat",    3,       17,     "K" },
		{ "ramp_grey",    "sequential.grey",    0,       1,      "" },
		{ "ramp_purple",  "sequential.purple",  -0.3,    0.3,    "ratio" },
		{ "ramp_terrain", "sequential.terrain", 0,       8848,   "m" },
		{ "ramp_diverge", "diverging.blue_red", -1,      1,      "" },
	};

	struct DeltaCase { const char* mKey; const char* mDelta; const char* mExpectedRamp; };
	const DeltaCase kDeltaCases[] = {
		{ "delta_default", nullptr,                 "diverging.blue_red" },
		{ "delta_heat",    "sequential.heat",       "sequential.heat" },
		{ "delta_diverge", "diverging.blue_red",    "diverging.blue_red" },
	};

	std::string FloatsSchema()
	{
		SchemaFixtureBuilder b;
		b.For("integration.floats");
		for (const RampCase& c : kRampCases)
		{
			b.Entry(c.mKey, "parameter", "float").Min(c.mMin).Max(c.mMax);
			if (c.mUnit[0]) { b.Unit(c.mUnit); }
			Json::Value ui(Json::objectValue);
			ui["ramp"] = c.mRamp;
			b.Set("ui", ui);
		}
		for (const DeltaCase& d : kDeltaCases)
		{
			b.Entry(d.mKey, "parameter", "float").Min(-5).Max(5);
			Json::Value ui(Json::objectValue);
			if (d.mDelta) { ui["delta_ramp"] = d.mDelta; }
			b.Set("ui", ui);
		}
		b.Entry("count_int", "parameter", "int").Min(0).Max(7).Unit("days");
		Json::Value ui(Json::objectValue);
		ui["ramp"] = "sequential.heat";
		b.Set("ui", ui);
		b.Entry("plain_float", "parameter", "float");
		return b.Build();
	}

	std::string EnumsSchema()
	{
		SchemaFixtureBuilder b;
		b.For("integration.enums");
		b.Entry("mixed", "parameter", "enum");
		b.EnumValueEntry("a", "Alpha", "first", "#ff0000");
		b.EnumValueEntry("b", "Beta");
		b.EnumValueEntry("c", "Gamma", "third", "#00ff00");
		b.EnumValueEntry("d", "Delta");
		b.EnumValueEntry("e", "Epsilon");
		b.Entry("big13", "parameter", "enum");
		for (int i = 0; i < 13; ++i) { b.EnumValueEntry(("v" + std::to_string(i)).c_str(), ("Value " + std::to_string(i)).c_str()); }
		return b.Build();
	}

	const Dia::API::ValueParam kCmdParams[] = {
		{ "rate", "float", true, "Spawn rate", "per_s", true, 0.5f, 9.5f, nullptr },
		{ "mode", "enum", false, "Mode", nullptr, false, 0.0f, 0.0f, "weather|ecology" },
		{ "silent", "int", false, nullptr },
	};

	void FillSource(FakeCodeSideSource& src)
	{
		src.AddMetric("integ.frame.ms", "Frame time", "Milliseconds per frame", "ms");
		src.AddMetric("integ.bare");
		src.AddCommand("integ-spawn", "IntegOwner", "Spawns things", kCmdParams, 3, false, "debug", "integ-spawn --rate=2");
		src.AddCommand("integ-reset", "IntegOwner", "Resets", nullptr, 0, true);
	}

	std::string ConfigFor(bool reversed)
	{
		return reversed ? R"({"schema_roots":["vsc_integ_b","vsc_integ_a"],"ramp_roots":["vsc_integ_r"]})"
						: R"({"schema_roots":["vsc_integ_a","vsc_integ_b"],"ramp_roots":["vsc_integ_r"]})";
	}

	// ---- dumps ----

	std::string Join(const std::vector<std::string>& lines)
	{
		std::string out;
		for (const std::string& l : lines) { out += l; out += "\n"; }
		return out;
	}

	std::string Trim(std::string s)
	{
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) { s.pop_back(); }
		return s;
	}

	const char* KindName(ValueKind k)
	{
		switch (k)
		{
		case ValueKind::Parameter: return "parameter";
		case ValueKind::Field: return "field";
		case ValueKind::Metric: return "metric";
		case ValueKind::Arg: return "arg";
		case ValueKind::Command: return "command";
		case ValueKind::Event: return "event";
		case ValueKind::ComponentField: return "component_field";
		}
		return "?";
	}

	const char* TypeName(ValueType t)
	{
		switch (t)
		{
		case ValueType::Float: return "float";
		case ValueType::Int: return "int";
		case ValueType::Bool: return "bool";
		case ValueType::Enum: return "enum";
		case ValueType::String: return "string";
		case ValueType::Ref: return "ref";
		case ValueType::RefList: return "reflist";
		case ValueType::Map: return "map";
		}
		return "?";
	}

	std::string DumpEntry(const ValueInfo& v)
	{
		char buf[64];
		std::string s = std::string(v.mFullKey.AsCStr()) + " | " + KindName(v.mKind) + " | " + TypeName(v.mType)
			+ " | label=" + v.mLabel.AsCStr() + " | unit=" + v.mUnit.AsCStr();
		if (v.mHasMin || v.mHasMax)
		{
			snprintf(buf, sizeof(buf), " | range=%g..%g", v.mMin, v.mMax);
			s += buf;
		}
		if (v.mRequired) { s += " | required"; }
		if (!v.mUi.isNull()) { s += " | ui=" + Trim(Json::FastWriter().write(v.mUi)); }
		if (v.mEnumValues.Size() > 0)
		{
			s += " | enum=";
			for (unsigned i = 0; i < v.mEnumValues.Size(); ++i)
			{
				const EnumValue& e = v.mEnumValues[i];
				s += std::string(i ? "," : "") + e.mId.AsCStr() + ":" + e.mColour.AsCStr() + (e.mHasColour ? "!" : "~");
			}
		}
		if (v.mRelated.Size() > 0)
		{
			s += " | related=";
			for (unsigned i = 0; i < v.mRelated.Size(); ++i) { s += std::string(i ? "," : "") + v.mRelated[i].AsCStr(); }
		}
		return s;
	}

	std::string DumpRegistry(const ValueSchemaService& svc)
	{
		std::vector<std::string> lines;
		for (const char* fmt : { "dia.command", "dia.metric", "integration.enums", "integration.floats" })
		{
			const ValueInfoSpan span = svc.ForFormat(StringCRC(fmt));
			for (unsigned i = 0; i < span.mCount; ++i) { lines.push_back(DumpEntry(*span.mItems[i])); }
		}
		std::sort(lines.begin(), lines.end());
		return Join(lines);
	}

	std::string Hex(uint32_t c)
	{
		char buf[16];
		snprintf(buf, sizeof(buf), "%08X", c);
		return buf;
	}

	// Resolves every fixture entry; EXPECTs the "everything is honoured" invariants; returns a behaviour dump.
	std::string Exercise(const ValueSchemaService& svc)
	{
		RampResolver res(svc);
		ValueClamper clamper;
		std::vector<std::string> lines;
		char buf[96];

		const ValueInfoSpan floats = svc.ForFormat(StringCRC("integration.floats"));
		EXPECT_EQ(static_cast<unsigned>(sizeof(kRampCases) / sizeof(kRampCases[0]) + sizeof(kDeltaCases) / sizeof(kDeltaCases[0]) + 2), floats.mCount);
		for (unsigned i = 0; i < floats.mCount; ++i)
		{
			const ValueInfo& info = *floats.mItems[i];
			const std::string key = info.mKey.AsCStr();
			const ResolvedRamp r = res.ResolveRamp(info);
			EXPECT_FALSE(r.mFellBack) << key;
			EXPECT_GE(r.mRamp.mStopCount, 2u) << key;
			const std::string wanted = info.mUi["ramp"].isString() ? info.mUi["ramp"].asString() : "sequential.grey";
			EXPECT_EQ(wanted, r.mRamp.mName.AsCStr()) << key;

			// SampleRamp / RampT.
			const bool hasRange = info.mHasMin && info.mHasMax;
			EXPECT_EQ(hasRange, r.mHasRange) << key;
			EXPECT_EQ(SampleRamp(r.mRamp, 0.0f), SampleRamp(r.mRamp, RampT(r, r.mMin))) << key;
			EXPECT_EQ(SampleRamp(r.mRamp, 1.0f), SampleRamp(r.mRamp, RampT(r, r.mMax))) << key;
			std::string line = key + " ramp=" + r.mRamp.mName.AsCStr();
			for (float t : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
			{
				const uint32_t c = SampleRamp(r.mRamp, t);
				EXPECT_EQ(0xFFu, c & 0xFFu) << key;
				line += " " + Hex(c);
			}

			// Legend.
			const LegendData l = BuildLegend(res, info, true, r.mMin, r.mMax);
			EXPECT_FALSE(l.mIsCategory) << key;
			EXPECT_FALSE(l.mRamp.mFellBack) << key;
			EXPECT_GE(l.mTickCount, 2u) << key;
			EXPECT_LE(l.mTickCount, kMaxLegendTicks) << key;
			EXPECT_EQ(r.mMin, l.mTicks[0]) << key;
			EXPECT_EQ(r.mMax, l.mTicks[l.mTickCount - 1]) << key;
			EXPECT_STREQ(info.mUnit.AsCStr(), l.mUnit.AsCStr()) << key;
			EXPECT_TRUE(l.mHasSeen) << key;
			line += " ticks";
			for (unsigned k = 0; k < l.mTickCount; ++k)
			{
				EXPECT_FALSE(std::isnan(l.mTicks[k])) << key;
				EXPECT_GE(l.mTicks[k], r.mMin) << key;
				EXPECT_LE(l.mTicks[k], r.mMax) << key;
				if (k) { EXPECT_LT(l.mTicks[k - 1], l.mTicks[k]) << key; }
				const float t = RampT(r, l.mTicks[k]);
				EXPECT_FALSE(std::isnan(t)) << key;
				EXPECT_GE(t, 0.0f);
				EXPECT_LE(t, 1.0f);
				snprintf(buf, sizeof(buf), " %g", static_cast<double>(l.mTicks[k]));
				line += buf;
			}

			// Clamper.
			if (hasRange)
			{
				// The clamper rounds inward: the result never leaves the declared double range.
				const double hi = clamper.Clamp(static_cast<float>(info.mMax) + 10.0f, info);
				const double lo = clamper.Clamp(static_cast<float>(info.mMin) - 10.0f, info);
				EXPECT_LE(hi, info.mMax) << key;
				EXPECT_GE(lo, info.mMin) << key;
				EXPECT_NEAR(info.mMax, hi, std::fabs(info.mMax) * 1e-6 + 1e-9) << key;
				EXPECT_NEAR(info.mMin, lo, std::fabs(info.mMin) * 1e-6 + 1e-9) << key;
			}
			lines.push_back(line);
		}

		// Delta ramps.
		for (const DeltaCase& d : kDeltaCases)
		{
			const ValueInfo* info = svc.Find(StringCRC("integration.floats"), StringCRC(d.mKey));
			EXPECT_NE(info, nullptr) << d.mKey;
			if (!info) { continue; }
			const ResolvedRamp r = res.ResolveDeltaRamp(*info, 5.0f);
			EXPECT_FALSE(r.mFellBack) << d.mKey;
			EXPECT_STREQ(d.mExpectedRamp, r.mRamp.mName.AsCStr()) << d.mKey;
			EXPECT_FLOAT_EQ(0.5f, RampT(r, 0.0f)) << d.mKey;
			snprintf(buf, sizeof(buf), "%s delta=%s centre=", d.mKey, r.mRamp.mName.AsCStr());
			lines.push_back(buf + Hex(SampleRamp(r.mRamp, RampT(r, 0.0f))) + " " + Hex(SampleRamp(r.mRamp, RampT(r, -5.0f)))
				+ " " + Hex(SampleRamp(r.mRamp, RampT(r, 5.0f))));
		}

		// Enums.
		const ValueInfoSpan enums = svc.ForFormat(StringCRC("integration.enums"));
		EXPECT_EQ(2u, enums.mCount);
		const RampData* cat = svc.FindRamp(StringCRC("categorical.12"));
		EXPECT_NE(cat, nullptr);
		for (unsigned i = 0; i < enums.mCount; ++i)
		{
			const ValueInfo& info = *enums.mItems[i];
			const std::string key = info.mKey.AsCStr();
			const ResolvedPalette p = res.ResolvePalette(info);
			const LegendData l = BuildLegend(res, info, false, 0, 0);
			EXPECT_EQ(info.mEnumValues.Size(), p.mEntries.Size()) << key;
			EXPECT_TRUE(l.mIsCategory) << key;
			EXPECT_EQ(info.mEnumValues.Size(), l.mSwatches.Size()) << key;
			std::string line = key + " palette";
			for (unsigned k = 0; k < p.mEntries.Size() && cat != nullptr; ++k)
			{
				EXPECT_EQ(p.mEntries[k].mRgba8, l.mSwatches[k].mRgba8) << key << k;
				EXPECT_STREQ(info.mEnumValues[k].mLabel.AsCStr(), l.mSwatches[k].mLabel.AsCStr()) << key << k;
				if (info.mEnumValues[k].mHasColour) { EXPECT_EQ(0xFFu, p.mEntries[k].mRgba8 & 0xFFu); }
				else { EXPECT_EQ(cat->mColours[k % 12], p.mEntries[k].mRgba8) << key << k; }
				line += " " + std::string(p.mEntries[k].mId.AsCStr()) + ":" + Hex(p.mEntries[k].mRgba8);
			}
			if (key == "mixed")
			{
				EXPECT_FALSE(p.mWrapped);
				EXPECT_EQ(5u, p.mEntries.Size());
				if (p.mEntries.Size() == 5)
				{
					EXPECT_EQ(0xFF0000FFu, p.mEntries[0].mRgba8);
					EXPECT_EQ(0x00FF00FFu, p.mEntries[2].mRgba8);
				}
			}
			else
			{
				EXPECT_TRUE(p.mWrapped) << key;
				EXPECT_EQ(p.mEntries[0].mRgba8, p.mEntries[12].mRgba8) << key;
			}
			lines.push_back(line);
		}

		// Plain float without ui/range: grey, unit interval.
		const ValueInfo* plain = svc.Find(StringCRC("integration.floats"), StringCRC("plain_float"));
		EXPECT_NE(plain, nullptr);
		if (plain)
		{
			const ResolvedRamp r = res.ResolveRamp(*plain);
			EXPECT_FALSE(r.mHasRange);
			EXPECT_FALSE(r.mFellBack);
			EXPECT_STREQ("sequential.grey", r.mRamp.mName.AsCStr());
			EXPECT_EQ(plain->mMin, plain->mMax);		// no range declared
		}

		EXPECT_EQ(0u, res.FallbackCount());
		EXPECT_EQ(0u, res.UnknownRampWarningCount());
		EXPECT_EQ(1u, res.WrapWarningCount());			// only big13 wraps
		EXPECT_EQ(22u, clamper.ClampCount());		// 11 ranged entries x (one clamp above max + one below min)
		std::sort(lines.begin(), lines.end());
		return Join(lines);
	}

	struct Outcome
	{
		std::string mDump;
		std::string mBehaviour;
		std::string mDumpAfterRestart;
		std::string mBehaviourAfterRestart;
		unsigned	mRampCount = 0;
		bool		mHealthy = false;
		bool		mOk = false;
	};

	Outcome RunFlow(bool reversed)
	{
		Outcome out;
		SchemaTempDirectory rampsDir("IntegRamps"), a("IntegSchemaA"), b("IntegSchemaB");
		rampsDir.Write("ramps.json", VsTest::ReadShippedRampsText(__FILE__));
		a.Write("floats.schema.json", FloatsSchema());
		b.Write("enums.schema.json", EnumsSchema());
		ScopedAlias ar("vsc_integ_r", rampsDir.Path()), aa("vsc_integ_a", a.Path()), ab("vsc_integ_b", b.Path());

		FakeCodeSideSource src;
		FillSource(src);
		Harness h(&src, ConfigFor(reversed));
		if (!h.Start()) { return out; }
		const ValueSchemaService* svc = g_module->GetService();
		if (svc == nullptr) { return out; }
		out.mRampCount = svc->RampCount();
		out.mHealthy = g_module->GetHealthReporter().Report().status == Dia::Observation::Health::HealthStatus::kOK;
		out.mDump = DumpRegistry(*svc);
		out.mBehaviour = Exercise(*svc);

		g_module->CycleStop();
		g_module->CycleStart();
		svc = g_module->GetService();
		if (svc == nullptr) { return out; }
		out.mDumpAfterRestart = DumpRegistry(*svc);
		out.mBehaviourAfterRestart = Exercise(*svc);
		out.mOk = true;
		return out;
	}

	const char* const kGoldenRegistry =
		"dia.command/integ-reset | command | string | label=integ-reset | unit= | ui={\"category\":\"\",\"example\":\"\",\"owner\":\"IntegOwner\"}\n"
		"dia.command/integ-spawn | command | string | label=integ-spawn | unit= | ui={\"category\":\"debug\",\"example\":\"integ-spawn --rate=2\",\"owner\":\"IntegOwner\"} | related=integ-spawn.rate,integ-spawn.mode,integ-spawn.silent\n"
		"dia.command/integ-spawn.mode | arg | enum | label=mode | unit= | enum=weather:~,ecology:~\n"
		"dia.command/integ-spawn.rate | arg | float | label=rate | unit=per_s | range=0.5..9.5 | required\n"
		"dia.command/integ-spawn.silent | arg | int | label=silent | unit=\n"
		"dia.metric/integ.bare | metric | float | label=integ.bare | unit=\n"
		"dia.metric/integ.frame.ms | metric | float | label=Frame time | unit=ms\n"
		"integration.enums/big13 | parameter | enum | label=big13 | unit= | enum=v0:#4E79A7~,v1:#F28E2B~,v2:#E15759~,v3:#76B7B2~,v4:#59A14F~,v5:#EDC948~,v6:#B07AA1~,v7:#FF9DA7~,v8:#9C755F~,v9:#BAB0AC~,v10:#1F77B4~,v11:#8CD17D~,v12:#4E79A7~\n"
		"integration.enums/mixed | parameter | enum | label=mixed | unit= | enum=a:#ff0000!,b:#F28E2B~,c:#00ff00!,d:#76B7B2~,e:#59A14F~\n"
		"integration.floats/count_int | parameter | int | label=count_int | unit=days | range=0..7 | ui={\"ramp\":\"sequential.heat\"}\n"
		"integration.floats/delta_default | parameter | float | label=delta_default | unit= | range=-5..5 | ui={}\n"
		"integration.floats/delta_diverge | parameter | float | label=delta_diverge | unit= | range=-5..5 | ui={\"delta_ramp\":\"diverging.blue_red\"}\n"
		"integration.floats/delta_heat | parameter | float | label=delta_heat | unit= | range=-5..5 | ui={\"delta_ramp\":\"sequential.heat\"}\n"
		"integration.floats/plain_float | parameter | float | label=plain_float | unit=\n"
		"integration.floats/ramp_blue | parameter | float | label=ramp_blue | unit= | range=-50..50 | ui={\"ramp\":\"sequential.blue\"}\n"
		"integration.floats/ramp_diverge | parameter | float | label=ramp_diverge | unit= | range=-1..1 | ui={\"ramp\":\"diverging.blue_red\"}\n"
		"integration.floats/ramp_green | parameter | float | label=ramp_green | unit=% | range=0..100 | ui={\"ramp\":\"sequential.green\"}\n"
		"integration.floats/ramp_grey | parameter | float | label=ramp_grey | unit= | range=0..1 | ui={\"ramp\":\"sequential.grey\"}\n"
		"integration.floats/ramp_heat | parameter | float | label=ramp_heat | unit=K | range=3..17 | ui={\"ramp\":\"sequential.heat\"}\n"
		"integration.floats/ramp_purple | parameter | float | label=ramp_purple | unit=ratio | range=-0.3..0.3 | ui={\"ramp\":\"sequential.purple\"}\n"
		"integration.floats/ramp_terrain | parameter | float | label=ramp_terrain | unit=m | range=0..8848 | ui={\"ramp\":\"sequential.terrain\"}\n";
}

// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaIntegration, EveryEntryResolvesWithoutFallbackThroughTheModule)
{
	const Outcome o = RunFlow(false);
	ASSERT_TRUE(o.mOk);
	EXPECT_EQ(8u, o.mRampCount);
	EXPECT_TRUE(o.mHealthy);
	EXPECT_FALSE(o.mBehaviour.empty());
}

TEST(DiaValueSchemaIntegration, ReversedSchemaRootOrderGivesIdenticalResults)
{
	const Outcome forward = RunFlow(false);
	const Outcome reversed = RunFlow(true);
	ASSERT_TRUE(forward.mOk);
	ASSERT_TRUE(reversed.mOk);
	EXPECT_EQ(forward.mDump, reversed.mDump);
	EXPECT_EQ(forward.mBehaviour, reversed.mBehaviour);
}

TEST(DiaValueSchemaIntegration, StopStartGivesIdenticalResults)
{
	const Outcome o = RunFlow(false);
	ASSERT_TRUE(o.mOk);
	EXPECT_EQ(o.mDump, o.mDumpAfterRestart);
	EXPECT_EQ(o.mBehaviour, o.mBehaviourAfterRestart);
}

TEST(DiaValueSchemaIntegration, RegistryDumpMatchesGolden)
{
	const Outcome o = RunFlow(false);
	ASSERT_TRUE(o.mOk);
	EXPECT_EQ(std::string(kGoldenRegistry), o.mDump);
}

TEST(DiaValueSchemaIntegration, CodeSideEntriesAreCollectedAlongsideSchemaEntries)
{
	SchemaTempDirectory rampsDir("IntegRamps2"), a("IntegSchemaA2"), b("IntegSchemaB2");
	rampsDir.Write("ramps.json", VsTest::ReadShippedRampsText(__FILE__));
	a.Write("floats.schema.json", FloatsSchema());
	b.Write("enums.schema.json", EnumsSchema());
	ScopedAlias ar("vsc_integ_r", rampsDir.Path()), aa("vsc_integ_a", a.Path()), ab("vsc_integ_b", b.Path());
	FakeCodeSideSource src;
	FillSource(src);
	Harness h(&src, ConfigFor(false));
	ASSERT_TRUE(h.Start());
	const CodeSideReport& rep = g_module->GetCodeSideReport();
	EXPECT_EQ(2u, rep.mMetricsCollected);
	EXPECT_EQ(1u, rep.mMetricsDescribed);
	EXPECT_EQ(2u, rep.mCommandsCollected);
	EXPECT_EQ(3u, rep.mArgsCollected);
	EXPECT_EQ(2u, rep.mArgsDescribed);
	EXPECT_EQ(1u, rep.mArgsUndescribed);
	EXPECT_EQ(0u, rep.mRejectedCount + rep.mDuplicateCount);
	const ValueSchemaService* svc = g_module->GetService();
	ASSERT_NE(svc, nullptr);
	EXPECT_EQ(12u + 2u + 2u + 2u + 3u, svc->Count());		// floats + enums + metrics + commands + args
	// The code-side float metric resolves through the ramp machinery like any schema entry.
	const ValueInfo* metric = svc->Find(StringCRC("dia.metric/integ.frame.ms"));
	ASSERT_NE(metric, nullptr);
	RampResolver res(*svc);
	const LegendData l = BuildLegend(res, *metric, false, 0, 0);
	EXPECT_STREQ("ms", l.mUnit.AsCStr());
	EXPECT_EQ(3u, l.mTickCount);
	EXPECT_FALSE(l.mRamp.mFellBack);
	EXPECT_EQ(0u, res.FallbackCount());
}

// ---------------------------------------------------------------------------------------------------------
// Lint: every ui.ramp / ui.delta_ramp in every shipped *.schema.json resolves to a loaded continuous ramp.
// ---------------------------------------------------------------------------------------------------------

namespace
{
	typedef std::map<std::string, RampKind> RampKinds;

	void LintEntries(const Json::Value& doc, const std::string& file, const RampKinds& ramps, std::vector<std::string>& problems)
	{
		const Json::Value& entries = doc["entries"];
		if (!entries.isArray()) { return; }
		for (const Json::Value& e : entries)
		{
			const Json::Value& ui = e["ui"];
			if (!ui.isObject()) { continue; }
			for (const char* key : { "ramp", "delta_ramp" })
			{
				if (!ui.isMember(key)) { continue; }
				const std::string where = file + ": entry '" + e["key"].asString() + "' ui." + key;
				if (!ui[key].isString()) { problems.push_back(where + " is not a string"); continue; }
				const auto it = ramps.find(ui[key].asString());
				if (it == ramps.end()) { problems.push_back(where + " names unknown ramp '" + ui[key].asString() + "'"); continue; }
				if (it->second != RampKind::Continuous) { problems.push_back(where + " names categorical ramp '" + ui[key].asString() + "'"); }
			}
		}
	}

	bool Excluded(const fs::path& dir)
	{
		const std::string n = dir.filename().string();
		return n == ".venv" || n == "node_modules" || n == "bin" || n == "out" || n == "obj" || n == "Testing" || n == ".git"
			|| n == "x64" || n == "External";
	}

	void CollectFiles(const fs::path& root, const char* suffix, std::vector<fs::path>& out)
	{
		std::error_code ec;
		if (!fs::exists(root, ec)) { return; }
		fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
		for (; it != end && !ec; it.increment(ec))
		{
			if (it->is_directory(ec) && Excluded(it->path())) { it.disable_recursion_pending(); continue; }
			const std::string name = it->path().filename().string();
			if (name.size() > strlen(suffix) && name.compare(name.size() - strlen(suffix), strlen(suffix), suffix) == 0) { out.push_back(it->path()); }
		}
	}

	void AddRampsFromText(const std::string& text, RampKinds& kinds)
	{
		RampLoadResult r = RampLoader::LoadFromText(text.c_str(), "lint");
		for (unsigned i = 0; i < r.mRamps.Size(); ++i) { kinds[r.mRamps[i].mName.AsCStr()] = r.mRamps[i].mKind; }
	}
}

TEST(DiaValueSchemaIntegrationLint, LintFlagsUnknownCategoricalAndNonStringRamps)
{
	RampKinds ramps;
	AddRampsFromText(VsTest::ReadShippedRampsText(__FILE__), ramps);
	ASSERT_EQ(8u, ramps.size());
	Json::Value doc;
	Json::Reader().parse(R"({"entries":[
		{"key":"ok","ui":{"ramp":"sequential.green","delta_ramp":"diverging.blue_red"}},
		{"key":"unknown","ui":{"ramp":"viridis"}},
		{"key":"cat","ui":{"delta_ramp":"categorical.12"}},
		{"key":"num","ui":{"ramp":5}},
		{"key":"noui"},
		{"key":"emptyui","ui":{}}]})", doc);
	std::vector<std::string> problems;
	LintEntries(doc, "synthetic", ramps, problems);
	ASSERT_EQ(3u, problems.size());
	EXPECT_NE(std::string::npos, problems[0].find("unknown ramp 'viridis'"));
	EXPECT_NE(std::string::npos, problems[1].find("categorical ramp"));
	EXPECT_NE(std::string::npos, problems[2].find("not a string"));
}

TEST(DiaValueSchemaIntegrationLint, ShippedSchemasReferenceOnlyLoadedRamps)
{
	const fs::path rampsFile = VsTest::FindUpwards(__FILE__, "Dia/DiaValueSchema/Data/ramps.json");
	ASSERT_FALSE(rampsFile.empty());
	// <root> is the directory that contains "Dia".
	const fs::path root = rampsFile.parent_path().parent_path().parent_path().parent_path();
	RampKinds ramps;
	{
		std::string text;
		ASSERT_TRUE(VsTest::ReadFileBytes(rampsFile, text));
		AddRampsFromText(text, ramps);
	}
	std::vector<fs::path> rampFiles, schemaFiles;
	for (const char* sub : { "Dia", "Cluiche/Assets" })
	{
		CollectFiles(root / sub, ".ramps.json", rampFiles);
		CollectFiles(root / sub, ".schema.json", schemaFiles);
	}
	for (const fs::path& p : rampFiles)
	{
		std::string text;
		if (VsTest::ReadFileBytes(p, text)) { AddRampsFromText(text, ramps); }
	}
	if (schemaFiles.empty())
	{
		GTEST_SKIP() << "No shipped *.schema.json files under Dia/ or Cluiche/Assets/ (test fixtures under Testing/ are excluded); lint logic is covered by LintFlagsUnknownCategoricalAndNonStringRamps";
	}
	std::vector<std::string> problems;
	for (const fs::path& p : schemaFiles)
	{
		std::string text;
		ASSERT_TRUE(VsTest::ReadFileBytes(p, text)) << p.string();
		Json::Value doc;
		ASSERT_TRUE(Json::Reader().parse(text, doc)) << p.string();
		LintEntries(doc, p.string(), ramps, problems);
	}
	for (const std::string& problem : problems) { ADD_FAILURE() << problem; }
}
