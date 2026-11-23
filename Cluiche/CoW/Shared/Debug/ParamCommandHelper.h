#pragma once

#include <stdint.h>

#include <vector>

#include <DiaAttribute/ParameterFile.h>
#include <DiaCore/Json/external/json/json.h>
#include <Shared/Debug/SimCommandSet.h>
#include <Shared/World/WorldSimNames.h>

namespace CoW {
namespace Debug {

// One sim-hosted command cow.<sys>.params bound to a system's ParameterFile. SimPU only.
// Actions: get, list (this class), set / reload / save_defaults (added with their own tasks).
// Replies use the SimQueryHost shape: {"success":true,...} or {"success":false,"error":"<names the key>"}.
//
// Value source (default | pack:<id> | set): ParameterFile does not track provenance, so the helper does. The owner
// reports each pack it applies (NotePackApplied, in application order; a later pack wins a key) and ClearSources()
// after a reload. A value changed through the command's own `set` is marked set. A key nobody reported is `default`.
class ParamCommandHelper
{
public:
    static const uint32_t kMaxPacks = 16;
    static const uint32_t kPackIdCapacity = 64;

    // sysToken: a World:: system token. The ParameterFile must outlive the helper.
    ParamCommandHelper(const char* sysToken, Dia::Attribute::ParameterFile& file);

    // The command name (cow.<sys>.params); empty for an unknown token.
    const char* CommandName() const { return mName.c_str(); }

    // Builds the registration entry (name, mandatory help, handler bound to this helper). Register it through a
    // SimCommandSet; the helper must outlive that registration.
    SimCommandEntry BuildEntry();

    // The command handler (also called directly by tests).
    Json::Value Handle(const Json::Value& args);

    // Marks every parameter present in `pack` as coming from pack `packId`. false when the pack table is full.
    bool NotePackApplied(const char* packId, const Json::Value& pack);
    void ClearSources();

private:
    static const uint8_t kSourceDefault = 0;
    static const uint8_t kSourceSet = 255;

    Json::Value Get(const Json::Value& args) const;
    Json::Value List(const Json::Value& args) const;
    Json::Value Describe(uint32_t index) const;
    int32_t FindIndex(const char* key) const;
    uint8_t SourceOf(uint32_t index) const;
    void MarkKey(const char* key, uint8_t source);
    static Json::Value Error(const char* text, const char* key);

    World::Name mName;
    Dia::Attribute::ParameterFile& mFile;
    std::vector<uint8_t> mSources;      // per attribute, schema order
    char mPackIds[kMaxPacks][kPackIdCapacity];
    uint32_t mPackCount = 0;
};

}  // namespace Debug
}  // namespace CoW
