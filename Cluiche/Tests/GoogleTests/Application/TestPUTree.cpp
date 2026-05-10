#include <gtest/gtest.h>
#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaApplicationFlow/ApplicationPhase.h>
#include <DiaApplicationFlow/ApplicationModule.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>

using namespace Dia::Application;
using namespace Dia::Core;

// ---------------------------------------------------------------------------
// Minimal test types
// ---------------------------------------------------------------------------

class TreeTestPU : public ProcessingUnit
{
public:
	static const StringCRC kTypeId;
	TreeTestPU(const StringCRC& id, float hz = -1.0f) : ProcessingUnit(id, hz, 4, 4) {}
	bool FlaggedToStopUpdating() const override { return mFlaggedStop; }

	void SetFlaggedStop(bool val) { mFlaggedStop = val; }

	bool mFlaggedStop = true;
};
const StringCRC TreeTestPU::kTypeId("TreeTestPU");

class TreeTestPhase : public Phase
{
public:
	static const StringCRC kTypeId;
	TreeTestPhase(ProcessingUnit* pu, const StringCRC& id) : Phase(pu, id) {}
	bool FlaggedToStopUpdating() const override { return true; }
};
const StringCRC TreeTestPhase::kTypeId("TreeTestPhase");

// Tracks destruction order
static std::vector<StringCRC> sDestructionOrder;

class DestructionTrackingPU : public ProcessingUnit
{
public:
	static const StringCRC kTypeId;
	DestructionTrackingPU(const StringCRC& id) : ProcessingUnit(id, -1.0f, 4, 4) {}
	~DestructionTrackingPU() { sDestructionOrder.push_back(GetUniqueId()); }
	bool FlaggedToStopUpdating() const override { return true; }
};
const StringCRC DestructionTrackingPU::kTypeId("DestructionTrackingPU");

// Helper: creates a UniquePtr<ProcessingUnit> from a derived type
static Dia::Core::UniquePtr<ProcessingUnit> MakeChildPU(const StringCRC& id)
{
	return Dia::Core::UniquePtr<ProcessingUnit>(new TreeTestPU(id));
}

static Dia::Core::UniquePtr<ProcessingUnit> MakeTrackingPU(const StringCRC& id)
{
	return Dia::Core::UniquePtr<ProcessingUnit>(new DestructionTrackingPU(id));
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class PUTreeTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		sDestructionOrder.clear();
	}
};

// ---------------------------------------------------------------------------
// AC1: AddChildProcessingUnit + GetChildren contains it
// ---------------------------------------------------------------------------
TEST_F(PUTreeTest, AddChild_AppearsInChildren)
{
	TreeTestPU root(StringCRC("Root"));

	bool result = root.AddChildProcessingUnit(MakeChildPU(StringCRC("Child1")));

	EXPECT_TRUE(result);
	EXPECT_EQ(root.GetChildren().Size(), 1u);
	EXPECT_NE(root.FindChildProcessingUnit(StringCRC("Child1")), nullptr);
}

// ---------------------------------------------------------------------------
// AC2: GetParent returns nullptr for root, non-null for child
// ---------------------------------------------------------------------------
TEST_F(PUTreeTest, GetParent_NullForRoot_NonNullForChild)
{
	TreeTestPU root(StringCRC("Root"));
	auto child = MakeChildPU(StringCRC("Child1"));
	ProcessingUnit* childPtr = child.Get();

	root.AddChildProcessingUnit(std::move(child));

	EXPECT_EQ(root.GetParent(), nullptr);
	EXPECT_EQ(childPtr->GetParent(), &root);
}

// ---------------------------------------------------------------------------
// AC3: FindChildProcessingUnit retrieves child by instance ID
// ---------------------------------------------------------------------------
TEST_F(PUTreeTest, FindChild_RetrievesByInstanceId)
{
	TreeTestPU root(StringCRC("Root"));
	auto child = MakeChildPU(StringCRC("SimPU"));
	ProcessingUnit* childPtr = child.Get();

	root.AddChildProcessingUnit(std::move(child));

	EXPECT_EQ(root.FindChildProcessingUnit(StringCRC("SimPU")), childPtr);
	EXPECT_EQ(root.FindChildProcessingUnit(StringCRC("NonExistent")), nullptr);
}

// ---------------------------------------------------------------------------
// AC4: IsRoot returns true for PUs with no parent
// ---------------------------------------------------------------------------
TEST_F(PUTreeTest, IsRoot_TrueForRootFalseForChild)
{
	TreeTestPU root(StringCRC("Root"));
	auto child = MakeChildPU(StringCRC("Child1"));
	ProcessingUnit* childPtr = child.Get();

	root.AddChildProcessingUnit(std::move(child));

	EXPECT_TRUE(root.IsRoot());
	EXPECT_FALSE(childPtr->IsRoot());
}

// ---------------------------------------------------------------------------
// AC5: RemoveChildProcessingUnit removes and destroys a child
// ---------------------------------------------------------------------------
TEST_F(PUTreeTest, RemoveChild_RemovesAndDestroys)
{
	TreeTestPU root(StringCRC("Root"));

	root.AddChildProcessingUnit(MakeTrackingPU(StringCRC("Child1")));
	EXPECT_EQ(root.GetChildren().Size(), 1u);

	bool result = root.RemoveChildProcessingUnit(StringCRC("Child1"));

	EXPECT_TRUE(result);
	EXPECT_EQ(root.GetChildren().Size(), 0u);
	EXPECT_EQ(sDestructionOrder.size(), 1u);
	EXPECT_EQ(sDestructionOrder[0], StringCRC("Child1"));
}

