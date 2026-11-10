#include <gtest/gtest.h>
#include <DiaValueSchema/ComponentAttributeAudit.h>
#include <DiaValueSchema/ValueSchemaHealth.h>
#include <DiaEntity/Testing/TestDescribedComponent.h>
#include <DiaEntity/ComponentTypeDesc.h>
#include <DiaCore/Reflect/FieldAttributes.h>
#include <memory>

// Feature 7 tasks 5+7: ComponentAttributeAudit (dropped registrations, empty descriptions, unknown-field describes).

using namespace Dia::ValueSchema;
using Dia::Reflect::DescribeAttribute;
using Dia::Reflect::FieldAttributeRegistry;
using Dia::Reflect::RequiredAttribute;
using DiaEntityTesting::TestDescribedComponent;

namespace
{
	uint32_t Crc(const char* s) { return Dia::Core::StringCRC(s).Value(); }

	// Audits run against a private copy of the singleton so no other test is polluted.
	struct ComponentAttributeAuditTest : ::testing::Test
	{
		void SetUp() override
		{
			mRegistry.reset(new FieldAttributeRegistry(FieldAttributeRegistry::Instance()));
			mDescs[0] = &TestDescribedComponent::GetDesc();
		}
		std::unique_ptr<FieldAttributeRegistry>		mRegistry;
		const Dia::Entity::ComponentTypeDesc*		mDescs[1] = {};
	};

	DescribeAttribute MakeDescribe(const char* label, const char* description)
	{
		DescribeAttribute d;
		d.label = label;
		d.description = description;
		return d;
	}
}

TEST_F(ComponentAttributeAuditTest, CleanFixtureReportsNothing)
{
	ComponentAttributeAudit audit;
	const ComponentAttributeAuditReport r = audit.Run(*mRegistry, mDescs, 1);
	EXPECT_EQ(r.mEmptyDescriptions, 0u);
	EXPECT_EQ(r.mUnknownFieldDescribes, 0u);
	EXPECT_EQ(r.mDroppedCount, 0u);
	EXPECT_EQ(r.mEntriesUsed, mRegistry->EntryCount());
	EXPECT_EQ(r.mNewWarnings, 0u);
}

TEST_F(ComponentAttributeAuditTest, EmptyDescriptionIsCountedAndWarnedOnce)
{
	static DescribeAttribute empty = MakeDescribe("Armor", "");
	mRegistry->Register(Crc("TestDescribedComponent"), Crc("armor"), &empty);

	ComponentAttributeAudit audit;
	ComponentAttributeAuditReport r = audit.Run(*mRegistry, mDescs, 1);
	EXPECT_EQ(r.mEmptyDescriptions, 1u);
	EXPECT_EQ(r.mNewWarnings, 1u);

	r = audit.Run(*mRegistry, mDescs, 1);	// repeated run: counted again, not re-warned
	EXPECT_EQ(r.mEmptyDescriptions, 1u);
	EXPECT_EQ(r.mNewWarnings, 0u);
}

TEST_F(ComponentAttributeAuditTest, DescribeForNonexistentFieldIsCountedAndWarnedOncePerPair)
{
	static DescribeAttribute d = MakeDescribe("Ghost", "No such field");
	mRegistry->Register(Crc("TestDescribedComponent"), Crc("noSuchField"), &d);
	mRegistry->Register(Crc("TestDescribedComponent"), Crc("anotherGhost"), &d);

	ComponentAttributeAudit audit;
	ComponentAttributeAuditReport r = audit.Run(*mRegistry, mDescs, 1);
	EXPECT_EQ(r.mUnknownFieldDescribes, 2u);
	EXPECT_EQ(r.mNewWarnings, 2u);

	r = audit.Run(*mRegistry, mDescs, 1);
	EXPECT_EQ(r.mUnknownFieldDescribes, 2u);
	EXPECT_EQ(r.mNewWarnings, 0u);
}

TEST_F(ComponentAttributeAuditTest, DescribeForUnrelatedClassIsIgnored)
{
	static DescribeAttribute d = MakeDescribe("X", "Some other class");
	mRegistry->Register(Crc("NotAComponent"), Crc("x"), &d);
	ComponentAttributeAudit audit;
	const ComponentAttributeAuditReport r = audit.Run(*mRegistry, mDescs, 1);
	EXPECT_EQ(r.mUnknownFieldDescribes, 0u);
}

TEST_F(ComponentAttributeAuditTest, ResetForgetsSeenPairs)
{
	static DescribeAttribute d = MakeDescribe("Ghost", "No such field");
	mRegistry->Register(Crc("TestDescribedComponent"), Crc("noSuchField"), &d);
	ComponentAttributeAudit audit;
	audit.Run(*mRegistry, mDescs, 1);
	audit.Reset();
	EXPECT_EQ(audit.Run(*mRegistry, mDescs, 1).mNewWarnings, 1u);
}

TEST_F(ComponentAttributeAuditTest, DroppedCountIsReported)
{
	static RequiredAttribute attr;
	mRegistry->Clear();
	for (unsigned int i = 0; i <= Dia::Reflect::FieldAttributeList::kMaxAttrsPerField; ++i) { mRegistry->Register(7u, 7u, &attr); }
	ComponentAttributeAudit audit;
	const ComponentAttributeAuditReport r = audit.Run(*mRegistry, mDescs, 1);
	EXPECT_EQ(r.mDroppedCount, 1u);
	EXPECT_EQ(r.mEntriesUsed, 1u);
}

TEST_F(ComponentAttributeAuditTest, SeenListOverflowIsCounted)
{
	static DescribeAttribute d = MakeDescribe("Ghost", "No such field");
	const unsigned int n = ComponentAttributeAudit::kMaxSeen + 5u;
	for (unsigned int i = 0; i < n; ++i) { mRegistry->Register(Crc("TestDescribedComponent"), 0xC0DE0000u + i, &d); }
	ComponentAttributeAudit audit;
	const ComponentAttributeAuditReport r = audit.Run(*mRegistry, mDescs, 1);
	EXPECT_EQ(r.mUnknownFieldDescribes, n);
	EXPECT_EQ(r.mSeenOverflow, 5u);
}

TEST(ValueSchemaHealthAttrDropped, DegradedOnlyWhenRegistrationsDropped)
{
	SchemaLoadStats stats;
	ComponentAttributeAuditReport rep;
	ValueSchemaHealth health(stats, nullptr, &rep);
	EXPECT_EQ(health.Report().status, Dia::Observation::Health::HealthStatus::kOK);
	rep.mDroppedCount = 3;
	const auto h = health.Report();
	EXPECT_EQ(h.status, Dia::Observation::Health::HealthStatus::kDegraded);
	EXPECT_EQ(h.reason, Dia::Core::StringCRC("valueschema.attr_registrations_dropped"));
}
