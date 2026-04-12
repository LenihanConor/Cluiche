// TestSingleton.cpp - Google Test unit tests for Singleton
//
// Tests Singleton pattern from DiaCore Architecture

#include <gtest/gtest.h>
#include <DiaCore/Architecture/Singleton/Singleton.h>

using namespace Dia::Core;

// ==============================================================================
// Test Singleton Class
// ==============================================================================

class MySingleton : public Singleton<MySingleton>
{
public:
    int value = 0;
};

// ==============================================================================
// Singleton Test Fixture (ensures clean state between tests)
// ==============================================================================

class SingletonTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        // Clean up singleton if it exists
        if (MySingleton::IsCreated())
        {
            MySingleton::Destroy();
        }
    }
};

// ==============================================================================
// Singleton Creation Tests
// ==============================================================================

TEST_F(SingletonTest, IsCreated_BeforeCreate_ReturnsFalse)
{
    EXPECT_FALSE(MySingleton::IsCreated());
}

TEST_F(SingletonTest, GetInstanceConst_BeforeCreate_Asserts)
{
    EXPECT_DEATH(MySingleton::GetInstanceConst(), ".*");
}

TEST_F(SingletonTest, Create_CreatesInstance)
{
    MySingleton::Create();

    EXPECT_TRUE(MySingleton::IsCreated());
}

TEST_F(SingletonTest, Create_CalledTwice_Asserts)
{
    MySingleton::Create();

    // Creating again should assert
    EXPECT_DEATH(MySingleton::Create(), ".*");
}

// ==============================================================================
// Singleton Destruction Tests
// ==============================================================================

TEST_F(SingletonTest, Destroy_DestroysInstance)
{
    MySingleton::Create();
    MySingleton::Destroy();

    EXPECT_FALSE(MySingleton::IsCreated());
}

TEST_F(SingletonTest, GetInstanceConst_AfterDestroy_Asserts)
{
    MySingleton::Create();
    MySingleton::Destroy();

    EXPECT_DEATH(MySingleton::GetInstanceConst(), ".*");
}

TEST_F(SingletonTest, Destroy_WithoutCreate_Asserts)
{
    EXPECT_DEATH(MySingleton::Destroy(), ".*");
}

// ==============================================================================
// Singleton Access Tests
// ==============================================================================

TEST_F(SingletonTest, GetInstance_AfterCreate_ReturnsValidReference)
{
    MySingleton::Create();

    MySingleton& instance = MySingleton::GetInstance();
    instance.value = 42;

    EXPECT_EQ(instance.value, 42);
}

TEST_F(SingletonTest, GetInstanceConst_AfterCreate_ReturnsValidReference)
{
    MySingleton::Create();
    MySingleton::GetInstance().value = 100;

    const MySingleton& instance = MySingleton::GetInstanceConst();

    EXPECT_EQ(instance.value, 100);
}

TEST_F(SingletonTest, GetInstance_ReturnsSameInstance)
{
    MySingleton::Create();

    MySingleton& instance1 = MySingleton::GetInstance();
    MySingleton& instance2 = MySingleton::GetInstance();

    instance1.value = 50;

    EXPECT_EQ(&instance1, &instance2);
    EXPECT_EQ(instance2.value, 50);
}

// ==============================================================================
// Singleton Lifecycle Tests
// ==============================================================================

TEST_F(SingletonTest, CreateDestroyCreate_WorksCorrectly)
{
    // First lifecycle
    MySingleton::Create();
    EXPECT_TRUE(MySingleton::IsCreated());
    MySingleton::Destroy();
    EXPECT_FALSE(MySingleton::IsCreated());

    // Second lifecycle
    MySingleton::Create();
    EXPECT_TRUE(MySingleton::IsCreated());
    MySingleton::Destroy();
    EXPECT_FALSE(MySingleton::IsCreated());
}

TEST_F(SingletonTest, CreateDestroyCreate_StateIsReset)
{
    MySingleton::Create();
    MySingleton::GetInstance().value = 123;
    MySingleton::Destroy();

    MySingleton::Create();
    MySingleton& newInstance = MySingleton::GetInstance();

    // New instance should have default state
    EXPECT_EQ(newInstance.value, 0);
}

// ==============================================================================
// Multiple Singleton Types Test
// ==============================================================================

class AnotherSingleton : public Singleton<AnotherSingleton>
{
public:
    float data = 0.0f;
};

TEST(SingletonMultipleTypes, DifferentSingletons_IndependentLifecycles)
{
    MySingleton::Create();
    AnotherSingleton::Create();

    EXPECT_TRUE(MySingleton::IsCreated());
    EXPECT_TRUE(AnotherSingleton::IsCreated());

    MySingleton::GetInstance().value = 10;
    AnotherSingleton::GetInstance().data = 3.14f;

    EXPECT_EQ(MySingleton::GetInstance().value, 10);
    EXPECT_FLOAT_EQ(AnotherSingleton::GetInstance().data, 3.14f);

    MySingleton::Destroy();

    EXPECT_FALSE(MySingleton::IsCreated());
    EXPECT_TRUE(AnotherSingleton::IsCreated());

    AnotherSingleton::Destroy();

    EXPECT_FALSE(AnotherSingleton::IsCreated());
}
