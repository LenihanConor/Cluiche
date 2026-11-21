#include <gtest/gtest.h>
#include <DiaValueSchema/RampRegistry.h>
#include <DiaValueSchema/RampSetLoader.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/Testing/SchemaTempDirectory.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include <DiaCore/CRC/StringCRC.h>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Exhaustive edge tests for RampRegistry, RampSetLoader (on-disk discovery of Dia and game ramp files) and the ramp
// half of ValueSchemaService, including concurrent readers. PIN tests document current behaviour on purpose.

using namespace Dia::ValueSchema;
using Dia::Core::StringCRC;
using Dia::ValueSchema::Testing::SchemaTempDirectory;
using Dia::ValueSchema::Testing::MakeValueInfo;
namespace fs = std::filesystem;

namespace
{
	RampData MakeRamp(const std::string& name, uint32_t c0 = 0x000000FFu, uint32_t c1 = 0xFFFFFFFFu)
	{
		RampData r;
		SetText(r.mName, name.c_str());
		r.mId = StringCRC(name.c_str());
		r.mKind = RampKind::Continuous;
		r.mStops[0].mT = 0.0f;
		r.mStops[0].mRgba8 = c0;
		r.mStops[1].mT = 1.0f;
		r.mStops[1].mRgba8 = c1;
		r.mStopCount = 2;
		return r;
	}

	std::string RampJson(const std::string& id, const char* c0 = "#000000", const char* c1 = "#ffffff")
	{
		return std::string(R"({"id":")") + id + R"(","kind":"continuous","stops":[{"t":0.0,"colour":")" + c0 +
			R"("},{"t":1.0,"colour":")" + c1 + R"("}]})";
	}
	std::string RampFile(const std::string& ramps)
	{
		return std::string(R"({"format":"dia.ramps","version":1,"ramps":[)") + ramps + "]}";
	}
	std::string ReplaceAll(std::string s, const std::string& from, const std::string& to)
	{
		size_t pos = 0;
		while ((pos = s.find(from, pos)) != std::string::npos) { s.replace(pos, from.size(), to); pos += to.size(); }
		return s;
	}

	// Two distinct lower-case names with the same 32-bit StringCRC (birthday search over seeded random strings).
	bool FindCollidingNames(std::string& a, std::string& b)
	{
		static std::string sA, sB;	// the search takes ~200 ms: do it once per process
		if (!sA.empty()) { a = sA; b = sB; return true; }
		std::mt19937 rng(4321);
		std::unordered_map<unsigned, std::string> seen;
		for (unsigned i = 0; i < 1000000; ++i)
		{
			std::string s = "ramp_";
			for (int c = 0; c < 8; ++c) { s += static_cast<char>('a' + rng() % 26); }
			const unsigned h = StringCRC(s.c_str()).Value();
			auto it = seen.find(h);
			if (it != seen.end() && it->second != s) { sA = a = it->second; sB = b = s; return true; }
			seen[h] = s;
		}
		return false;
	}

	uint32_t FirstStop(const RampRegistry& reg, const char* id)
	{
		const RampData* r = reg.Find(StringCRC(id));
		return r ? r->mStops[0].mRgba8 : 0u;
	}
}

// ---- RampRegistry -----------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampRegistryEdge, CountAfterRejectionsOnlyCountsAccepted)
{
	RampRegistry reg;
	EXPECT_EQ(reg.Register(MakeRamp("dia.one"), true, "dia.json"), RampRegisterResult::Ok);
	EXPECT_EQ(reg.Register(MakeRamp("dia.one"), false, "game1.ramps.json"), RampRegisterResult::ClashesWithDia);
	EXPECT_EQ(reg.Register(MakeRamp("dia.one"), true, "dia2.json"), RampRegisterResult::DuplicateId);
	EXPECT_EQ(reg.Register(MakeRamp("game.one"), false, "game1.ramps.json"), RampRegisterResult::Ok);
	EXPECT_EQ(reg.Register(MakeRamp("game.one"), false, "game2.ramps.json"), RampRegisterResult::DuplicateId);
	EXPECT_EQ(reg.Count(), 2u);
}

TEST(DiaValueSchemaRampRegistryEdge, PublishTwiceIsIdempotentAndRegisterAfterPublishRejected)
{
	RampRegistry reg;
	ASSERT_EQ(reg.Register(MakeRamp("a"), false, "f"), RampRegisterResult::Ok);
	EXPECT_FALSE(reg.IsPublished());
	reg.Publish();
	reg.Publish();
	EXPECT_TRUE(reg.IsPublished());
	EXPECT_EQ(reg.Register(MakeRamp("b"), false, "f"), RampRegisterResult::AlreadyPublished);
	EXPECT_EQ(reg.Register(MakeRamp("a"), false, "f"), RampRegisterResult::AlreadyPublished);	// even a duplicate
	EXPECT_EQ(reg.Register(MakeRamp("a"), true, "f"), RampRegisterResult::AlreadyPublished);
	EXPECT_EQ(reg.Count(), 1u);
	EXPECT_NE(reg.Find(StringCRC("a")), nullptr);
	EXPECT_EQ(reg.Find(StringCRC("b")), nullptr);
}

