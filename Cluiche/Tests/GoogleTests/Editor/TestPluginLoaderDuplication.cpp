#include <gtest/gtest.h>
#include <DiaEditor/Plugin/IPluginLoader.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Editor;
using namespace Dia::Core;

namespace
{
    // Minimal IPluginLoader implementation that exercises the same duplicate guard
    // used in PluginLoaderModule::LoadPlugin.
    class FakePluginLoader : public IPluginLoader
    {
    public:
        static const unsigned int kMax = 16;

        void LoadPlugin(const StringCRC& typeId, const StringCRC& /*instanceId*/) override
        {
            if (IsPluginTypeLoaded(typeId))
                return;
            if (mLoaded.IsFull())
                return;
            mLoaded.Add(typeId);
        }

        bool UnloadPlugin(const StringCRC& typeId) override
        {
            for (unsigned int i = 0; i < mLoaded.Size(); ++i)
            {
                if (mLoaded[i] == typeId)
                {
                    mLoaded.RemoveAt(i);
                    return true;
                }
            }
            return false;
        }

        bool IsPluginTypeLoaded(const StringCRC& typeId) const override
        {
            for (unsigned int i = 0; i < mLoaded.Size(); ++i)
                if (mLoaded[i] == typeId) return true;
            return false;
        }

        bool IsPluginPinned(const StringCRC& /*typeId*/) const override { return false; }

        unsigned int GetLoadedCount() const { return mLoaded.Size(); }

    private:
        Dia::Core::Containers::DynamicArrayC<StringCRC, kMax> mLoaded;
    };
}

TEST(PluginLoader, LoadPlugin_DuplicateTypeIgnored)
{
    FakePluginLoader loader;
    const StringCRC typeA("HomeEditorPlugin");

    loader.LoadPlugin(typeA, StringCRC("inst_a"));
    loader.LoadPlugin(typeA, StringCRC("inst_a"));

    EXPECT_EQ(loader.GetLoadedCount(), 1u);
    EXPECT_TRUE(loader.IsPluginTypeLoaded(typeA));
}

TEST(PluginLoader, LoadPlugin_FourBuiltins_NoDuplication)
{
    FakePluginLoader loader;
    const StringCRC builtins[] = {
        StringCRC("HomeEditorPlugin"),
        StringCRC("OutputConsoleEditorPlugin"),
        StringCRC("GameConnectionEditorPlugin"),
        StringCRC("PluginBrowserEditorPlugin"),
    };

    for (auto& t : builtins) loader.LoadPlugin(t, t);
    for (auto& t : builtins) loader.LoadPlugin(t, t);

    EXPECT_EQ(loader.GetLoadedCount(), 4u);
}

TEST(PluginLoader, LoadPlugin_ManifestRepeatDoesNotDuplicate)
{
    FakePluginLoader loader;
    const StringCRC typeA("HomeEditorPlugin");
    const StringCRC typeB("OutputConsoleEditorPlugin");

    // Simulate builtin load followed by manifest that re-lists the same types.
    loader.LoadPlugin(typeA, StringCRC("builtin_a"));
    loader.LoadPlugin(typeB, StringCRC("builtin_b"));
    loader.LoadPlugin(typeA, StringCRC("manifest_a"));
    loader.LoadPlugin(typeB, StringCRC("manifest_b"));

    EXPECT_EQ(loader.GetLoadedCount(), 2u);
}

TEST(PluginLoader, UnloadPlugin_AllowsReload)
{
    FakePluginLoader loader;
    const StringCRC typeA("HomeEditorPlugin");

    loader.LoadPlugin(typeA, StringCRC("inst"));
    EXPECT_TRUE(loader.IsPluginTypeLoaded(typeA));

    loader.UnloadPlugin(typeA);
    EXPECT_FALSE(loader.IsPluginTypeLoaded(typeA));

    loader.LoadPlugin(typeA, StringCRC("inst"));
    EXPECT_TRUE(loader.IsPluginTypeLoaded(typeA));
    EXPECT_EQ(loader.GetLoadedCount(), 1u);
}

TEST(PluginLoader, IsPluginTypeLoaded_FalseForUnknown)
{
    FakePluginLoader loader;
    EXPECT_FALSE(loader.IsPluginTypeLoaded(StringCRC("NonExistentPlugin")));
}
