// AC 40: this translation unit includes ONLY HydrologyInputsProvider.h (before anything else) and must compile.
#include <Shared/Weather/HydrologyInputsProvider.h>

#include <gtest/gtest.h>

namespace
{
class FakeProvider : public CoW::Weather::IHydrologyInputsProvider
{
public:
    bool GetInputs(Draoi::HexWorld::Weather::HydrologyInputs& out) override
    {
        out.present = true;
        return true;
    }
};
}

TEST(HydrologyInputsProviderHeader, StandaloneHeaderCompilesAndIsImplementable)
{
    FakeProvider provider;
    CoW::Weather::IHydrologyInputsProvider& iface = provider;
    Draoi::HexWorld::Weather::HydrologyInputs inputs;
    EXPECT_FALSE(inputs.present);
    EXPECT_TRUE(iface.GetInputs(inputs));
    EXPECT_TRUE(inputs.present);
}