TEST(DiaValueSchemaRampRegistryEdge, NullSourceFileIsAcceptedEverywhere)
{
	RampRegistry reg;
	EXPECT_EQ(reg.Register(MakeRamp("a"), false, nullptr), RampRegisterResult::Ok);
	EXPECT_EQ(reg.Register(MakeRamp("a"), false, nullptr), RampRegisterResult::DuplicateId);
	EXPECT_EQ(reg.Register(MakeRamp("a"), true, nullptr), RampRegisterResult::Ok);	// Dia replaces the game ramp
	EXPECT_EQ(reg.Register(MakeRamp("a"), false, nullptr), RampRegisterResult::ClashesWithDia);
	reg.Publish();
	EXPECT_EQ(reg.Register(MakeRamp("z"), false, nullptr), RampRegisterResult::AlreadyPublished);
}

TEST(DiaValueSchemaRampRegistryEdge, PrintfDirectivesInSourceFilesAreHarmless)
{
	RampRegistry reg;
	EXPECT_EQ(reg.Register(MakeRamp("a"), false, "dir%s/%d%n.ramps.json"), RampRegisterResult::Ok);
	EXPECT_EQ(reg.Register(MakeRamp("a"), false, "other%n%s.ramps.json"), RampRegisterResult::DuplicateId);
	EXPECT_EQ(reg.Register(MakeRamp("b"), true, "100%.json"), RampRegisterResult::Ok);
	EXPECT_EQ(reg.Register(MakeRamp("b"), false, "%s%s%s"), RampRegisterResult::ClashesWithDia);
	reg.Publish();
	EXPECT_EQ(reg.Register(MakeRamp("c"), false, "%n"), RampRegisterResult::AlreadyPublished);
	EXPECT_EQ(reg.Count(), 2u);
}

TEST(DiaValueSchemaRampRegistryEdge, PrintfDirectivesInRampNamesAreHarmless)
{
	RampRegistry reg;
	EXPECT_EQ(reg.Register(MakeRamp("%s%d%n"), false, "f"), RampRegisterResult::Ok);
	EXPECT_EQ(reg.Register(MakeRamp("%s%d%n"), false, "g"), RampRegisterResult::DuplicateId);
	EXPECT_NE(reg.Find(StringCRC("%s%d%n")), nullptr);
}

TEST(DiaValueSchemaRampRegistryEdge, DiaRampRegisteredAfterGameRampStillWins)
{
	// FIX 7: the header promises "Dia wins regardless of load order", but a Dia ramp registered after a game ramp
	// with the same id was rejected as DuplicateId and the game ramp stayed.
	RampRegistry reg;
	ASSERT_EQ(reg.Register(MakeRamp("shared", 0x111111FFu), false, "game.ramps.json"), RampRegisterResult::Ok);
	const RampData* before = reg.Find(StringCRC("shared"));
	ASSERT_NE(before, nullptr);
	EXPECT_EQ(reg.Register(MakeRamp("shared", 0x222222FFu), true, "dia/ramps.json"), RampRegisterResult::Ok);
	EXPECT_EQ(reg.Count(), 1u);
	EXPECT_EQ(FirstStop(reg, "shared"), 0x222222FFu);
	EXPECT_EQ(reg.Find(StringCRC("shared")), before) << "pointers handed out earlier stay valid";
	// Once Dia owns the id, a later game ramp is the usual clash and a later Dia ramp a duplicate.
	EXPECT_EQ(reg.Register(MakeRamp("shared", 0x333333FFu), false, "game2.ramps.json"), RampRegisterResult::ClashesWithDia);
	EXPECT_EQ(reg.Register(MakeRamp("shared", 0x444444FFu), true, "dia2.json"), RampRegisterResult::DuplicateId);
	EXPECT_EQ(FirstStop(reg, "shared"), 0x222222FFu);
}

TEST(DiaValueSchemaRampRegistryEdge, DiaFirstThenGameKeepsDiaColours)
{
	RampRegistry reg;
	ASSERT_EQ(reg.Register(MakeRamp("shared", 0x222222FFu), true, "dia"), RampRegisterResult::Ok);
	EXPECT_EQ(reg.Register(MakeRamp("shared", 0x111111FFu), false, "game"), RampRegisterResult::ClashesWithDia);
	EXPECT_EQ(FirstStop(reg, "shared"), 0x222222FFu);
}

TEST(DiaValueSchemaRampRegistryEdge, FindUnknownAndZeroIdsAreNull)
{
	RampRegistry reg;
	reg.Register(MakeRamp("a"), false, "f");
	EXPECT_EQ(reg.Find(StringCRC("nope")), nullptr);
	EXPECT_EQ(reg.Find(StringCRC::kZero), nullptr);
	RampRegistry empty;
	EXPECT_EQ(empty.Find(StringCRC("a")), nullptr);
	EXPECT_EQ(empty.Count(), 0u);
}

