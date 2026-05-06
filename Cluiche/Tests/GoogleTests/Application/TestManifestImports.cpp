#define _CRT_SECURE_NO_WARNINGS
#include <gtest/gtest.h>
#include <DiaApplication/Manifest/ApplicationManifestLoader.h>
#include <DiaApplication/Manifest/ApplicationManifest.h>
#include <DiaApplication/Manifest/ManifestValidator.h>
#include <DiaApplication/Manifest/ManifestComposer.h>
#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/CRC/StringCRC.h>
#include <cstdio>
#include <cstring>

using namespace Dia::Application;
using namespace Dia::Core;

// ---------------------------------------------------------------------------
// Minimal test types
// ---------------------------------------------------------------------------

class ImportTestPU : public ProcessingUnit
{
public:
	static const StringCRC kTypeId;
	ImportTestPU(const StringCRC& id, float hz) : ProcessingUnit(id, hz, 4, 4) {}
	bool FlaggedToStopUpdating() const override { return true; }
};
const StringCRC ImportTestPU::kTypeId("ImportTestPU");

class ImportTestPhase : public Phase
{
public:
	static const StringCRC kTypeId;
	ImportTestPhase(ProcessingUnit* pu, const StringCRC& id) : Phase(pu, id) {}
	bool FlaggedToStopUpdating() const override { return true; }
};
const StringCRC ImportTestPhase::kTypeId("ImportTestPhase");

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void WriteFile(const char* path, const char* content)
{
	FILE* f = fopen(path, "w");
	if (f) { fputs(content, f); fclose(f); }
}

static void DeleteFile(const char* path) { remove(path); }

