#pragma once

#include <stddef.h>

namespace CoW {
namespace WorldStep {

// Seam over WorldGenerationModule::HoldGeneration / ReleaseGeneration (counter-based). The owner module adapts its
// WorldGenerationModule to this; tests use a counting fake.
class IGenerationHold
{
public:
    virtual ~IGenerationHold() {}
    virtual void Hold() = 0;
    virtual void Release() = 0;
};

// RAII: one Hold on Take, exactly one Release (explicit, idempotent, or on destruction). Move-only.
class HoldGenerationToken
{
public:
    HoldGenerationToken() = default;
    explicit HoldGenerationToken(IGenerationHold* host) : mHost(host) {}
    ~HoldGenerationToken() { Release(); }

    HoldGenerationToken(const HoldGenerationToken&) = delete;
    HoldGenerationToken& operator=(const HoldGenerationToken&) = delete;
    HoldGenerationToken(HoldGenerationToken&& other) noexcept
        : mHost(other.mHost), mHeld(other.mHeld)
    {
        other.mHost = nullptr;
        other.mHeld = false;
    }
    HoldGenerationToken& operator=(HoldGenerationToken&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            mHost = other.mHost;
            mHeld = other.mHeld;
            other.mHost = nullptr;
            other.mHeld = false;
        }
        return *this;
    }

    void SetHost(IGenerationHold* host)
    {
        if (host != mHost) { Release(); mHost = host; }
    }

    // No host, or already held: no-op.
    void Take()
    {
        if (mHost == nullptr || mHeld) { return; }
        mHost->Hold();
        mHeld = true;
    }

    void Release()
    {
        if (!mHeld) { return; }
        mHeld = false;
        mHost->Release();
    }

    bool IsHeld() const { return mHeld; }

private:
    IGenerationHold* mHost = nullptr;
    bool mHeld = false;
};

}  // namespace WorldStep
}  // namespace CoW