TEST(DiaValueSchemaRampRegistryEdge, ReturnedPointersStayValidAcrossManyRegistrations)
{
	RampRegistry reg;
	ASSERT_EQ(reg.Register(MakeRamp("first", 0xAABBCCFFu), false, "f"), RampRegisterResult::Ok);
	const RampData* first = reg.Find(StringCRC("first"));
	for (int i = 0; i < 2000; ++i) { reg.Register(MakeRamp("r" + std::to_string(i)), false, "f"); }
	EXPECT_EQ(reg.Find(StringCRC("first")), first);
	EXPECT_EQ(first->mStops[0].mRgba8, 0xAABBCCFFu);
	EXPECT_EQ(reg.Count(), 2001u);
}

TEST(DiaValueSchemaRampRegistryEdge, EmptyRampNameIsAnOrdinaryIdPin)
{
	// PIN: the registry does not validate names; "" registers once under StringCRC("").
	RampRegistry reg;
	EXPECT_EQ(reg.Register(MakeRamp(""), false, "f"), RampRegisterResult::Ok);
	EXPECT_EQ(reg.Register(MakeRamp(""), false, "g"), RampRegisterResult::DuplicateId);
}

TEST(DiaValueSchemaRampRegistryEdge, DifferentNamesWithTheSameCrcAreAHashCollisionNotADuplicate)
{
	std::string a, b;
	ASSERT_TRUE(FindCollidingNames(a, b));
	ASSERT_NE(a, b);
	ASSERT_EQ(StringCRC(a.c_str()).Value(), StringCRC(b.c_str()).Value());

	RampRegistry reg;
	ASSERT_EQ(reg.Register(MakeRamp(a, 0x111111FFu), false, "first.ramps.json"), RampRegisterResult::Ok);
	EXPECT_EQ(reg.Register(MakeRamp(b, 0x222222FFu), false, "second.ramps.json"), RampRegisterResult::HashCollision);
	EXPECT_EQ(reg.Count(), 1u);
	const RampData* r = reg.Find(StringCRC(a.c_str()));
	ASSERT_NE(r, nullptr);
	EXPECT_STREQ(r->mName.AsCStr(), a.c_str());
	EXPECT_EQ(r->mStops[0].mRgba8, 0x111111FFu);
	// The genuine duplicate keeps its own verdict.
	EXPECT_EQ(reg.Register(MakeRamp(a), false, "third.ramps.json"), RampRegisterResult::DuplicateId);
}

TEST(DiaValueSchemaRampRegistryEdge, CollisionAcrossDiaAndGameIsACollisionNotAClash)
{
	std::string a, b;
	ASSERT_TRUE(FindCollidingNames(a, b));
	{
		RampRegistry reg;
		ASSERT_EQ(reg.Register(MakeRamp(a), true, "dia"), RampRegisterResult::Ok);
		EXPECT_EQ(reg.Register(MakeRamp(b), false, "game"), RampRegisterResult::HashCollision);
		EXPECT_EQ(reg.Count(), 1u);
	}
	{
		RampRegistry reg;
		ASSERT_EQ(reg.Register(MakeRamp(a), false, "game"), RampRegisterResult::Ok);
		EXPECT_EQ(reg.Register(MakeRamp(b), true, "dia"), RampRegisterResult::HashCollision);	// must not replace the game ramp
		EXPECT_EQ(reg.Count(), 1u);
		EXPECT_STREQ(reg.Find(StringCRC(a.c_str()))->mName.AsCStr(), a.c_str());
	}
}

// ---- RampSetLoader: roots ---------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampSetEdge, NoRootsAndDegenerateDiaRootsLoadNothing)
{
	SchemaTempDirectory dir("RampEdge_Degenerate");
	dir.Write("empty_dir/placeholder.txt", "x");
	dir.Write("ramps.json/inside.txt", "x");	// "ramps.json" is a DIRECTORY here
	RampSetLoader loader;
	loader.AddDiaRoot("");
	loader.AddDiaRoot(nullptr);
	loader.AddDiaRoot(dir.Path("does/not/exist").c_str());
	loader.AddDiaRoot(dir.Path("empty_dir").c_str());
	loader.AddDiaRoot(dir.Path("ramps.json").c_str());	// directory with no ramps.json inside
	loader.AddDiaRoot(dir.Path().c_str());				// directory whose "ramps.json" is a directory
	RampRegistry reg;
	const RampLoadStats s = loader.Load(reg);
	EXPECT_EQ(s.mFilesFound, 0u);
	EXPECT_EQ(s.mFilesFailed, 0u);
	EXPECT_EQ(reg.Count(), 0u);

	RampSetLoader bare;
	RampRegistry reg2;
	const RampLoadStats s2 = bare.Load(reg2);
	EXPECT_EQ(s2.mFilesFound, 0u);
	EXPECT_EQ(reg2.Count(), 0u);
}

