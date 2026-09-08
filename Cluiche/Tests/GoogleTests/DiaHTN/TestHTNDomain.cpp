#include <gtest/gtest.h>
#include <DiaHTN/HTNDomain.h>
#include <DiaHTN/Testing/HTNTestHelpers.h>
#include <DiaCore/Json/external/json/json.h>

// DiaHTN_Domain
// Covers LoadFromJson, IsValid, IsCompound/IsPrimitive, and Validate.

namespace
{
    // Minimal domain: one compound root with one method, three primitives.
    constexpr const char* kSimpleDomainJson = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [
                    {
                        "id": "Root_Always",
                        "subtasks": ["StepA", "StepB"]
                    }
                ]
            },
            "StepA": { "type": "primitive", "operator": "OpA", "params": [] },
            "StepB": { "type": "primitive", "operator": "OpB", "params": ["target"] }
        }
    })";

    Json::Value ParseJson(const char* jsonStr)
    {
        Json::Value root;
        Json::Reader reader;
        reader.parse(jsonStr, root);
        return root;
    }
}

TEST(DiaHTN_Domain, LoadFromJson_ValidDomain_IsValid)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);
    EXPECT_TRUE(domain.IsValid());
    EXPECT_EQ(errors.Size(), 0u);
}

TEST(DiaHTN_Domain, LoadFromJson_EmptyJson_IsInvalid)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(Json::Value{}, errors);
    EXPECT_FALSE(domain.IsValid());
    EXPECT_GT(errors.Size(), 0u);
}

TEST(DiaHTN_Domain, IsCompound_KnownCompound_ReturnsTrue)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);
    EXPECT_TRUE(domain.IsCompound(Dia::Core::StringCRC("Root")));
}

TEST(DiaHTN_Domain, IsCompound_PrimitiveTask_ReturnsFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);
    EXPECT_FALSE(domain.IsCompound(Dia::Core::StringCRC("StepA")));
}

TEST(DiaHTN_Domain, IsPrimitive_KnownPrimitive_ReturnsTrue)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);
    EXPECT_TRUE(domain.IsPrimitive(Dia::Core::StringCRC("StepA")));
    EXPECT_TRUE(domain.IsPrimitive(Dia::Core::StringCRC("StepB")));
}

TEST(DiaHTN_Domain, IsPrimitive_CompoundTask_ReturnsFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);
    EXPECT_FALSE(domain.IsPrimitive(Dia::Core::StringCRC("Root")));
}

TEST(DiaHTN_Domain, IsCompound_UnknownTask_ReturnsFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);
    EXPECT_FALSE(domain.IsCompound(Dia::Core::StringCRC("DoesNotExist")));
}

TEST(DiaHTN_Domain, Validate_ValidDomain_ReturnsTrue)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> loadErrors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), loadErrors);

    Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;
    EXPECT_TRUE(domain.Validate(validateErrors));
    EXPECT_EQ(validateErrors.Size(), 0u);
}

TEST(DiaHTN_Domain, Validate_DanglingSubTask_ReturnsFalse)
{
    constexpr const char* kBadJson = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{ "id": "m", "subtasks": ["MissingTask"] }]
            }
        }
    })";

    Dia::Core::Containers::DynamicArrayC<const char*, 32> loadErrors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kBadJson), loadErrors);

    Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;
    EXPECT_FALSE(domain.Validate(validateErrors));
    EXPECT_GT(validateErrors.Size(), 0u);
}

TEST(DiaHTN_Domain, Validate_CycleDetected_ReturnsFalse)
{
    constexpr const char* kCycleJson = R"({
        "tasks": {
            "A": {
                "type": "compound",
                "methods": [{ "id": "m", "subtasks": ["B"] }]
            },
            "B": {
                "type": "compound",
                "methods": [{ "id": "m2", "subtasks": ["A"] }]
            }
        }
    })";

    Dia::Core::Containers::DynamicArrayC<const char*, 32> loadErrors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kCycleJson), loadErrors);

    Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;
    EXPECT_FALSE(domain.Validate(validateErrors));
    EXPECT_GT(validateErrors.Size(), 0u);
}

TEST(DiaHTN_Domain, MoveConstruct_TransfersValidity)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domainA = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);
    ASSERT_TRUE(domainA.IsValid());

    auto domainB = std::move(domainA);
    EXPECT_TRUE(domainB.IsValid());
    EXPECT_FALSE(domainA.IsValid());
}

TEST(DiaHTN_Domain, PrimitiveWithParams_ParsesCorrectly)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);
    EXPECT_TRUE(domain.IsPrimitive(Dia::Core::StringCRC("StepB")));
}

// --- DefaultConstruct / move-assign ---

TEST(DiaHTN_Domain, DefaultConstruct_IsInvalid)
{
    Dia::HTN::HTNDomain domain;
    EXPECT_FALSE(domain.IsValid());
}

