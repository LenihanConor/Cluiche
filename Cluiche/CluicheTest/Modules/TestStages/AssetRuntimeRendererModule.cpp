#include "Modules/TestStages/AssetRuntimeRendererModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaGraphics/Frame/SpriteDrawCommand.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CluicheTest {

const Dia::Core::StringCRC AssetRuntimeRendererModule::kTypeId("AssetRuntimeRendererModule");

AssetRuntimeRendererModule::AssetRuntimeRendererModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

Dia::ApplicationFlow::StartResult AssetRuntimeRendererModule::DoStart()
{
    return Dia::ApplicationFlow::StartResult::kReady;
}

void AssetRuntimeRendererModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    mFrame.Clear();

    if (mTextureHandlerService.IsAvailable())
    {
        Dia::AssetRuntime::TextureHandler& tex = mTextureHandlerService.Get();

        Dia::Graphics::ITexture* tex1 = tex.LookupTexture(Dia::Core::StringCRC("texture.ar_tex1"));
        Dia::Graphics::ITexture* tex2 = tex.LookupTexture(Dia::Core::StringCRC("texture.ar_tex2"));
        Dia::Graphics::ITexture* tex3 = tex.LookupTexture(Dia::Core::StringCRC("texture.ar_tex3"));

        if (tex1)
            mFrame.RequestDrawSprite(Dia::Graphics::SpriteDrawCommand(tex1, Dia::Maths::Vector2D(600.0f, 150.0f)));

        if (tex2)
            mFrame.RequestDrawSprite(Dia::Graphics::SpriteDrawCommand(tex2, Dia::Maths::Vector2D(700.0f, 150.0f)));

        if (tex3)
            mFrame.RequestDrawSprite(Dia::Graphics::SpriteDrawCommand(tex3, Dia::Maths::Vector2D(800.0f, 150.0f)));
    }

    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());
}

Dia::ApplicationFlow::StopResult AssetRuntimeRendererModule::DoStop()
{
    // Flush a cleared frame so stale ITexture* pointers don't survive stage unload.
    mFrame.Clear();
    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());
    return Dia::ApplicationFlow::StopResult::kDone;
}

void AssetRuntimeRendererModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mRenderOutput.Connect(app);
    mTextureHandlerService.Connect(app);
}

} // namespace CluicheTest

namespace { using AssetRuntimeRendererModule_ = CluicheTest::AssetRuntimeRendererModule; }
DIA_MODULE(AssetRuntimeRendererModule_);
DIA_DESCRIBE(AssetRuntimeRendererModule_::kTypeId, "Renders loaded assets in the asset runtime test stage for visual verification.");