TEST(DiaValueSchemaRampSetEdge, GameRootNullEmptyMissingAreSkipped)
{
	SchemaTempDirectory dir("RampEdge_GameRoots");
	dir.Write("a.ramps.json", RampFile(RampJson("g.one")));
	RampSetLoader loader;
	loader.AddGameRoot(nullptr);
	loader.AddGameRoot("");
	loader.AddGameRoot(dir.Path("missing").c_str());
	loader.AddGameRoot(dir.Path().c_str());
	RampRegistry reg;
	const RampLoadStats s = loader.Load(reg);
	EXPECT_EQ(s.mFilesFound, 1u);
	EXPECT_EQ(s.mRampsLoaded, 1u);
}

TEST(DiaValueSchemaRampSetEdge, GameFileNamedExactlyRampsJsonIsNotPickedUp)
{
	// The Dia-set name "ramps.json" must not leak into the game search: "*.ramps.json" needs a name before the dot.
	SchemaTempDirectory game("RampEdge_RampsJsonGame");
	game.Write("ramps.json", RampFile(RampJson("leak.one")));
	game.Write("sub/ramps.json", RampFile(RampJson("leak.two")));
	game.Write(".ramps.json", RampFile(RampJson("leak.three")));	// exactly the suffix: no name before the dot
	game.Write("A.RAMPS.JSON", RampFile(RampJson("leak.four")));	// case-sensitive
	game.Write("real.ramps.json", RampFile(RampJson("real.one")));
	RampSetLoader loader;
	loader.AddGameRoot(game.Path().c_str());
	RampRegistry reg;
	const RampLoadStats s = loader.Load(reg);
	EXPECT_EQ(s.mFilesFound, 1u);
	EXPECT_EQ(reg.Count(), 1u);
	EXPECT_NE(reg.Find(StringCRC("real.one")), nullptr);
	EXPECT_EQ(reg.Find(StringCRC("leak.one")), nullptr);
}

TEST(DiaValueSchemaRampSetEdge, SameDiaRootTwiceLoadsTheFileOnce)
{
	// FIX 2: the same Dia root added twice made the second load reject every ramp (DuplicateId).
	SchemaTempDirectory dia("RampEdge_DiaTwice");
	dia.Write("ramps.json", RampFile(RampJson("dia.one") + "," + RampJson("dia.two")));
	RampSetLoader loader;
	loader.AddDiaRoot(dia.Path().c_str());
	loader.AddDiaRoot(dia.Path().c_str());
	RampRegistry reg;
	const RampLoadStats s = loader.Load(reg);
	EXPECT_EQ(s.mFilesFound, 1u);
	EXPECT_EQ(s.mFilesLoaded, 1u);
	EXPECT_EQ(s.mRampsLoaded, 2u);
	EXPECT_EQ(s.mRampsRejected, 0u);
	EXPECT_EQ(reg.Count(), 2u);
}

TEST(DiaValueSchemaRampSetEdge, DiaRootSpelledDifferentlyIsStillTheSameFile)
{
	SchemaTempDirectory dia("RampEdge_DiaSpelling");
	dia.Write("sub/ramps.json", RampFile(RampJson("dia.one")));
	std::string backslashed = dia.Path("sub") + "/";
	for (char& c : backslashed) { if (c == '/') { c = '\\'; } }
	RampSetLoader loader;
	loader.AddDiaRoot(dia.Path("sub").c_str());
	loader.AddDiaRoot((dia.Path("sub") + "/").c_str());
	loader.AddDiaRoot(backslashed.c_str());
	loader.AddDiaRoot((dia.Path() + "/sub/../sub/.").c_str());
	RampRegistry reg;
	const RampLoadStats s = loader.Load(reg);
	EXPECT_EQ(s.mFilesFound, 1u);
	EXPECT_EQ(s.mRampsRejected, 0u);
	EXPECT_EQ(reg.Count(), 1u);
}

TEST(DiaValueSchemaRampSetEdge, TwoDifferentDiaRootsWithTheSameIdFirstWinsPin)
{
	// PIN: distinct Dia files are not merged; the second one's clashing ramps count as rejected.
	SchemaTempDirectory a("RampEdge_DiaA"), b("RampEdge_DiaB");
	a.Write("ramps.json", RampFile(RampJson("shared", "#111111", "#222222")));
	b.Write("ramps.json", RampFile(RampJson("shared", "#333333", "#444444") + "," + RampJson("only.b")));
	RampSetLoader loader;
	loader.AddDiaRoot(a.Path().c_str());
	loader.AddDiaRoot(b.Path().c_str());
	RampRegistry reg;
	const RampLoadStats s = loader.Load(reg);
	EXPECT_EQ(s.mFilesFound, 2u);
	EXPECT_EQ(s.mRampsLoaded, 2u);
	EXPECT_EQ(s.mRampsRejected, 1u);
	EXPECT_EQ(FirstStop(reg, "shared"), 0x111111FFu);
}

