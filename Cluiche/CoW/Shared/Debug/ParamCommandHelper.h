#pragma once

#include <stdint.h>

#include <functional>
#include <string>
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
// One queued parameter change, applied by the owner at the next Capture through ParamCommandHelper::ApplyMutation.
struct ParamMutation
{
    enum class Kind : uint8_t { kSet, kReload };
    static const uint32_t kKeyCapacity = 128;

    Kind kind = Kind::kSet;
    char key[kKeyCapacity] = {};
    Json::Value value;      // kSet: the validated, normalised value (number, bool or enum id string)
};

// What the owner's reload callback reports (preview at queue time, real at apply time).
struct ParamReloadResult
{
    std::string error;                        // non-empty on failure
    std::vector<std::string> packIds;         // in application order
    std::vector<Json::Value> packBodies;      // parallel to packIds; used to record value sources on apply
    std::vector<std::string> warnings;
};

// The day the next Capture will run and the defer lag k (0 = not deferred).
struct ParamDayInfo
{
    uint32_t effectiveDay = 0;
    uint32_t deferLag = 0;
};

// Ordered user packs plus one optional preset pack slot, always placed last. ApplyPreset replaces the slot (never
// accumulates), ClearPreset empties it. Compose yields the ordered list a ReloadFn feeds to ParameterFile::Reload
// (and ParamCommandHelper::ApplyMutation then records as value sources: the preset id shows as pack:<presetId>).
// The preset id is the one World::PresetPath(sys, id) accepts (known system token, non-empty id).
class PresetPackLayer
{
public:
    explicit PresetPackLayer(const char* sysToken);

    // Appends a user pack (kept in call order, always before the preset). false for a null/empty id or non-object body.
    bool AddUserPack(const char* packId, const Json::Value& body);
    // Replaces the preset slot. false (slot unchanged) for an unknown system, empty id or non-object body.
    bool ApplyPreset(const char* presetId, const Json::Value& body);
    void ClearPreset();
    bool HasPreset() const { return mHasPreset; }
    const char* PresetId() const { return mPresetId.c_str(); }

    // Fills packIds/packBodies: user packs in order, then the preset if any. Leaves error/warnings untouched.
    void Compose(ParamReloadResult& out) const;

private:
    bool mSysKnown = false;
    std::vector<std::string> mUserIds;
    std::vector<Json::Value> mUserBodies;
    bool mHasPreset = false;
    std::string mPresetId;
    Json::Value mPresetBody;
};

class ParamCommandHelper
{
public:
    // Queues a mutation for the owner's next Capture; false when the queue is full.
    typedef std::function<bool(const ParamMutation&)> MutationEnqueue;
    // apply=false: validate and preview only (change nothing). apply=true: perform the ParameterFile::Reload.
    // Returns false (result.error set) on failure.
    typedef std::function<bool(bool apply, ParamReloadResult& result)> ReloadFn;
    typedef std::function<ParamDayInfo()> DayProvider;

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

    // Seams, set by the owner. Without an enqueue seam `set` and `reload` reply with an error.
    void SetMutationEnqueue(MutationEnqueue enqueue) { mEnqueue = std::move(enqueue); }
    void SetDayProvider(DayProvider provider) { mDayProvider = std::move(provider); }
    void SetReloadFn(ReloadFn fn) { mReloadFn = std::move(fn); }

    // save_defaults seams. Read returns the sidecar text and its path (false = unreadable); Write replaces the sidecar
    // text (false = failed). The helper does no file I/O itself. Without them save_defaults replies with an error.
    typedef std::function<bool(std::string& text, std::string& path)> SidecarReadFn;
    typedef std::function<bool(const std::string& text)> SidecarWriteFn;
    void SetSidecarIO(SidecarReadFn read, SidecarWriteFn write) { mSidecarRead = std::move(read); mSidecarWrite = std::move(write); }

    // Dev-build gate for save_defaults. Dia has no dev/shipping flag, so this is CoW's: a dev build unless the
    // COW_SHIPPING_BUILD macro is defined. Tests and owners may inject their own answer.
    typedef std::function<bool()> DevBuildFn;
    static bool DefaultIsDevBuild();
    void SetDevBuildFn(DevBuildFn fn) { mDevBuild = std::move(fn); }

    // Any action that is not get/list/set/reload/save_defaults goes here when set (e.g. a system's `cycle`).
    typedef std::function<Json::Value(const std::string& action, const Json::Value& args)> SystemActionHandler;
    void SetSystemActionHandler(SystemActionHandler handler) { mSystemAction = std::move(handler); }

    // Called by the owner's stage (ApplyEntry, inside Capture) for a drained mutation. kSet: ParameterFile::Set then
    // marks the key `set`. kReload: runs the reload callback, then ClearSources + NotePackApplied per pack.
    // false when the change was refused (logged); the file is unchanged in that case.
    bool ApplyMutation(const ParamMutation& mutation);

private:
    static const uint8_t kSourceDefault = 0;
    static const uint8_t kSourceSet = 255;

    Json::Value Get(const Json::Value& args) const;
    Json::Value List(const Json::Value& args) const;
    Json::Value SetAction(const Json::Value& args);
    Json::Value ReloadAction();
    Json::Value SaveDefaultsAction();
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
    MutationEnqueue mEnqueue;
    DayProvider mDayProvider;
    ReloadFn mReloadFn;
    SidecarReadFn mSidecarRead;
    SidecarWriteFn mSidecarWrite;
    DevBuildFn mDevBuild;
    SystemActionHandler mSystemAction;
};

}  // namespace Debug
}  // namespace CoW
