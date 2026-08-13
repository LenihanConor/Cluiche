#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/EconomyDrawer.h"

#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaEconomy/EconomyInstance.h>
#include <cstdio>

namespace CluicheTest {

static constexpr float kMineX      = -300.f;
static constexpr float kBaseX      =    0.f;
static constexpr float kConsumerX  =  250.f;
static constexpr float kSceneY     =   50.f;
static constexpr float kCarryCap   =   50.f;
static constexpr float kTreasuryMax = 2000.f;

EconomyDrawer::EconomyDrawer(
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
    Dia::Debug::DebugLayerManager&       mgr)
    : mGatherers(gatherers)
    , mTreasury(treasury)
    , mConsumerWallet(consumerWallet)
    , mMarketActive(marketActive)
    , mPoolChangedCount(poolChangedCount)
    , mFirstTransferDone(firstTransferDone)
    , mTreasuryAbove500(treasuryAbove500)
    , mConsumerSpent(consumerSpent)
    , mModifierActivated(modifierActivated)
    , mEventCountNonZero(eventCountNonZero)
    , mMgr(mgr)
{}

Dia::Core::StringCRC EconomyDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("Economy");
}

void EconomyDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    static const Dia::Core::StringCRC kGold("gold");

    const Dia::Core::RGBA kMineCol    (180,  90,  20, 200);
    const Dia::Core::RGBA kBaseCol    ( 60, 160,  60, 200);
    const Dia::Core::RGBA kConsumerCol(200,  40,  40, 200);
    const Dia::Core::RGBA kGoldArc    (220, 180,   0, 180);
    const Dia::Core::RGBA kBarBg      ( 40,  40,  40, 180);
    const Dia::Core::RGBA kBarFill    (220, 180,   0, 220);
    const Dia::Core::RGBA kTextCol    (255, 255, 255, 230);
    const Dia::Core::RGBA kGathCol[2] = {
        Dia::Core::RGBA(100, 180, 255, 220),
        Dia::Core::RGBA(180, 100, 255, 220)
    };

    const Dia::Maths::Vector2D minePos    (kMineX,     kSceneY);
    const Dia::Maths::Vector2D basePos    (kBaseX,     kSceneY);
    const Dia::Maths::Vector2D consumerPos(kConsumerX, kSceneY);

    // Path lines
    draw.RequestDraw(minePos, basePos,     Dia::Core::RGBA(120, 120, 120,  80));
    draw.RequestDraw(basePos, consumerPos, Dia::Core::RGBA(160,  60,  60,  80));

    // Mine marker
    draw.RequestDrawRect(minePos + Dia::Maths::Vector2D(-18.f, -18.f),
                         minePos + Dia::Maths::Vector2D( 18.f,  18.f),
                         kMineCol, Dia::Core::RGBA(180, 90, 20, 50));
    draw.RequestDrawText(minePos + Dia::Maths::Vector2D(-14.f, 26.f), "MINE", 12.f, kMineCol);

    // Base marker
    draw.RequestDrawRect(basePos + Dia::Maths::Vector2D(-18.f, -18.f),
                         basePos + Dia::Maths::Vector2D( 18.f,  18.f),
                         kBaseCol, Dia::Core::RGBA(60, 160, 60, 50));
    draw.RequestDrawText(basePos + Dia::Maths::Vector2D(-16.f, 26.f), "BASE", 12.f, kBaseCol);

    // Consumer marker
    draw.RequestDrawRect(consumerPos + Dia::Maths::Vector2D(-18.f, -18.f),
                         consumerPos + Dia::Maths::Vector2D( 18.f,  18.f),
                         kConsumerCol, Dia::Core::RGBA(200, 40, 40, 50));
    draw.RequestDrawText(consumerPos + Dia::Maths::Vector2D(-22.f, 26.f), "CONSUMER", 12.f, kConsumerCol);

    // Gatherer circles
    for (int i = 0; i < kEconomyDrawerGathererCount; ++i)
    {
        const GathererDrawData& g = mGatherers[i];
        draw.RequestDraw(g.pos, 14.f,
                         kGathCol[i],
                         Dia::Core::RGBA(kGathCol[i].R(), kGathCol[i].G(), kGathCol[i].B(), 60));

        const float frac = (kCarryCap > 0.f) ? (g.carry / kCarryCap) : 0.f;
        if (frac > 0.01f)
            draw.RequestDrawArc(g.pos, 10.f, 0.f, frac * 360.f, kGoldArc);

        char lbl[8];
        snprintf(lbl, sizeof(lbl), "G%d", i + 1);
        draw.RequestDrawText(g.pos + Dia::Maths::Vector2D(-7.f, -7.f), lbl, 11.f, kTextCol);
    }

    // Treasury gold bar
    const float treasuryVal  = mTreasury.GetValue(kGold);
    const float treasuryFrac = (kTreasuryMax > 0.f) ? (treasuryVal / kTreasuryMax) : 0.f;
    const float barW = 200.f;
    const float barH = 18.f;
    const Dia::Maths::Vector2D barMin(kBaseX - barW * 0.5f, kSceneY - 60.f);
    const Dia::Maths::Vector2D barMax(kBaseX + barW * 0.5f, kSceneY - 60.f + barH);
    draw.RequestDrawRect(barMin, barMax, kBarBg, kBarBg);
    if (treasuryFrac > 0.001f)
    {
        draw.RequestDrawRect(barMin,
                             Dia::Maths::Vector2D(barMin.X() + barW * treasuryFrac, barMax.Y()),
                             kBarFill, kBarFill);
    }
    draw.RequestDrawRect(barMin, barMax, kTextCol);

    char barLbl[32];
    snprintf(barLbl, sizeof(barLbl), "Treasury: %.0f", treasuryVal);
    draw.RequestDrawText(barMin + Dia::Maths::Vector2D(4.f, 3.f), barLbl, 11.f, kTextCol);

    // Market bonus indicator
    if (mMarketActive)
    {
        draw.RequestDrawText(Dia::Maths::Vector2D(kMineX, kSceneY - 80.f),
                             "MARKET BONUS x2!", 14.f,
                             Dia::Core::RGBA(255, 220, 0, 255));
    }
}

} // namespace CluicheTest

#endif // DIA_DEBUG