TEST(DiaValueSchemaRampSetEdge, ReversedGameRootOrderGivesTheSameWinner)
{
	SchemaTempDirectory game("RampEdge_Reverse");
	game.Write("root_b/z.ramps.json", RampFile(RampJson("dup", "#bbbbbb", "#cccccc")));
	game.Write("root_a/y.ramps.json", RampFile(RampJson("dup", "#aaaaaa", "#cccccc")));
	for (int pass = 0; pass < 2; ++pass)
	{
		RampSetLoader loader;
		if (pass == 0) { loader.AddGameRoot(game.Path("root_b").c_str()); loader.AddGameRoot(game.Path("root_a").c_str()); }
		else           { loader.AddGameRoot(game.Path("root_a").c_str()); loader.AddGameRoot(game.Path("root_b").c_str()); }
		RampRegistry reg;
		const RampLoadStats s = loader.Load(reg);
		EXPECT_EQ(s.mRampsLoaded, 1u) << pass;
		EXPECT_EQ(s.mRampsRejected, 1u) << pass;
		EXPECT_EQ(FirstStop(reg, "dup"), 0xAAAAAAFFu) << "pass " << pass << ": sorted full path wins, not root order";
	}
}

TEST(DiaValueSchemaRampSetEdge, OverlappingGameRootsLoadEachFileOnce)
{
	SchemaTempDirectory game("RampEdge_Overlap");
	game.Write("top.ramps.json", RampFile(RampJson("o.one")));
	game.Write("sub/deep.ramps.json", RampFile(RampJson("o.two")));
	RampSetLoader loader;
	loader.AddGameRoot(game.Path().c_str());
	loader.AddGameRoot(game.Path("sub").c_str());
	loader.AddGameRoot(game.Path().c_str());
	RampRegistry reg;
	const RampLoadStats s = loader.Load(reg);
	EXPECT_EQ(s.mFilesFound, 2u);
	EXPECT_EQ(s.mRampsRejected, 0u);
	EXPECT_EQ(reg.Count(), 2u);
}

TEST(DiaValueSchemaRampSetEdge, StatsInvariantFoundEqualsLoadedPlusFailed)
{
	SchemaTempDirectory dia("RampEdge_InvDia"), game("RampEdge_InvGame");
	dia.Write("ramps.json", RampFile(RampJson("dia.one")));
	game.Write("1.ramps.json", RampFile(RampJson("dia.one") + "," + RampJson("g.one")));	// one clash
	game.Write("2.ramps.json", "{ broken");
	game.Write("3.ramps.json", "");
	game.Write("4.ramps.json", RampFile(R"({"id":"BAD ID","kind":"continuous","stops":[]},)" + RampJson("g.two")));	// one bad ramp
	RampSetLoader loader;
	loader.AddDiaRoot(dia.Path().c_str());
	loader.AddGameRoot(game.Path().c_str());
	RampRegistry reg;
	const RampLoadStats s = loader.Load(reg);
	EXPECT_EQ(s.mFilesFound, 5u);
	EXPECT_EQ(s.mFilesLoaded, 3u);
	EXPECT_EQ(s.mFilesFailed, 2u);
	EXPECT_EQ(s.mFilesFound, s.mFilesLoaded + s.mFilesFailed);
	EXPECT_EQ(s.mRampsLoaded, 3u);		// dia.one, g.one, g.two
	EXPECT_EQ(s.mRampsRejected, 2u);	// the clash + the bad ramp
	EXPECT_EQ(reg.Count(), s.mRampsLoaded);
}

TEST(DiaValueSchemaRampSetEdge, LoadTwiceIntoTwoRegistriesIsIdentical)
{
	SchemaTempDirectory dia("RampEdge_TwiceDia"), game("RampEdge_TwiceGame");
	dia.Write("ramps.json", RampFile(RampJson("dia.one")));
	game.Write("a.ramps.json", RampFile(RampJson("g.one") + "," + RampJson("dia.one")));
	game.Write("b.ramps.json", "{ broken");
	RampSetLoader loader;
	loader.AddDiaRoot(dia.Path().c_str());
	loader.AddGameRoot(game.Path().c_str());
	RampRegistry r1, r2;
	const RampLoadStats s1 = loader.Load(r1);
	const RampLoadStats s2 = loader.Load(r2);
	EXPECT_EQ(s1.mFilesFound, s2.mFilesFound);
	EXPECT_EQ(s1.mFilesLoaded, s2.mFilesLoaded);
	EXPECT_EQ(s1.mFilesFailed, s2.mFilesFailed);
	EXPECT_EQ(s1.mRampsLoaded, s2.mRampsLoaded);
	EXPECT_EQ(s1.mRampsRejected, s2.mRampsRejected);
	EXPECT_EQ(r1.Count(), r2.Count());
}

