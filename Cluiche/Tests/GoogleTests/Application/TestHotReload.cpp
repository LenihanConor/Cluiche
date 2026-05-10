#include <gtest/gtest.h>
#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaApplicationFlow/ApplicationPhase.h>
#include <DiaApplicationFlow/ApplicationModule.h>
#include <DiaApplicationFlow/HotReloadManager.h>

using namespace Dia::Application;
using namespace Dia::Core;

// Test processing unit
class TestProcessingUnit : public ProcessingUnit
{
public:
    TestProcessingUnit() : ProcessingUnit(StringCRC("TestPU"), -1.0f, 16, 16) {}
    virtual bool FlaggedToStopUpdating() const override { return true; }
};

// Hot-reloadable module with state
class ReloadableModule : public Module
{
public:
    ReloadableModule(ProcessingUnit* pu, const char* name, int major, int minor, int patch)
        : Module(pu, StringCRC(name), RunningEnum::kUpdate)
        , mName(name)
        , mStateValue(0)
    {
        mVersion.major = major;
        mVersion.minor = minor;
        mVersion.patch = patch;
    }

    virtual StateObject::OpertionResponse DoStart(const IStartData* startData) override
    {
        mStarted = true;
        return StateObject::OpertionResponse::kImmediate;
    }

    virtual void DoUpdate() override
    {
        mStateValue++;
    }

    virtual void DoStop() override
    {
        mStarted = false;
    }

    // Hot reload support
    virtual bool CanHotReload() const override { return true; }

    virtual ModuleVersion GetVersion() const override { return mVersion; }

    virtual void* SaveState() override
    {
        int* state = new int(mStateValue);
        return state;
    }

    virtual void RestoreState(void* state) override
    {
        if (state != nullptr)
        {
            mStateValue = *static_cast<int*>(state);
            delete static_cast<int*>(state);
        }
    }

    // Test helpers
    int GetStateValue() const { return mStateValue; }
    void SetStateValue(int value) { mStateValue = value; }
    bool IsStarted() const { return mStarted; }

private:
    ModuleVersion mVersion;
    std::string mName;
    int mStateValue;
    bool mStarted = false;
};

// Module that cannot be reloaded
class NonReloadableModule : public Module
{
public:
    NonReloadableModule(ProcessingUnit* pu)
        : Module(pu, StringCRC("NonReloadable"), RunningEnum::kUpdate)
    {}

    virtual StateObject::OpertionResponse DoStart(const IStartData*) override
    {
        return StateObject::OpertionResponse::kImmediate;
    }

    virtual void DoUpdate() override {}
    virtual void DoStop() override {}

    virtual bool CanHotReload() const override { return false; }
};

// Simple phase
class TestPhase : public Phase
{
public:
    TestPhase(ProcessingUnit* pu, const StringCRC& id) : Phase(pu, id) {}
    virtual bool FlaggedToStopUpdating() const override { return true; }
};

TEST(HotReload, VersionCompatibilityCheck)
{
    Module::ModuleVersion v1_0_0(1, 0, 0);
    Module::ModuleVersion v1_0_1(1, 0, 1);  // Patch update
    Module::ModuleVersion v1_1_0(1, 1, 0);  // Minor update
    Module::ModuleVersion v2_0_0(2, 0, 0);  // Major update

    EXPECT_TRUE(v1_0_1.IsCompatibleWith(v1_0_0));  // Patch compatible
    EXPECT_TRUE(v1_1_0.IsCompatibleWith(v1_0_0));  // Minor compatible
    EXPECT_FALSE(v2_0_0.IsCompatibleWith(v1_0_0)); // Major not compatible
}

TEST(HotReload, GetHotReloadManagerLazyInit)
{
    TestProcessingUnit pu;

    HotReloadManager* mgr1 = pu.GetHotReloadManager();
    EXPECT_NE(mgr1, nullptr);

    HotReloadManager* mgr2 = pu.GetHotReloadManager();
    EXPECT_EQ(mgr1, mgr2);  // Same instance

    // Test calling ReplaceModule with null parameters
    std::cout << "Testing ReplaceModule with invalid params..." << std::endl;
    auto result = mgr1->ReplaceModule(StringCRC("NonExistent"), nullptr);
    std::cout << "Result: " << (int)result << std::endl;
    EXPECT_NE(result, HotReloadManager::ReloadResult::kSuccess);
}

