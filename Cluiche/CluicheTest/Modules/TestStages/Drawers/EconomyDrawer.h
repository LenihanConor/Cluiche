#pragma once
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaEconomy/EconomyInstance.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace Debug { class DebugLayerManager; } }

namespace CluicheTest {

// -----------------------------------------------------------------------
// EconomyDrawer
//
// Draws the gatherer/treasury scene for EconomyTestStageModule.
// Registered with VisualDebuggerModule's DebugLayerManager from SimPU.
// -----------------------------------------------------------------------

// Forward declaration of types the drawer needs by reference
struct GathererDrawData
{
    Dia::Maths::Vector2D pos;
    float                carry = 0.f;  // current carry gold value
    int                  state = 0;    // 0=toMine 1=mining 2=toBase 3=depositing
};

static constexpr int kEconomyDrawerGathererCount = 2;

class EconomyDrawer : public Dia::Debug::IVisualDebugger
{
public:
    EconomyDrawer(
        const GathererDrawData      gatherers[kEconomyDrawerGathererCount],
        const Dia::Economy::EconomyInstance& treasury,
        const Dia::Economy::EconomyInstance& consumerWallet,
        const bool&                          marketActive,
        const unsigned int&                  poolChangedCount,
        const bool&                          firstTransferDone,
        const bool&                          treasuryAbove500,
        const bool&                          consumerSpent,
        const bool&                          modifierActivated,
        const bool&                          eventCountNonZero,
        Dia::Debug::DebugLayerManager&       mgr);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const GathererDrawData*              mGatherers;
    const Dia::Economy::EconomyInstance& mTreasury;
    const Dia::Economy::EconomyInstance& mConsumerWallet;
    const bool&                          mMarketActive;
    const unsigned int&                  mPoolChangedCount;
    const bool&                          mFirstTransferDone;
    const bool&                          mTreasuryAbove500;
    const bool&                          mConsumerSpent;
    const bool&                          mModifierActivated;
    const bool&                          mEventCountNonZero;
    Dia::Debug::DebugLayerManager&       mMgr;
};

} // namespace CluicheTest

#endif // DIA_DEBUG