TEST(DiaValueSchemaRampSetEdge, LoadTwiceIntoTheSameRegistryRejectsTheSecondPass)
{
	SchemaTempDirectory game("RampEdge_SameReg");
	game.Write("a.ramps.json", RampFile(RampJson("g.one") + "," + RampJson("g.two")));
	RampSetLoader loader;
	loader.AddGameRoot(game.Path().c_str());
	RampRegistry reg;
	EXPECT_EQ(loader.Load(reg).mRampsLoaded, 2u);
	const RampLoadStats second = loader.Load(reg);
	EXPECT_EQ(second.mRampsLoaded, 0u);
	EXPECT_EQ(second.mRampsRejected, 2u);
	EXPECT_EQ(second.mFilesLoaded, 1u);
	EXPECT_EQ(reg.Count(), 2u);
}

TEST(DiaValueSchemaRampSetEdge, LoadIntoPublishedRegistryRejectsRampsButLoadsFiles)
{
	SchemaTempDirectory game("RampEdge_Published");
	game.Write("a.ramps.json", RampFile(RampJson("g.one")));
	RampSetLoader loader;
	loader.AddGameRoot(game.Path().c_str());
	RampRegistry reg;
	reg.Publish();
	const RampLoadStats s = loader.Load(reg);
	EXPECT_EQ(s.mFilesLoaded, 1u);
	EXPECT_EQ(s.mRampsLoaded, 0u);
	EXPECT_EQ(s.mRampsRejected, 1u);
	EXPECT_EQ(reg.Count(), 0u);
}

TEST(DiaValueSchemaRampSetEdge, CollidingRampIdsInDifferentFilesAreRejectedAsCollision)
{
	std::string a, b;
	ASSERT_TRUE(FindCollidingNames(a, b));
	SchemaTempDirectory game("RampEdge_Collide");
	game.Write("1.ramps.json", RampFile(RampJson(a, "#111111", "#222222")));
	game.Write("2.ramps.json", RampFile(RampJson(b, "#333333", "#444444") + "," + RampJson("fine.one")));
	RampSetLoader loader;
	loader.AddGameRoot(game.Path().c_str());
	RampRegistry reg;
	const RampLoadStats s = loader.Load(reg);
	EXPECT_EQ(s.mRampsLoaded, 2u);
	EXPECT_EQ(s.mRampsRejected, 1u);
	EXPECT_EQ(FirstStop(reg, a.c_str()), 0x111111FFu);
	EXPECT_STREQ(reg.Find(StringCRC(b.c_str()))->mName.AsCStr(), a.c_str());
}

// ---- RampSetLoader: encodings and unusual names ------------------------------------------------------------------------

TEST(DiaValueSchemaRampSetEdge, BomAndCrlfRampFilesOnDiskLoad)
{
	SchemaTempDirectory dia("RampEdge_BomDia"), game("RampEdge_BomGame");
	const std::string crlf = ReplaceAll(RampFile(RampJson("bom.dia")), ",", ",\r\n");
	dia.Write("ramps.json", std::string("\xEF\xBB\xBF") + crlf);
	game.Write("bom.ramps.json", std::string("\xEF\xBB\xBF") + RampFile(RampJson("bom.game")));
	game.Write("crlf.ramps.json", ReplaceAll(ReplaceAll(RampFile(RampJson("crlf.game")), ",", ",\r\n"), "{", "{\r\n"));
	RampSetLoader loader;
	loader.AddDiaRoot(dia.Path().c_str());
	loader.AddGameRoot(game.Path().c_str());
	RampRegistry reg;
	const RampLoadStats s = loader.Load(reg);
	EXPECT_EQ(s.mFilesFailed, 0u);
	EXPECT_EQ(s.mFilesLoaded, 3u);
	EXPECT_EQ(reg.Count(), 3u);
}

TEST(DiaValueSchemaRampSetEdge, EmptyWhitespaceAndNulFilesCountFailed)
{
	SchemaTempDirectory game("RampEdge_EmptyFiles");
	game.Write("a.ramps.json", "");
	game.Write("b.ramps.json", " \r\n\t ");
	game.Write("c.ramps.json", std::string(32, '\0'));
	game.Write("d.ramps.json", RampFile(RampJson("ok.one")));
	RampSetLoader loader;
	loader.AddGameRoot(game.Path().c_str());
	RampRegistry reg;
	const RampLoadStats s = loader.Load(reg);
	EXPECT_EQ(s.mFilesFound, 4u);
	EXPECT_EQ(s.mFilesFailed, 3u);
	EXPECT_EQ(s.mFilesLoaded, 1u);
}

TEST(DiaValueSchemaRampSetEdge, EmptyDiaRampsJsonCountsFailed)
{
	SchemaTempDirectory dia("RampEdge_EmptyDia");
	dia.Write("ramps.json", "");
	RampSetLoader loader;
	loader.AddDiaRoot(dia.Path().c_str());
	RampRegistry reg;
	const RampLoadStats s = loader.Load(reg);
	EXPECT_EQ(s.mFilesFound, 1u);
	EXPECT_EQ(s.mFilesFailed, 1u);
}

