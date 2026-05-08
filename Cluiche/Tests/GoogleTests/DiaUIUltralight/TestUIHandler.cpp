#include <gtest/gtest.h>

#include <DiaUIUltralight/UIHandler.h>
#include <DiaUI/IUISystem.h>
#include <DiaAsset/IAssetTypeHandler.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String512.h>

namespace
{
    struct TestCallback : public Dia::AssetRuntime::IAssetLoadCallback
    {
        bool completed = false;
        bool failed = false;
        const char* failReason = nullptr;

        void OnLoadComplete(const Dia::Core::StringCRC&) override { completed = true; }
        void OnLoadFailed(const Dia::Core::StringCRC&, const char* reason) override
        {
            failed = true;
            failReason = reason;
        }
    };

    struct DummyUISystem : public Dia::UI::IUISystem
    {
        void Initialize() override {}
        void Shutdown() override {}
        void LoadPage(Dia::UI::Page&) override {}
        void UnloadPage() override {}
        bool IsPageLoaded() const override { return false; }
        void Update() override {}
        void FetchUIDataBuffer(Dia::UI::UIDataBuffer&) const override {}
        Dia::UI::IPage* CreatePage(const char*, int, int) override { return nullptr; }
        void DestroyPage(Dia::UI::IPage*) override {}
        int GetPageCount() const override { return 0; }
        void InjectMouseMove(int, int) override {}
        void InjectMouseDown(Dia::Input::EMouseButton, int, int) override {}
        void InjectMouseUp(Dia::Input::EMouseButton, int, int) override {}
        void InjectMouseClick(Dia::Input::EMouseButton, int, int) override {}
        void InjectMouseWheel(int, int) override {}
    };
}

TEST(UIHandlerTest, Load_NoUISystem_CallsOnLoadFailed)
{
    Dia::UI::Ultralight::UIHandler handler;
    TestCallback cb;

    Dia::Core::StringCRC assetId("ui.page");
    Dia::Core::Containers::String512 path("ui/page.html");

    handler.Load(assetId, path, &cb);

    EXPECT_TRUE(cb.failed);
    EXPECT_FALSE(cb.completed);
}

TEST(UIHandlerTest, Load_WithUISystem_CallsOnLoadComplete)
{
    Dia::UI::Ultralight::UIHandler handler;
    DummyUISystem uiSystem;
    handler.SetUISystem(&uiSystem);
    TestCallback cb;

    Dia::Core::StringCRC assetId("ui.page");
    Dia::Core::Containers::String512 path("ui/page.html");

    handler.Load(assetId, path, &cb);

    EXPECT_TRUE(cb.completed);
    EXPECT_FALSE(cb.failed);
}

TEST(UIHandlerTest, Unload_DoesNotCrash)
{
    Dia::UI::Ultralight::UIHandler handler;
    Dia::Core::StringCRC assetId("ui.page");
    handler.Unload(assetId);
}