TEST(HotReload, ReplaceModuleWithStateTransfer)
{
    std::cout << "Creating ProcessingUnit..." << std::endl;
    TestProcessingUnit pu;

    std::cout << "Creating modules..." << std::endl;
    ReloadableModule* oldModule = new ReloadableModule(&pu, "TestModule", 1, 0, 0);
    ReloadableModule* newModule = new ReloadableModule(&pu, "TestModule", 1, 0, 1);

    std::cout << "Adding oldModule to PU..." << std::endl;
    pu.AddModule(oldModule);

    std::cout << "Creating Phase..." << std::endl;
    TestPhase testPhase(&pu, StringCRC("TestPhase"));

    std::cout << "Adding oldModule to Phase..." << std::endl;
    testPhase.AddModule(oldModule);

    std::cout << "Adding Phase to PU..." << std::endl;
    pu.AddPhase(&testPhase);

    std::cout << "Initializing PU..." << std::endl;
    pu.Initialize();

    std::cout << "Setting initial phase..." << std::endl;
    pu.SetInitialPhase(&testPhase);

    std::cout << "Setting state value..." << std::endl;
    oldModule->SetStateValue(42);

    std::cout << "Getting HotReloadManager..." << std::endl;
    HotReloadManager* hrm = pu.GetHotReloadManager();

    std::cout << "Calling ReplaceModule..." << std::endl;
    auto result = hrm->ReplaceModule(oldModule->GetUniqueId(), newModule);

    std::cout << "Result: " << (int)result << std::endl;

    EXPECT_EQ(result, HotReloadManager::ReloadResult::kSuccess);

    EXPECT_EQ(newModule->GetStateValue(), 42);  // State transferred

    // Note: oldModule was deleted by HotReloadManager, only newModule is valid
    std::cout << "Test completed" << std::endl;
}

TEST(HotReload, ReplaceNonReloadableModuleFails)
{
    TestProcessingUnit pu;
    NonReloadableModule* oldModule = new NonReloadableModule(&pu);
    NonReloadableModule* newModule = new NonReloadableModule(&pu);

    pu.AddModule(oldModule);

    TestPhase testPhase(&pu, StringCRC("TestPhase"));
    testPhase.AddModule(oldModule);

    pu.AddPhase(&testPhase);
    pu.Initialize();
    pu.SetInitialPhase(&testPhase);  // Required to avoid hang

    auto result = pu.GetHotReloadManager()->ReplaceModule(oldModule->GetUniqueId(), newModule);

    EXPECT_EQ(result, HotReloadManager::ReloadResult::kModuleNotReloadable);

    // Cleanup
    delete newModule;
}

TEST(HotReload, IncompatibleVersionFails)
{
    TestProcessingUnit pu;
    ReloadableModule* oldModule = new ReloadableModule(&pu, "TestModule", 1, 0, 0);
    ReloadableModule* newModule = new ReloadableModule(&pu, "TestModule", 2, 0, 0);  // Major version change

    pu.AddModule(oldModule);

    TestPhase testPhase(&pu, StringCRC("TestPhase"));
    testPhase.AddModule(oldModule);

    pu.AddPhase(&testPhase);
    pu.Initialize();
    pu.SetInitialPhase(&testPhase);  // Required to avoid hang

    auto result = pu.GetHotReloadManager()->ReplaceModule(oldModule->GetUniqueId(), newModule);

    EXPECT_EQ(result, HotReloadManager::ReloadResult::kVersionIncompatible);

    // Cleanup
    delete newModule;
}

TEST(HotReload, ModuleNotFoundFails)
{
    TestProcessingUnit pu;
    ReloadableModule* oldModule = new ReloadableModule(&pu, "OldModule", 1, 0, 0);
    ReloadableModule* newModule = new ReloadableModule(&pu, "NewModule", 1, 0, 0);

    pu.AddModule(oldModule);

    TestPhase testPhase(&pu, StringCRC("TestPhase"));
    testPhase.AddModule(oldModule);

    pu.AddPhase(&testPhase);
    pu.Initialize();
    pu.SetInitialPhase(&testPhase);  // Required to avoid hang

    // Try to replace module with wrong ID
    auto result = pu.GetHotReloadManager()->ReplaceModule(StringCRC("NonExistent"), newModule);

    EXPECT_EQ(result, HotReloadManager::ReloadResult::kModuleNotFound);

    // Cleanup
    delete newModule;
}