TEST(DiaValueSchemaRampSetEdge, PrintfDirectivesInDirectoriesAndFileNames)
{
	SchemaTempDirectory dia("RampEdge_%s%d%n_dia"), game("RampEdge_100%_game");
	dia.Write("ramps.json", RampFile(RampJson("pct.dia")));
	game.Write("a%s%d%n.ramps.json", RampFile(RampJson("pct.one") + "," + RampJson("pct.dia")));
	game.Write("b%n%n.ramps.json", RampFile(RampJson("pct.one")));
	game.Write("c%s.ramps.json", "{ broken %s");
	RampSetLoader loader;
	loader.AddDiaRoot(dia.Path().c_str());
	loader.AddGameRoot(game.Path().c_str());
	RampRegistry reg;
	const RampLoadStats s = loader.Load(reg);
	EXPECT_EQ(s.mFilesFound, 4u);
	EXPECT_EQ(s.mFilesFailed, 1u);
	EXPECT_EQ(s.mRampsLoaded, 2u);		// pct.dia (Dia) + pct.one (first game file)
	EXPECT_EQ(s.mRampsRejected, 2u);	// pct.dia clash + pct.one duplicate
	EXPECT_EQ(reg.Count(), 2u);
}

TEST(DiaValueSchemaRampSetEdge, TwoHundredGameFilesStress)
{
	SchemaTempDirectory game("RampEdge_Stress200");
	for (int i = 0; i < 200; ++i)
	{
		char rel[48];
		snprintf(rel, sizeof(rel), "d%d/r%03d.ramps.json", i % 5, i);
		game.Write(rel, RampFile(RampJson("stress.r" + std::to_string(i))));
	}
	RampSetLoader loader;
	loader.AddGameRoot(game.Path().c_str());
	RampRegistry reg;
	const RampLoadStats s = loader.Load(reg);
	EXPECT_EQ(s.mFilesFound, 200u);
	EXPECT_EQ(s.mFilesLoaded, 200u);
	EXPECT_EQ(s.mRampsLoaded, 200u);
	EXPECT_EQ(reg.Count(), 200u);
}

// ---- ValueSchemaService (ramp half) -----------------------------------------------------------------------------------

TEST(DiaValueSchemaServiceEdge, FindRampAndRampCountWithRegistry)
{
	ValueSchemaRegistry schemas;
	schemas.Publish();
	RampRegistry ramps;
	ramps.Register(MakeRamp("a", 0x010203FFu), false, "f");
	ramps.Register(MakeRamp("b"), true, "f");
	ramps.Publish();
	ValueSchemaService svc(schemas, &ramps);
	EXPECT_EQ(svc.RampCount(), 2u);
	ASSERT_NE(svc.FindRamp(StringCRC("a")), nullptr);
	EXPECT_EQ(svc.FindRamp(StringCRC("a"))->mStops[0].mRgba8, 0x010203FFu);
	EXPECT_EQ(svc.FindRamp(StringCRC("a")), ramps.Find(StringCRC("a")));
	EXPECT_EQ(svc.FindRamp(StringCRC("zzz")), nullptr);
	EXPECT_EQ(svc.FindRamp(StringCRC::kZero), nullptr);
}

TEST(DiaValueSchemaServiceEdge, NullRampRegistryConstructorGivesNoRamps)
{
	ValueSchemaRegistry schemas;
	schemas.Register(MakeValueInfo("f", "k"));
	schemas.Publish();
	ValueSchemaService svc(schemas, nullptr);
	EXPECT_EQ(svc.FindRamp(StringCRC("a")), nullptr);
	EXPECT_EQ(svc.RampCount(), 0u);
	EXPECT_EQ(svc.Count(), 1u);
	EXPECT_NE(svc.Find(StringCRC("f/k")), nullptr);
}

TEST(DiaValueSchemaServiceEdge, ServiceReadsTheLiveRampRegistryPin)
{
	// PIN: the service references (does not snapshot) the ramp registry, so ramps registered before Publish show up.
	ValueSchemaRegistry schemas;
	schemas.Publish();
	RampRegistry ramps;
	ValueSchemaService svc(schemas, &ramps);
	EXPECT_EQ(svc.RampCount(), 0u);
	ramps.Register(MakeRamp("late"), false, "f");
	EXPECT_EQ(svc.RampCount(), 1u);
	EXPECT_NE(svc.FindRamp(StringCRC("late")), nullptr);
}

