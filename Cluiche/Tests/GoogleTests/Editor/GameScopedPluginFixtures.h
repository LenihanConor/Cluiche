#pragma once

// Shared, test-only fixtures for the game-scoped editor plugin integration tests.
//
//  - StubGamePlugin:    a game-scoped editor plugin ("StubGamePlugin") that is only compiled into GoogleTests.
//  - StubGenericPlugin: a plain (not game-scoped) plugin used to prove generic plugins are unaffected.
//  - Fixture files:     stub_game.diagame (editor.manifests -> stub_editor.diaapp listing StubGamePlugin)
//                       and other_game.diagame (no editor key). Written under a scratch directory so the
//                       tests do not depend on the working directory containing checked-in data.
//  - ResetEditorRegistry(): clears the global registry's game scope / tags so tests stay independent.

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace GameScopedFixtures
{
    inline const char* kStubGameType    = "StubGamePlugin";
    inline const char* kStubGenericType = "StubGenericPlugin";

    // Counters shared by every stub instance so tests can observe lifecycle calls.
    struct StubCounters
    {
        int loads = 0;
        int unloads = 0;
        int navigates = 0;
        int saves = 0;
        bool dirty = false;
        bool saveResult = true;

        void Reset() { *this = StubCounters(); }
    };

    inline StubCounters& GameCounters()    { static StubCounters c; return c; }
    inline StubCounters& GenericCounters() { static StubCounters c; return c; }

    class StubPluginBase : public Dia::Editor::IEditorPlugin
    {
    public:
        StubPluginBase(const char* name, StubCounters& counters) : mCounters(counters)
        {
            strncpy_s(mName, sizeof(mName), name, _TRUNCATE);
        }
        const char* GetName() const override { return mName; }
        const char* GetVersion() const override { return "1.0"; }
        const char* GetDescription() const override { return "integration test stub"; }
        const char* GetUIPath() const override { return "dia://stub"; }
        Dia::Editor::LayoutMode GetLayoutMode() const override { return Dia::Editor::LayoutMode::kDockable; }
        void OnLoad(const Dia::Editor::EditorPluginContext&) override { ++mCounters.loads; }
        void OnUnload() override { ++mCounters.unloads; }
        void OnUpdate(float) override {}
        void OnNavigate(const Dia::Core::StringCRC&) override { ++mCounters.navigates; }
        bool IsDirty() const override { return mCounters.dirty; }
        bool Save() override
        {
            ++mCounters.saves;
            if (mCounters.saveResult)
                mCounters.dirty = false;
            return mCounters.saveResult;
        }

    private:
        char mName[64];
        StubCounters& mCounters;
    };

    class StubGamePlugin : public StubPluginBase
    {
    public:
        StubGamePlugin() : StubPluginBase("Stub Game Plugin", GameCounters()) {}
    };

    class StubGenericPlugin : public StubPluginBase
    {
    public:
        StubGenericPlugin() : StubPluginBase("Stub Generic Plugin", GenericCounters()) {}
    };

    template <typename T>
    class StubFactory : public Dia::Editor::IEditorPluginFactory
    {
    public:
        Dia::Editor::IEditorPlugin* Create() override { return new T(); }
        Dia::Editor::EditorPluginInfo GetPluginInfo() override
        {
            T tmp;
            Dia::Editor::EditorPluginInfo info = {};
            strncpy_s(info.name, sizeof(info.name), tmp.GetName(), _TRUNCATE);
            strncpy_s(info.version, sizeof(info.version), tmp.GetVersion(), _TRUNCATE);
            strncpy_s(info.description, sizeof(info.description), tmp.GetDescription(), _TRUNCATE);
            info.layoutMode = tmp.GetLayoutMode();
            return info;
        }
    };

    // Registers both stubs with the global registry (idempotent). Registered from test code only.
    inline void EnsureStubsRegistered()
    {
        static StubFactory<StubGamePlugin> sGameFactory;
        static StubFactory<StubGenericPlugin> sGenericFactory;
        Dia::Editor::EditorPluginRegistry& r = Dia::Editor::EditorPluginRegistry::Instance();
        if (!r.IsPluginRegistered(Dia::Core::StringCRC(kStubGameType)))
            r.RegisterPlugin(Dia::Core::StringCRC(kStubGameType), &sGameFactory);
        if (!r.IsPluginRegistered(Dia::Core::StringCRC(kStubGenericType)))
            r.RegisterPlugin(Dia::Core::StringCRC(kStubGenericType), &sGenericFactory);
    }

    inline void ResetEditorRegistry()
    {
        Dia::Editor::EditorPluginRegistry& r = Dia::Editor::EditorPluginRegistry::Instance();
        r.ClearGameScope();
        r.ClearActiveManifests();
        r.TagGameScoped(Dia::Core::StringCRC(kStubGameType), Dia::Core::StringCRC());
        r.TagPluginManifest(Dia::Core::StringCRC(kStubGameType), Dia::Core::StringCRC());
        r.TagPluginManifest(Dia::Core::StringCRC(kStubGenericType), Dia::Core::StringCRC());
    }

    // ----- fixture files ---------------------------------------------------------------------

    struct FixtureFiles
    {
        std::string dir;
        std::string stubGame;     // stub_game.diagame  (editor.manifests -> stub_editor.diaapp)
        std::string otherGame;    // other_game.diagame (no editor key)
        std::string stubManifest; // stub_editor.diaapp (lists StubGamePlugin)
    };

    inline FixtureFiles WriteFixtureFiles(const char* dirName = "game_scoped_plugin_fixtures")
    {
        FixtureFiles f;
        f.dir = dirName;
        std::filesystem::create_directories(f.dir);
        f.stubGame     = f.dir + "/stub_game.diagame";
        f.otherGame    = f.dir + "/other_game.diagame";
        f.stubManifest = f.dir + "/stub_editor.diaapp";

        {
            std::ofstream o(f.stubManifest);
            o << "{ \"editor\": { \"enabled\": true, \"plugins\": [ {\"type\": \"" << kStubGameType << "\"} ] } }\n";
        }
        {
            std::ofstream o(f.stubGame);
            o << "{ \"name\": \"StubGame\", \"version\": \"1.0\", \"imports\": [], \"config\": {},"
                 " \"editor\": { \"manifests\": [\"stub_editor.diaapp\"] } }\n";
        }
        {
            std::ofstream o(f.otherGame);
            o << "{ \"name\": \"OtherGame\", \"version\": \"1.0\", \"imports\": [], \"config\": {} }\n";
        }
        return f;
    }

    inline void RemoveFixtureFiles(const FixtureFiles& f)
    {
        std::error_code ec;
        std::filesystem::remove_all(f.dir, ec);
    }
}