TEST(DiaHTN_Domain, MoveAssign_TransfersValidity)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domainA = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);
    ASSERT_TRUE(domainA.IsValid());

    Dia::HTN::HTNDomain domainB;
    domainB = std::move(domainA);

    EXPECT_TRUE(domainB.IsValid());
    EXPECT_FALSE(domainA.IsValid());
}

// --- GetMethodCount ---

TEST(DiaHTN_Domain, GetMethodCount_Compound_ReturnsOne)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);
    EXPECT_EQ(domain.GetMethodCount(Dia::Core::StringCRC("Root")), 1);
}

TEST(DiaHTN_Domain, GetMethodCount_Primitive_ReturnsZero)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);
    EXPECT_EQ(domain.GetMethodCount(Dia::Core::StringCRC("StepA")), 0);
}

TEST(DiaHTN_Domain, GetMethodCount_UnknownTask_ReturnsZero)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);
    EXPECT_EQ(domain.GetMethodCount(Dia::Core::StringCRC("NoSuchTask")), 0);
}

// --- EvalMethodPrecondition ---

TEST(DiaHTN_Domain, EvalMethodPrecondition_NoPrecondition_AlwaysTrue)
{
    // Root_Always has no precondition node — should always pass.
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);
    Dia::HTN::Testing::MockHTNContext ctx; // empty context
    EXPECT_TRUE(domain.EvalMethodPrecondition(Dia::Core::StringCRC("Root"), 0, ctx));
}

TEST(DiaHTN_Domain, EvalMethodPrecondition_OutOfRangeIndex_ReturnsFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);
    Dia::HTN::Testing::MockHTNContext ctx;
    // Method index 5 doesn't exist (only 0).
    EXPECT_FALSE(domain.EvalMethodPrecondition(Dia::Core::StringCRC("Root"), 5, ctx));
}

TEST(DiaHTN_Domain, EvalMethodPrecondition_NegativeIndex_ReturnsFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);
    Dia::HTN::Testing::MockHTNContext ctx;
    EXPECT_FALSE(domain.EvalMethodPrecondition(Dia::Core::StringCRC("Root"), -1, ctx));
}

// --- GetMethodSubtasks ---

TEST(DiaHTN_Domain, GetMethodSubtasks_ValidIndex_ReturnsSubtasks)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> subtasks;
    const bool ok = domain.GetMethodSubtasks(Dia::Core::StringCRC("Root"), 0, subtasks);

    EXPECT_TRUE(ok);
    ASSERT_EQ(subtasks.Size(), 2u);
    EXPECT_EQ(subtasks[0].Value(), Dia::Core::StringCRC("StepA").Value());
    EXPECT_EQ(subtasks[1].Value(), Dia::Core::StringCRC("StepB").Value());
}

TEST(DiaHTN_Domain, GetMethodSubtasks_OutOfRangeIndex_ReturnsFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> subtasks;
    EXPECT_FALSE(domain.GetMethodSubtasks(Dia::Core::StringCRC("Root"), 99, subtasks));
    EXPECT_EQ(subtasks.Size(), 0u);
}

// --- GetPrimitiveInfo ---

TEST(DiaHTN_Domain, GetPrimitiveInfo_KnownPrimitive_ValidInfo)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);

    const auto info = domain.GetPrimitiveInfo(Dia::Core::StringCRC("StepA"));
    EXPECT_TRUE(info.valid);
    EXPECT_EQ(info.operatorId.Value(), Dia::Core::StringCRC("OpA").Value());
}

TEST(DiaHTN_Domain, GetPrimitiveInfo_UnknownTask_InvalidInfo)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);

    const auto info = domain.GetPrimitiveInfo(Dia::Core::StringCRC("Nonexistent"));
    EXPECT_FALSE(info.valid);
}

TEST(DiaHTN_Domain, GetPrimitiveInfo_PrimitiveWithParams_IncludesParams)
{
    // StepB has params: ["target"]
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kSimpleDomainJson), errors);

    const auto info = domain.GetPrimitiveInfo(Dia::Core::StringCRC("StepB"));
    ASSERT_TRUE(info.valid);
    ASSERT_EQ(info.params.Size(), 1u);
    EXPECT_EQ(info.params[0].Value(), Dia::Core::StringCRC("target").Value());
}

// --- Malformed JSON ---

TEST(DiaHTN_Domain, LoadFromJson_MissingTasksKey_IsInvalid)
{
    constexpr const char* kNoTasks = R"({ "other": {} })";
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto domain = Dia::HTN::HTNDomain::LoadFromJson(ParseJson(kNoTasks), errors);
    EXPECT_FALSE(domain.IsValid());
    EXPECT_GT(errors.Size(), 0u);
}