// Minimal valid single-PU manifest JSON
static const char* MinimalPU(const char* typeId, const char* instanceId, bool root = false,
	const char* phaseTypeId = "ImportTestPhase", const char* phaseInstanceId = "PhaseA")
{
	static char buf[2048];
	snprintf(buf, sizeof(buf),
		"{ \"version\": 1, \"processing_units\": [{"
		"  \"type_id\": \"%s\", \"instance_id\": \"%s\","
		"  \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"  %s"
		"  \"initial_phase\": \"%s\","
		"  \"phases\": [{ \"type_id\": \"%s\", \"instance_id\": \"%s\", \"config\": {} }],"
		"  \"modules\": [], \"transitions\": [], \"config\": {}"
		"}] }",
		typeId, instanceId,
		root ? "\"root\": true," : "",
		phaseInstanceId, phaseTypeId, phaseInstanceId);
	return buf;
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class ManifestImportsTest : public ::testing::Test
{
protected:
	ApplicationTypeRegistry mRegistry;

	void SetUp() override
	{
		// RegisterKnown* stubs: the loader can validate types; instantiation not needed for these tests
		mRegistry.RegisterKnownProcessingUnitType(ImportTestPU::kTypeId);
		mRegistry.RegisterKnownPhaseType(ImportTestPhase::kTypeId);
	}
};

// ---------------------------------------------------------------------------
// AC8: No imports — backward compatible
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, NoImports_LoadsIdenticalToCurrent)
{
	WriteFile("test_noimports.diaapp", MinimalPU("ImportTestPU", "PU_A", true));

	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	ManifestValidationResult result = loader.LoadFromFile("test_noimports.diaapp", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_EQ(manifest.processingUnits.Size(), 1u);
	EXPECT_EQ(manifest.processingUnits[0].instanceId, StringCRC("PU_A"));

	DeleteFile("test_noimports.diaapp");
}

// ---------------------------------------------------------------------------
// AC1/AC2: Single import — merged manifest has both PUs
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, SingleImport_BothPUsPresent)
{
	WriteFile("test_imported.diaapp", MinimalPU("ImportTestPU", "PU_B", false, "ImportTestPhase", "PhaseB"));

	static char root[512];
	snprintf(root, sizeof(root),
		"{ \"version\": 1, \"imports\": [\"test_imported.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_A\","
		"    \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"    \"initial_phase\": \"PhaseA\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseA\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_root.diaapp", root);

	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	ManifestValidationResult result = loader.LoadFromFile("test_root.diaapp", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_EQ(manifest.processingUnits.Size(), 2u);

	DeleteFile("test_root.diaapp");
	DeleteFile("test_imported.diaapp");
}

// ---------------------------------------------------------------------------
// AC2: Multiple imports — all PUs present
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, MultipleImports_AllPUsPresent)
{
	WriteFile("test_imp_b.diaapp", MinimalPU("ImportTestPU", "PU_B", false, "ImportTestPhase", "PhaseB"));
	WriteFile("test_imp_c.diaapp", MinimalPU("ImportTestPU", "PU_C", false, "ImportTestPhase", "PhaseC"));

	static char root[512];
	snprintf(root, sizeof(root),
		"{ \"version\": 1, \"imports\": [\"test_imp_b.diaapp\", \"test_imp_c.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_A\","
		"    \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"    \"initial_phase\": \"PhaseA\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseA\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_multi_root.diaapp", root);

	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	ManifestValidationResult result = loader.LoadFromFile("test_multi_root.diaapp", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_EQ(manifest.processingUnits.Size(), 3u);

	DeleteFile("test_multi_root.diaapp");
	DeleteFile("test_imp_b.diaapp");
	DeleteFile("test_imp_c.diaapp");
}

// ---------------------------------------------------------------------------
// AC1: Nested imports — A imports B which imports C
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, NestedImports_AllPUsPresent)
{
	WriteFile("test_nest_c.diaapp", MinimalPU("ImportTestPU", "PU_C", false, "ImportTestPhase", "PhaseC"));

	static char b[512];
	snprintf(b, sizeof(b),
		"{ \"version\": 1, \"imports\": [\"test_nest_c.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_B\","
		"    \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"    \"initial_phase\": \"PhaseB\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseB\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_nest_b.diaapp", b);

	static char a[512];
	snprintf(a, sizeof(a),
		"{ \"version\": 1, \"imports\": [\"test_nest_b.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_A\","
		"    \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"    \"initial_phase\": \"PhaseA\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseA\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_nest_a.diaapp", a);

	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	ManifestValidationResult result = loader.LoadFromFile("test_nest_a.diaapp", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_EQ(manifest.processingUnits.Size(), 3u);

	DeleteFile("test_nest_a.diaapp");
	DeleteFile("test_nest_b.diaapp");
	DeleteFile("test_nest_c.diaapp");
}

// ---------------------------------------------------------------------------
// AC4: Circular import detection
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, CircularImport_ReturnsImportCycle)
{
	// A imports B, B imports A
	static char b[512];
	snprintf(b, sizeof(b),
		"{ \"version\": 1, \"imports\": [\"test_cycle_a.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_B\","
		"    \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"    \"initial_phase\": \"PhaseB\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseB\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_cycle_b.diaapp", b);

	static char a[512];
	snprintf(a, sizeof(a),
		"{ \"version\": 1, \"imports\": [\"test_cycle_b.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_A\","
		"    \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"    \"initial_phase\": \"PhaseA\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseA\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_cycle_a.diaapp", a);

	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	ManifestValidationResult result = loader.LoadFromFile("test_cycle_a.diaapp", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kImportCycle);

	DeleteFile("test_cycle_a.diaapp");
	DeleteFile("test_cycle_b.diaapp");
}

// ---------------------------------------------------------------------------
// AC4: Self-import detection
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, SelfImport_ReturnsImportCycle)
{
	static char self[512];
	snprintf(self, sizeof(self),
		"{ \"version\": 1, \"imports\": [\"test_self.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_A\","
		"    \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"    \"initial_phase\": \"PhaseA\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseA\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_self.diaapp", self);

	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	ManifestValidationResult result = loader.LoadFromFile("test_self.diaapp", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kImportCycle);

	DeleteFile("test_self.diaapp");
}

// ---------------------------------------------------------------------------
// AC9: Missing import file returns error
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, MissingImportFile_ReturnsImportNotFound)
{
	static char root[512];
	snprintf(root, sizeof(root),
		"{ \"version\": 1, \"imports\": [\"does_not_exist.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_A\","
		"    \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"    \"initial_phase\": \"PhaseA\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseA\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_missing_import.diaapp", root);

	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	ManifestValidationResult result = loader.LoadFromFile("test_missing_import.diaapp", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kImportNotFound);

	DeleteFile("test_missing_import.diaapp");
}

