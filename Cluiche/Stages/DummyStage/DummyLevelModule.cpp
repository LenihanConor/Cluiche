#include "DummyLevelModule.h"
#include "Modules/KernelModule.h"
#include "Modules/AssetServiceModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaGraphics/Frame/SpriteDrawCommand.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaSFML/TextureHandler.h>
#include <DiaInput/EKey.h>
#include <DiaLogger/DiaLog.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <cmath>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC DummyLevelModule::kTypeId("DummyLevelModule");

// Screen bounds
static constexpr float kScreenWidth  = 800.0f;
static constexpr float kScreenHeight = 600.0f;
static constexpr float kMoveSpeed    = 100.0f; // pixels per second

DummyLevelModule::DummyLevelModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{
}

Dia::ApplicationFlow::StartResult DummyLevelModule::DoStart()
{
    AssetServiceModule* assets = AssetServiceModule::GetStatic();

    if (!mLoadEntryLogged)
    {
        DIA_LOG_INFO("Application", "DummyLevelModule DoStart entry — polling for stage load");
        mLoadEntryLogged = true;
    }

    if (!assets)
        return Dia::ApplicationFlow::StartResult::kLoading;

    using LoadState = AssetServiceModule::StageLoadState;
    LoadState state = assets->GetStageLoadState(Dia::Core::StringCRC("DummyStage"));

    if (state == LoadState::kFailed)
    {
        DIA_LOG_ERROR("Application", "DummyLevelModule DoStart — stage load failed");
        return Dia::ApplicationFlow::StartResult::kFailed;
    }
    if (state == LoadState::kComplete)
    {
        DIA_LOG_INFO("Application", "DummyLevelModule DoStart ready");
        return Dia::ApplicationFlow::StartResult::kReady;
    }
    return Dia::ApplicationFlow::StartResult::kLoading;
}

void DummyLevelModule::DoUpdate(float dt)
{
    // --- Input ---
    if (InputStreamModule* input = mInput.Get())
    {
        if (input->IsKeyDown(Dia::Input::EKey::Left) ||
            input->IsKeyDown(Dia::Input::EKey::A))
        {
            mSpriteX -= kMoveSpeed * dt;
        }
        if (input->IsKeyDown(Dia::Input::EKey::Right) ||
            input->IsKeyDown(Dia::Input::EKey::D))
        {
            mSpriteX += kMoveSpeed * dt;
        }
        if (input->IsKeyDown(Dia::Input::EKey::Up) ||
            input->IsKeyDown(Dia::Input::EKey::W))
        {
            mSpriteY -= kMoveSpeed * dt;
        }
        if (input->IsKeyDown(Dia::Input::EKey::Down) ||
            input->IsKeyDown(Dia::Input::EKey::S))
        {
            mSpriteY += kMoveSpeed * dt;
        }

        // Clamp to screen bounds
        if (mSpriteX < 0.0f)          mSpriteX = 0.0f;
        if (mSpriteX > kScreenWidth)  mSpriteX = kScreenWidth;
        if (mSpriteY < 0.0f)          mSpriteY = 0.0f;
        if (mSpriteY > kScreenHeight) mSpriteY = kScreenHeight;
    }

    // --- Build frame ---
    mFrame.Clear();

    // Composite the latest UI buffer so stage HUD/page is visible.
    if (const Dia::UI::UIDataBuffer* uiBuffer = mUIInput.FetchLatest())
    {
        mFrame.RequestDrawUI(*uiBuffer);
    }

    // Debug-draw: static white circle + animated red circle + line between
    // them. Mirrors v1 SimProcessingUnit's "sim pipeline is alive" marker.
    {
        static float s_elapsed = 0.0f;
        s_elapsed += dt;
        const float kRadius = 60.0f;
        Dia::Maths::Vector2D center(100.0f, 100.0f);
        Dia::Maths::Vector2D dynamic(
            center.x + std::cos(s_elapsed) * kRadius,
            center.y + std::sin(s_elapsed) * kRadius);

        mFrame.RequestDraw(center,  75.0f, Dia::Graphics::RGBA::White);
        mFrame.RequestDraw(dynamic, 25.0f, Dia::Graphics::RGBA::Red);
        mFrame.RequestDraw(center, dynamic, Dia::Graphics::RGBA::White);
    }

    // Draw debug sprites (mirrors v1 SimRunningPhase). Texture IDs come from
    // the texture handler on MainPU via static accessor.
    if (Dia::SFML::TextureHandler* tex = KernelModule::GetStaticTextureHandler())
    {
        unsigned int redId   = tex->GetTextureId(Dia::Core::StringCRC("texture.test_red"));
        unsigned int blueId  = tex->GetTextureId(Dia::Core::StringCRC("texture.test_blue"));
        unsigned int greenId = tex->GetTextureId(Dia::Core::StringCRC("texture.test_green"));

        if (redId != 0)
        {
            Dia::Graphics::SpriteDrawCommand redSprite(redId, Dia::Maths::Vector2D(200.0f, 200.0f));
            mFrame.RequestDrawSprite(redSprite);

            Dia::Graphics::SpriteDrawCommand transparentSprite(redId, Dia::Maths::Vector2D(200.0f, 300.0f));
            transparentSprite.tint = Dia::Graphics::RGBA(255, 255, 255, 128);
            mFrame.RequestDrawSprite(transparentSprite);
        }

        if (blueId != 0)
        {
            Dia::Graphics::SpriteDrawCommand blueSprite(blueId, Dia::Maths::Vector2D(300.0f, 200.0f));
            blueSprite.scale = Dia::Maths::Vector2D(1.5f, 1.5f);
            mFrame.RequestDrawSprite(blueSprite);
        }

        if (greenId != 0)
        {
            Dia::Graphics::SpriteDrawCommand greenSprite(greenId, Dia::Maths::Vector2D(400.0f, 200.0f));
            greenSprite.rotation = 45.0f;
            mFrame.RequestDrawSprite(greenSprite);
        }
    }

    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());

    // --- UI commands ---
    float fps = 0.0f;
    if (TimeServerModule* ts = mTimeServer.Get())
    {
        float delta = ts->GetDeltaTime();
        fps = (delta > 0.0f) ? (1.0f / delta) : 0.0f;
    }

    UICommand fpsCmd;
    fpsCmd.type  = UICommand::Type::kUpdateValue;
    fpsCmd.key   = Dia::Core::StringCRC("FPS");
    fpsCmd.value = fps;
    mUIOutput.Send(fpsCmd);

    UICommand scoreCmd;
    scoreCmd.type  = UICommand::Type::kUpdateValue;
    scoreCmd.key   = Dia::Core::StringCRC("Score");
    scoreCmd.value = mScore;
    mUIOutput.Send(scoreCmd);

    // Accumulate score to prove the pipeline is live
    mScore += dt;
}

Dia::ApplicationFlow::StopResult DummyLevelModule::DoStop()
{
    DIA_LOG_INFO("Application", "DummyLevelModule DoStop entry");
    mLoadEntryLogged = false;
    return Dia::ApplicationFlow::StopResult::kDone;
}

void DummyLevelModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mRenderOutput.Connect(app);
    mUIOutput.Connect(app);
    mUIInput.Connect(app);
}

} } // namespace Cluiche::AppFlow

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using DummyLevelModule_ = Cluiche::AppFlow::DummyLevelModule; }
DIA_MODULE(DummyLevelModule_);
