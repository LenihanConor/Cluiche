#include <gtest/gtest.h>
#include <cstddef>
#include <cstring>
#include <new>
#include <DiaEntity/Testing/TestDescribedComponent.h>
#include <DiaEntity/ComponentTypeDesc.h>

using namespace Dia::Entity;
using DiaEntityTesting::TestDescribedComponent;
using DiaEntityTesting::TestDescribedNested;

// Nested serialize must live in the type's namespace so ADL finds it from the archive.
namespace DiaEntityTesting {
DIA_SERIALIZE(TestDescribedNested, 1)
    DIA_FIELD(gain)
    DIA_FIELD(count)
DIA_SERIALIZE_END

DIA_SERIALIZE(TestDescribedComponent, TestDescribedComponent::kVersion)
    DIA_FIELD(speed)
    DIA_FIELD(hitPoints)
    DIA_FIELD(active)
    DIA_FIELD(tag)
    DIA_FIELD(nested)
DIA_SERIALIZE_END

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

static Dia::Entity::FieldDesc s_TestDescribedComponent_fields[] = {
    DIA_FIELD_ENTRY(float,   speed,     TestDescribedComponent)
    DIA_FIELD_ENTRY(int32_t, hitPoints, TestDescribedComponent)
    DIA_FIELD_ENTRY(bool,    active,    TestDescribedComponent)
    DIA_FIELD_ENTRY(Dia::Core::StringCRC, tag, TestDescribedComponent)
    DIA_FIELD_ENTRY(TestDescribedNested,  nested, TestDescribedComponent)
};

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

DIA_COMPONENT_REGISTER(TestDescribedComponent, "test-described-component", false, false,
    s_TestDescribedComponent_fields, DIA_ARRAY_COUNT(s_TestDescribedComponent_fields),
    nullptr, 0,
    nullptr, 0)

} // namespace DiaEntityTesting

// ============================================================
// ConstructDefault
// ============================================================

TEST(DiaEntityConstructDefault, MacroRegisteredComponentHasConstructDefault) {
    EXPECT_NE(TestDescribedComponent::GetDesc().constructDefault, nullptr);
}

TEST(DiaEntityConstructDefault, ConstructedInstanceCarriesFieldDefaultsNotZero) {
    const ComponentTypeDesc& desc = TestDescribedComponent::GetDesc();
    ASSERT_NE(desc.constructDefault, nullptr);

    alignas(std::max_align_t) unsigned char buffer[256];
    ASSERT_LE(desc.size, sizeof(buffer));
    std::memset(buffer, 0, sizeof(buffer));
    desc.constructDefault(buffer);
    IComponent* comp = reinterpret_cast<IComponent*>(buffer);

    Json::Value out;
    desc.saveToJson(comp, out);
    comp->~IComponent();

    EXPECT_FLOAT_EQ(out["speed"].asFloat(), 3.5f);
    EXPECT_EQ(out["hitPoints"].asInt(), 7);
    EXPECT_TRUE(out["active"].asBool());
    EXPECT_FLOAT_EQ(out["nested"]["gain"].asFloat(), 2.0f);
    EXPECT_EQ(out["nested"]["count"].asInt(), 4);
    EXPECT_TRUE(out.isMember("tag"));
}

TEST(DiaEntityConstructDefault, HandBuiltDescWithoutConstructDefaultReportsNone) {
    ComponentTypeDesc desc {
        Dia::Core::StringCRC("hand-built"), "HandBuilt", 0u, 0u, 1u, 0u,
        nullptr, 0u, nullptr, 0u, nullptr, 0u, nullptr, nullptr
    };
    EXPECT_EQ(desc.constructDefault, nullptr);
    EXPECT_EQ(desc.description, nullptr);
}