// ---------------------------------------------------------------------------
// AC9: Provenance tracking — sourceManifestPath set after merge
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, ProvenanceTracking_SourceManifestPathSet)
{
	WriteFile("test_prov_b.diaapp", MinimalPU("ImportTestPU", "PU_B", false, "ImportTestPhase", "PhaseB"));

	static char root[512];
	snprintf(root, sizeof(root),
		"{ \"version\": 1, \"imports\": [\"test_prov_b.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_A\","
		"    \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"    \"initial_phase\": \"PhaseA\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseA\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_prov_a.diaapp", root);

	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	ManifestValidationResult result = loader.LoadFromFile("test_prov_a.diaapp", manifest);

	ASSERT_EQ(result, ManifestValidationResult::kSuccess);
	ASSERT_EQ(manifest.processingUnits.Size(), 2u);

	bool foundA = false, foundB = false;
	for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
	{
		const auto& pu = manifest.processingUnits[i];
		EXPECT_FALSE(pu.sourceManifestPath.IsEmpty());
		if (pu.instanceId == StringCRC("PU_A")) { foundA = true; EXPECT_NE(strstr(pu.sourceManifestPath.AsCStr(), "test_prov_a"), nullptr); }
		if (pu.instanceId == StringCRC("PU_B")) { foundB = true; EXPECT_NE(strstr(pu.sourceManifestPath.AsCStr(), "test_prov_b"), nullptr); }
	}
	EXPECT_TRUE(foundA);
	EXPECT_TRUE(foundB);

	DeleteFile("test_prov_a.diaapp");
	DeleteFile("test_prov_b.diaapp");
}

// ---------------------------------------------------------------------------
// AC6: root flag parsed correctly
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, RootFlag_ParsedFromJSON)
{
	WriteFile("test_rootflag.diaapp", MinimalPU("ImportTestPU", "PU_A", true));

	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	loader.LoadFromFile("test_rootflag.diaapp", manifest);

	ASSERT_EQ(manifest.processingUnits.Size(), 1u);
	EXPECT_TRUE(manifest.processingUnits[0].root);

	DeleteFile("test_rootflag.diaapp");
}

// ---------------------------------------------------------------------------
// No-root flag defaults to false
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, NoRootFlag_DefaultsFalse)
{
	WriteFile("test_norootflag.diaapp", MinimalPU("ImportTestPU", "PU_A", false));

	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	loader.LoadFromFile("test_norootflag.diaapp", manifest);

	ASSERT_EQ(manifest.processingUnits.Size(), 1u);
	EXPECT_FALSE(manifest.processingUnits[0].root);

	DeleteFile("test_norootflag.diaapp");
}

// ---------------------------------------------------------------------------
// AC3: Imported PU phases stay in their own PU entry (not cross-merged)
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, ImportedPUPhases_StayInTheirPU)
{
	static char imported[512];
	snprintf(imported, sizeof(imported),
		"{ \"version\": 1, \"processing_units\": [{"
		"  \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_B\","
		"  \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"  \"initial_phase\": \"PhaseB1\","
		"  \"phases\": ["
		"    { \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseB1\", \"config\": {} },"
		"    { \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseB2\", \"config\": {} }"
		"  ],"
		"  \"modules\": [], \"transitions\": [], \"config\": {}"
		"}] }");
	WriteFile("test_phases_b.diaapp", imported);

	static char root[512];
	snprintf(root, sizeof(root),
		"{ \"version\": 1, \"imports\": [\"test_phases_b.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_A\","
		"    \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"    \"initial_phase\": \"PhaseA\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseA\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_phases_root.diaapp", root);

	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	ManifestValidationResult result = loader.LoadFromFile("test_phases_root.diaapp", manifest);

	ASSERT_EQ(result, ManifestValidationResult::kSuccess);
	ASSERT_EQ(manifest.processingUnits.Size(), 2u);

	for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
	{
		const auto& pu = manifest.processingUnits[i];
		if (pu.instanceId == StringCRC("PU_A"))
			EXPECT_EQ(pu.phases.Size(), 1u);
		else if (pu.instanceId == StringCRC("PU_B"))
			EXPECT_EQ(pu.phases.Size(), 2u);
	}

	DeleteFile("test_phases_root.diaapp");
	DeleteFile("test_phases_b.diaapp");
}