TEST(DiaValueSchemaServiceEdge, ServiceOverEmptyRegistriesReturnsEmptyResults)
{
	ValueSchemaRegistry schemas;
	schemas.Publish();
	RampRegistry ramps;
	ramps.Publish();
	ValueSchemaService svc(schemas, &ramps);
	EXPECT_EQ(svc.Count(), 0u);
	EXPECT_EQ(svc.RampCount(), 0u);
	EXPECT_EQ(svc.Find(StringCRC("a/b")), nullptr);
	EXPECT_EQ(svc.Find(StringCRC("a"), StringCRC("b")), nullptr);
	EXPECT_EQ(svc.ForFormat(StringCRC("a")).mCount, 0u);
	EXPECT_EQ(svc.FindEvent(StringCRC("e")), nullptr);
	EXPECT_EQ(svc.FindRamp(StringCRC("a")), nullptr);
}

TEST(DiaValueSchemaServiceEdge, ConcurrentReadersSeeConsistentRampsSchemasAndEvents)
{
	constexpr int kRampCount = 64;
	constexpr int kFormats = 8;
	constexpr int kPerFormat = 25;
	constexpr int kReaders = 4;
	constexpr int kIterations = 8000;

	ValueSchemaRegistry schemas;
	for (int f = 0; f < kFormats; ++f)
	{
		for (int i = 0; i < kPerFormat; ++i)
		{
			const bool isEvent = (i % 5) == 0;
			schemas.Register(MakeValueInfo(("cf" + std::to_string(f)).c_str(), ((isEvent ? "ev" : "k") + std::to_string(f) + "_" + std::to_string(i)).c_str(),
				isEvent ? ValueKind::Event : ValueKind::Parameter));
		}
	}
	schemas.Publish();
	RampRegistry ramps;
	for (int i = 0; i < kRampCount; ++i) { ramps.Register(MakeRamp("cr" + std::to_string(i), 0x01000000u + static_cast<uint32_t>(i)), (i % 2) == 0, "f"); }
	ramps.Publish();
	ValueSchemaService svc(schemas, &ramps);
	ASSERT_EQ(svc.RampCount(), static_cast<unsigned int>(kRampCount));

	std::vector<StringCRC> rampIds, formats, fullKeys, eventKeys;
	for (int i = 0; i < kRampCount; ++i) { rampIds.push_back(StringCRC(("cr" + std::to_string(i)).c_str())); }
	for (int f = 0; f < kFormats; ++f)
	{
		formats.push_back(StringCRC(("cf" + std::to_string(f)).c_str()));
		for (int i = 0; i < kPerFormat; ++i)
		{
			const bool isEvent = (i % 5) == 0;
			const std::string key = (isEvent ? "ev" : "k") + std::to_string(f) + "_" + std::to_string(i);
			fullKeys.push_back(StringCRC(("cf" + std::to_string(f) + "/" + key).c_str()));
			if (isEvent) { eventKeys.push_back(StringCRC(key.c_str())); }
		}
	}
	const StringCRC unknown("does.not.exist");

	std::atomic<bool> go{ false };
	std::atomic<unsigned int> failures{ 0 };
	auto reader = [&](int seed)
	{
		while (!go.load(std::memory_order_acquire)) { std::this_thread::yield(); }
		unsigned int bad = 0;
		for (int it = 0; it < kIterations; ++it)
		{
			const size_t r = (static_cast<size_t>(it) * 31u + static_cast<size_t>(seed) * 7u) % rampIds.size();
			const RampData* ramp = svc.FindRamp(rampIds[r]);
			if (ramp == nullptr || ramp->mStops[0].mRgba8 != 0x01000000u + static_cast<uint32_t>(r) || !(ramp->mId == rampIds[r])) { ++bad; }
			if (svc.FindRamp(unknown) != nullptr) { ++bad; }
			if (svc.RampCount() != static_cast<unsigned int>(kRampCount)) { ++bad; }

			const size_t k = (static_cast<size_t>(it) * 13u + static_cast<size_t>(seed)) % fullKeys.size();
			if (svc.Find(fullKeys[k]) == nullptr) { ++bad; }
			if (svc.Find(unknown) != nullptr) { ++bad; }
			if (svc.Count() != static_cast<unsigned int>(kFormats * kPerFormat)) { ++bad; }

			if ((it % 20) == 0)
			{
				const ValueInfoSpan span = svc.ForFormat(formats[static_cast<size_t>(it) % formats.size()]);
				if (span.mCount != static_cast<unsigned int>(kPerFormat)) { ++bad; }
				const ValueInfo* e = svc.FindEvent(eventKeys[static_cast<size_t>(it) % eventKeys.size()]);
				if (e == nullptr || e->mKind != ValueKind::Event) { ++bad; }
				if (svc.FindEvent(unknown) != nullptr) { ++bad; }
			}
		}
		if (bad != 0) { failures.fetch_add(bad); }
	};
	std::vector<std::thread> threads;
	for (int t = 0; t < kReaders; ++t) { threads.emplace_back(reader, t); }
	go.store(true, std::memory_order_release);
	for (auto& t : threads) { t.join(); }
	EXPECT_EQ(failures.load(), 0u);
}