// ---------------------------------------------------------------------------
// AC5 (cont): Remove non-existent returns false
// ---------------------------------------------------------------------------
TEST_F(PUTreeTest, RemoveChild_NonExistent_ReturnsFalse)
{
	TreeTestPU root(StringCRC("Root"));

	bool result = root.RemoveChildProcessingUnit(StringCRC("NoSuchChild"));
	EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// AC8: Destroying parent destroys all children (recursive teardown)
// ---------------------------------------------------------------------------
TEST_F(PUTreeTest, DestroyParent_DestroysAllChildren)
{
	{
		TreeTestPU root(StringCRC("Root"));

		auto child = MakeTrackingPU(StringCRC("Child"));
		auto grandchild = MakeTrackingPU(StringCRC("Grandchild"));

		ProcessingUnit* childPtr = child.Get();
		childPtr->AddChildProcessingUnit(std::move(grandchild));
		root.AddChildProcessingUnit(std::move(child));
	}
	// Root goes out of scope — all descendants must be destroyed
	EXPECT_EQ(sDestructionOrder.size(), 2u);

	// Both child and grandchild were destroyed (order: derived dtor records
	// before base dtor recurses, so Child records before Grandchild)
	bool childDestroyed = false;
	bool grandchildDestroyed = false;
	for (const auto& id : sDestructionOrder)
	{
		if (id == StringCRC("Child")) childDestroyed = true;
		if (id == StringCRC("Grandchild")) grandchildDestroyed = true;
	}
	EXPECT_TRUE(childDestroyed);
	EXPECT_TRUE(grandchildDestroyed);
}

// ---------------------------------------------------------------------------
// AC9: Duplicate instance ID rejected
// ---------------------------------------------------------------------------
TEST_F(PUTreeTest, AddChild_DuplicateId_Rejected)
{
	TreeTestPU root(StringCRC("Root"));

	EXPECT_TRUE(root.AddChildProcessingUnit(MakeChildPU(StringCRC("SameId"))));
	EXPECT_FALSE(root.AddChildProcessingUnit(MakeChildPU(StringCRC("SameId"))));
	EXPECT_EQ(root.GetChildren().Size(), 1u);
}

// ---------------------------------------------------------------------------
// AC10: GetChildren returns all direct children
// ---------------------------------------------------------------------------
TEST_F(PUTreeTest, GetChildren_ReturnsAllDirectChildren)
{
	TreeTestPU root(StringCRC("Root"));
	root.AddChildProcessingUnit(MakeChildPU(StringCRC("A")));
	root.AddChildProcessingUnit(MakeChildPU(StringCRC("B")));
	root.AddChildProcessingUnit(MakeChildPU(StringCRC("C")));

	EXPECT_EQ(root.GetChildren().Size(), 3u);
}

// ---------------------------------------------------------------------------
// AC13: Tree traversal — FindProcessingUnitInTree recursive search
// ---------------------------------------------------------------------------
TEST_F(PUTreeTest, FindInTree_RecursiveSearch)
{
	TreeTestPU root(StringCRC("Root"));

	auto child = MakeChildPU(StringCRC("Child"));
	auto grandchild = MakeChildPU(StringCRC("Grandchild"));
	ProcessingUnit* grandchildPtr = grandchild.Get();

	ProcessingUnit* childPtr = child.Get();
	childPtr->AddChildProcessingUnit(std::move(grandchild));
	root.AddChildProcessingUnit(std::move(child));

	EXPECT_EQ(root.FindProcessingUnitInTree(StringCRC("Root")), &root);
	EXPECT_EQ(root.FindProcessingUnitInTree(StringCRC("Child")), childPtr);
	EXPECT_EQ(root.FindProcessingUnitInTree(StringCRC("Grandchild")), grandchildPtr);
	EXPECT_EQ(root.FindProcessingUnitInTree(StringCRC("NonExistent")), nullptr);
}

// ---------------------------------------------------------------------------
// Null child rejected
// ---------------------------------------------------------------------------
TEST_F(PUTreeTest, AddChild_Null_ReturnsFalse)
{
	TreeTestPU root(StringCRC("Root"));
	Dia::Core::UniquePtr<ProcessingUnit> nullChild(nullptr);
	EXPECT_FALSE(root.AddChildProcessingUnit(std::move(nullChild)));
	EXPECT_EQ(root.GetChildren().Size(), 0u);
}

// ---------------------------------------------------------------------------
// Max depth enforcement
// ---------------------------------------------------------------------------
TEST_F(PUTreeTest, AddChild_ExceedsMaxDepth_Rejected)
{
	TreeTestPU root(StringCRC("Level0"));

	ProcessingUnit* current = &root;
	// Build chain up to kMaxTreeDepth - 1 (depth 7 children under root at depth 0)
	for (unsigned int i = 1; i < 8; ++i)
	{
		char name[32];
		snprintf(name, sizeof(name), "Level%u", i);
		auto child = MakeChildPU(StringCRC(name));
		ProcessingUnit* childPtr = child.Get();
		EXPECT_TRUE(current->AddChildProcessingUnit(std::move(child)));
		current = childPtr;
	}

	// Depth 8 should be rejected
	EXPECT_FALSE(current->AddChildProcessingUnit(MakeChildPU(StringCRC("Level8"))));
}