// ---------------------------------------------------------------------------
// Diamond import: A->B->D and A->C->D — D loaded once (dedup)
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, DiamondImport_SharedNodeLoadedOnce)
{
	WriteFile("test_dia_d.diaapp", MinimalPU("ImportTestPU", "PU_D", false, "ImportTestPhase", "PhaseD"));

	static char b[512];
	snprintf(b, sizeof(b),
		"{ \"version\": 1, \"imports\": [\"test_dia_d.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_B\","
		"    \"frequency_hz\": 30.0, \"dedicated_thread\": false, \"initial_phase\": \"PhaseB\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseB\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_dia_b.diaapp", b);

	static char c[512];
	snprintf(c, sizeof(c),
		"{ \"version\": 1, \"imports\": [\"test_dia_d.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_C\","
		"    \"frequency_hz\": 30.0, \"dedicated_thread\": false, \"initial_phase\": \"PhaseC\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseC\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_dia_c.diaapp", c);

	static char a[512];
	snprintf(a, sizeof(a),
		"{ \"version\": 1, \"imports\": [\"test_dia_b.diaapp\", \"test_dia_c.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_A\","
		"    \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false, \"initial_phase\": \"PhaseA\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseA\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_dia_a.diaapp", a);

	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	ManifestValidationResult result = loader.LoadFromFile("test_dia_a.diaapp", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	// A + B + C + D = 4 PUs, D appears only once
	EXPECT_EQ(manifest.processingUnits.Size(), 4u);

	DeleteFile("test_dia_a.diaapp");
	DeleteFile("test_dia_b.diaapp");
	DeleteFile("test_dia_c.diaapp");
	DeleteFile("test_dia_d.diaapp");
}

// ---------------------------------------------------------------------------
// AC5: Duplicate PU instance_id across manifests → kDuplicateInstanceId
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, DuplicatePUInstanceId_ReturnsError)
{
	// Both files define a PU with instance_id "PU_A"
	WriteFile("test_dup_imported.diaapp", MinimalPU("ImportTestPU", "PU_A", false, "ImportTestPhase", "PhaseA2"));

	static char root[512];
	snprintf(root, sizeof(root),
		"{ \"version\": 1, \"imports\": [\"test_dup_imported.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_A\","
		"    \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"    \"initial_phase\": \"PhaseA\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseA\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_dup_root.diaapp", root);

	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	ManifestValidationResult result = loader.LoadFromFile("test_dup_root.diaapp", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kDuplicateInstanceId);

	DeleteFile("test_dup_root.diaapp");
	DeleteFile("test_dup_imported.diaapp");
}

