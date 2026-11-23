#pragma once

#include <stdint.h>

#include <DiaDeferredStep/IStepStage.h>

namespace CoW {
namespace WorldStep {
namespace Testing {

// Fixed-capacity log of call names (string literals; no STL).
struct CallLog
{
    static const uint32_t kCapacity = 64;

    void Add(const char* name)
    {
        if (mCount < kCapacity) { mNames[mCount++] = name; }
    }
    uint32_t Count() const { return mCount; }
    const char* At(uint32_t i) const { return i < mCount ? mNames[i] : ""; }
    void Clear() { mCount = 0; }

private:
    const char* mNames[kCapacity] = {};
    uint32_t mCount = 0;
};

// Plain structs for the later WorldStepStageBase fixture.
struct FakeSim   { int value = 0; };
struct FakeEntry { int value = 0; };
struct FakeLatch { int value = 0; };

// Records every IStepStage call by name. Seed for the WorldStepStageBase test fixture.
class FakeWorldStage : public Dia::DeferredStep::IStepStage
{
public:
    bool IsReady() const override
    {
        log.Add("IsReady");
        return readyValue && extraReady && !initFails;
    }
    void Capture(Dia::DeferredStep::StepKey) override { log.Add("Capture"); }
    void Run(Dia::DeferredStep::IParallelExecutor&) override { log.Add("Run"); }
    void Commit() override { log.Add("Commit"); }
    void SerializePending(Dia::SaveGame::SaveContext&) const override { log.Add("SerializePending"); }
    void DeserializePending(Dia::SaveGame::LoadContext&) override { log.Add("DeserializePending"); }
    bool ValidatePending(Dia::SaveGame::LoadContext&) const override
    {
        log.Add("ValidatePending");
        return latchOk;
    }

    mutable CallLog log;
    bool extraReady = true;
    bool latchOk = true;
    bool initFails = false;
    bool readyValue = true;
};

}  // namespace Testing
}  // namespace WorldStep
}  // namespace CoW
