#include <gtest/gtest.h>
#include <DiaCore/Architecture/Components/Concrete/StaticPooledComponentFactory.h>
#include <DiaCore/Architecture/Components/Concrete/StaticSizedComponentObject.h>

using namespace Dia::Core;

// Test component definition
class DonkeyFeetComponent : public IComponent
{
public:
    COMPONENT_DECLARATION(0xDEADBEEF);
};

CREATE_STATIC_SIZED_COMPONENT_FACTORY(DonkeyFeetComponentFactory, DonkeyFeetComponent, 4);

// Test component object
class Donkey : public StaticSizedComponentObject<2>
{
public:
};

TEST(Component, AddAndRemoveComponent_WorksCorrectly)
{
    DonkeyFeetComponentFactory donkeyFeetFactory;

    IComponent* donkeyFeetA = nullptr;
    DonkeyFeetComponent* donkeyFeetB = nullptr;

    Donkey myDonkey;

    myDonkey.AddComponentFromFactory(&donkeyFeetFactory, nullptr);

    donkeyFeetA = myDonkey.GetComponentPtr(DonkeyFeetComponent::ID);
    donkeyFeetB = myDonkey.GetCastComponentPtr<DonkeyFeetComponent>();

    EXPECT_TRUE(myDonkey.HasComponent(DonkeyFeetComponent::ID));
    EXPECT_NE(donkeyFeetA, nullptr);
    EXPECT_NE(donkeyFeetB, nullptr);

    myDonkey.RemoveComponentFromFactory(&donkeyFeetFactory);

    donkeyFeetA = myDonkey.GetComponentPtr(DonkeyFeetComponent::ID);
    donkeyFeetB = myDonkey.GetCastComponentPtr<DonkeyFeetComponent>();

    EXPECT_FALSE(myDonkey.HasComponent(DonkeyFeetComponent::ID));
    EXPECT_EQ(donkeyFeetA, nullptr);
    EXPECT_EQ(donkeyFeetB, nullptr);
}

TEST(Component, FactoryOutOfScope_RemovesComponent)
{
    IComponent* donkeyFeetA = nullptr;
    DonkeyFeetComponent* donkeyFeetB = nullptr;

    Donkey myDonkey;

    // Test the factory going out of scope
    {
        DonkeyFeetComponentFactory donkeyFeetFactory;

        myDonkey.AddComponentFromFactory(&donkeyFeetFactory, nullptr);

        EXPECT_TRUE(myDonkey.HasComponent(DonkeyFeetComponent::ID));
    }

    EXPECT_FALSE(myDonkey.HasComponent(DonkeyFeetComponent::ID));
}

TEST(Component, ObjectOutOfScope_ReturnsComponentToPool)
{
    IComponent* donkeyFeetA = nullptr;
    DonkeyFeetComponent* donkeyFeetB = nullptr;

    DonkeyFeetComponentFactory donkeyFeetFactory;

    // Test the object going out of scope
    {
        Donkey myDonkey;
        myDonkey.AddComponentFromFactory(&donkeyFeetFactory, nullptr);
        EXPECT_EQ(donkeyFeetFactory.GetNumberOfActiveComponents(), 1);
    }

    EXPECT_EQ(donkeyFeetFactory.GetNumberOfActiveComponents(), 0);
}