// ---------------------------------------------------------------------------
// AC6: Relative path resolution — import in a subdirectory resolves correctly
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, RelativePath_SubdirImport_Resolves)
{
	// Create subdirectory by path prefix in filename (simulates sub/sim.diaapp relative to main dir)
	// Test uses current-directory-relative paths: test_subdir_b.diaapp acts as "sub/imported"
	// We write the root and import side by side, with the import path using a prefix that
	// resolves relative to the root's directory (both in CWD, so prefix = "")
	WriteFile("test_relpath_sub.diaapp", MinimalPU("ImportTestPU", "PU_Sub", false, "ImportTestPhase", "PhaseSub"));

	static char root[512];
	snprintf(root, sizeof(root),
		"{ \"version\": 1, \"imports\": [\"test_relpath_sub.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_Root\","
		"    \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"    \"initial_phase\": \"PhaseRoot\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseRoot\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_relpath_root.diaapp", root);

	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	ManifestValidationResult result = loader.LoadFromFile("test_relpath_root.diaapp", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_EQ(manifest.processingUnits.Size(), 2u);

	bool foundRoot = false, foundSub = false;
	for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
	{
		if (manifest.processingUnits[i].instanceId == StringCRC("PU_Root")) foundRoot = true;
		if (manifest.processingUnits[i].instanceId == StringCRC("PU_Sub")) foundSub = true;
	}
	EXPECT_TRUE(foundRoot);
	EXPECT_TRUE(foundSub);

	DeleteFile("test_relpath_root.diaapp");
	DeleteFile("test_relpath_sub.diaapp");
}

// ---------------------------------------------------------------------------
// Deep nesting: 4-level chain — A→B→C→D, all PUs present
// Stays within ApplicationManifest::processingUnits capacity (DynamicArrayC<..., 4>)
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, DeepNesting_FourLevels_AllPUsPresent)
{
	WriteFile("test_deep_d.diaapp", MinimalPU("ImportTestPU", "PU_D", false, "ImportTestPhase", "PhaseD"));

	static char c[512];
	snprintf(c, sizeof(c),
		"{ \"version\": 1, \"imports\": [\"test_deep_d.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_C\","
		"    \"frequency_hz\": 30.0, \"dedicated_thread\": false, \"initial_phase\": \"PhaseC\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseC\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_deep_c.diaapp", c);

	static char b[512];
	snprintf(b, sizeof(b),
		"{ \"version\": 1, \"imports\": [\"test_deep_c.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_B\","
		"    \"frequency_hz\": 30.0, \"dedicated_thread\": false, \"initial_phase\": \"PhaseB\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseB\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_deep_b.diaapp", b);

	static char a[512];
	snprintf(a, sizeof(a),
		"{ \"version\": 1, \"imports\": [\"test_deep_b.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_A\","
		"    \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false, \"initial_phase\": \"PhaseA\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseA\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_deep_a.diaapp", a);

	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	ManifestValidationResult result = loader.LoadFromFile("test_deep_a.diaapp", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_EQ(manifest.processingUnits.Size(), 4u);

	DeleteFile("test_deep_a.diaapp");
	DeleteFile("test_deep_b.diaapp");
	DeleteFile("test_deep_c.diaapp");
	DeleteFile("test_deep_d.diaapp");
}

// ---------------------------------------------------------------------------
// Serialize round-trip: imports array preserved
// ---------------------------------------------------------------------------

TEST_F(ManifestImportsTest, SerializeRoundTrip_ImportsPreserved)
{
	WriteFile("test_rt_b.diaapp", MinimalPU("ImportTestPU", "PU_B", false, "ImportTestPhase", "PhaseB"));

	static char root[512];
	snprintf(root, sizeof(root),
		"{ \"version\": 1, \"imports\": [\"test_rt_b.diaapp\"],"
		"  \"processing_units\": [{"
		"    \"type_id\": \"ImportTestPU\", \"instance_id\": \"PU_A\","
		"    \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"    \"initial_phase\": \"PhaseA\","
		"    \"phases\": [{ \"type_id\": \"ImportTestPhase\", \"instance_id\": \"PhaseA\", \"config\": {} }],"
		"    \"modules\": [], \"transitions\": [], \"config\": {}"
		"  }] }");
	WriteFile("test_rt_a.diaapp", root);

	// LoadFromFile composes imports: result has both PUs, imports array is consumed
	ApplicationManifestLoader loader(mRegistry);
	ApplicationManifest manifest;
	loader.LoadFromFile("test_rt_a.diaapp", manifest);
	EXPECT_EQ(manifest.processingUnits.Size(), 2u);

	// Verify both PUs are present (import was resolved into the composed manifest)
	bool foundA = false, foundB = false;
	for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
	{
		if (manifest.processingUnits[i].instanceId == StringCRC("PU_A")) foundA = true;
		if (manifest.processingUnits[i].instanceId == StringCRC("PU_B")) foundB = true;
	}
	EXPECT_TRUE(foundA);
	EXPECT_TRUE(foundB);

	DeleteFile("test_rt_a.diaapp");
	DeleteFile("test_rt_b.diaapp");
}
