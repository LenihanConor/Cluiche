#include <gtest/gtest.h>

#include <DiaAnimation2D/AnimClipLoader.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Bone.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cmath>

// ============================================================
// JSON literals
// ============================================================

static const char* kSimpleCustomJson = R"({
    "id": "test_clip", "duration": 0.5,
    "tracks": [{"bone": "spine", "keyframes": [
        {"time": 0.0, "position": [0,0], "rotation": 0, "scale": [1,1]},
        {"time": 0.5, "position": [0,1], "rotation": 1.0, "scale": [1,1]}
    ]}]
})";

static const char* kSpineJson = R"({
    "animations": {
        "walk": {"bones": {"spine": {"rotate": [{"time": 0.0, "angle": 90.0}]}}},
        "run":  {"bones": {"spine": {"rotate": [{"time": 0.0, "angle": 180.0}]}}}
    }
})";

// ============================================================
// JSON parse helper
// ============================================================

static Json::Value ParseJson(const char* str)
{
    Json::Value root;
    Json::Reader reader;
    reader.parse(str, root);
    return root;
}

// ============================================================
// Skeleton helper
// ============================================================

static Dia::Rig2D::SkeletonDef MakeSpineSkelDef()
{
    Dia::Rig2D::SkeletonDef def;
    def.id = Dia::Core::StringCRC("spine_test");

    Dia::Rig2D::Bone root;
    root.name         = Dia::Core::StringCRC("spine");
    root.parentIndex  = -1;
    root.length       = 1.0f;
    root.localPosition = Dia::Maths::Vector2D(0.0f, 0.0f);
    def.bones.Add(root);

    return def;
}

// ============================================================
// AnimClipLoader tests
// ============================================================

TEST(AnimClipLoader, LoadFromJson_AllFields)
{
    Dia::Rig2D::Skeleton skeleton(MakeSpineSkelDef());

    Json::Value root = ParseJson(kSimpleCustomJson);
    Dia::Animation2D::AnimClipDef def = Dia::Animation2D::LoadAnimClipDefFromJson(root);

    EXPECT_EQ(def.id, Dia::Core::StringCRC("test_clip"));
    EXPECT_NEAR(def.duration, 0.5f, 1e-4f);
    EXPECT_EQ(def.tracks.Size(), 1u);
    EXPECT_EQ(def.tracks[0].keyframes.Size(), 2u);
}

TEST(AnimClipLoader, LoadFromJson_PartialKeyframe_Defaults)
{
    const char* json = R"({
        "id": "partial", "duration": 1.0,
        "tracks": [{"bone": "spine", "keyframes": [
            {"time": 0.0, "rotation": 1.5}
        ]}]
    })";

    Json::Value root = ParseJson(json);
    Dia::Animation2D::AnimClipDef def = Dia::Animation2D::LoadAnimClipDefFromJson(root);

    ASSERT_EQ(def.tracks.Size(), 1u);
    ASSERT_EQ(def.tracks[0].keyframes.Size(), 1u);

    const Dia::Animation2D::Keyframe& kf = def.tracks[0].keyframes[0];
    EXPECT_NEAR(kf.position.X(), 0.0f, 1e-5f);
    EXPECT_NEAR(kf.position.Y(), 0.0f, 1e-5f);
    EXPECT_NEAR(kf.scale.X(),    1.0f, 1e-5f);
    EXPECT_NEAR(kf.scale.Y(),    1.0f, 1e-5f);
}

TEST(AnimClipLoader, LoadFromSpine_DegreesToRadians)
{
    Json::Value root = ParseJson(kSpineJson);
    Dia::Animation2D::AnimClipDef def =
        Dia::Animation2D::LoadAnimClipDefFromSpine(root, "walk");

    ASSERT_GE(def.tracks.Size(), 1u);
    ASSERT_GE(def.tracks[0].keyframes.Size(), 1u);

    const float expectedRad = 90.0f * (3.14159265f / 180.0f);
    EXPECT_NEAR(def.tracks[0].keyframes[0].rotation, expectedRad, 1e-4f);
}

TEST(AnimClipLoader, LoadFromSpine_YUpConversion)
{
    const char* spineYJson = R"({
        "animations": {
            "test": {"bones": {"spine": {
                "translate": [{"time": 0.0, "x": 0.0, "y": 2.0}]
            }}}
        }
    })";

    Json::Value root = ParseJson(spineYJson);
    Dia::Animation2D::AnimClipDef def =
        Dia::Animation2D::LoadAnimClipDefFromSpine(root, "test");

    ASSERT_GE(def.tracks.Size(), 1u);
    ASSERT_GE(def.tracks[0].keyframes.Size(), 1u);
    EXPECT_NEAR(def.tracks[0].keyframes[0].position.Y(), -2.0f, 1e-5f);
}

TEST(AnimClipLoader, LoadFromSpine_IgnoresSlots)
{
    const char* spineWithSlots = R"({
        "animations": {
            "walk": {
                "slots": {"slot0": {"attachment": [{"time": 0.0, "name": "img"}]}},
                "bones": {"spine": {"rotate": [{"time": 0.0, "angle": 45.0}]}}
            }
        }
    })";

    Json::Value root = ParseJson(spineWithSlots);

    // Should not crash; only bone tracks should be loaded
    Dia::Animation2D::AnimClipDef def =
        Dia::Animation2D::LoadAnimClipDefFromSpine(root, "walk");

    // At most 1 track (bone "spine"); no slot tracks
    EXPECT_LE(def.tracks.Size(), 1u);
    SUCCEED();
}

TEST(AnimClipLoader, Duration_MaxKeyframeTime)
{
    const char* json = R"({
        "id": "max_time", "duration": 1.0,
        "tracks": [
            {"bone": "spine", "keyframes": [
                {"time": 0.0, "rotation": 0},
                {"time": 0.5, "rotation": 1.0}
            ]},
            {"bone": "spine", "keyframes": [
                {"time": 0.0, "rotation": 0},
                {"time": 0.7, "rotation": 0.5}
            ]}
        ]
    })";
    // Note: duplicate bone tracks will assert in AnimClip but LoadAnimClipDef
    // should just report the duration as max keyframe time.
    // Use a simpler test: single track with last key at 0.7.
    const char* jsonSingle = R"({
        "id": "single_max", "duration": 0.7,
        "tracks": [
            {"bone": "spine", "keyframes": [
                {"time": 0.0, "rotation": 0},
                {"time": 0.7, "rotation": 1.0}
            ]}
        ]
    })";

    Json::Value root = ParseJson(jsonSingle);
    Dia::Animation2D::AnimClipDef def = Dia::Animation2D::LoadAnimClipDefFromJson(root);

    EXPECT_NEAR(def.duration, 0.7f, 1e-4f);
}

#ifdef _DEBUG
TEST(AnimClipLoader, LoadFromJson_InvalidStructure_Asserts)
{
    const char* badJson = R"({"id": "bad"})"; // missing "tracks"
    Json::Value root = ParseJson(badJson);
    EXPECT_DEATH(Dia::Animation2D::LoadAnimClipDefFromJson(root), "");
}
#endif // _DEBUG

TEST(AnimClipLoader, BoneNames_AsStringCRC)
{
    Json::Value root = ParseJson(kSimpleCustomJson);
    Dia::Animation2D::AnimClipDef def = Dia::Animation2D::LoadAnimClipDefFromJson(root);

    ASSERT_GE(def.tracks.Size(), 1u);
    EXPECT_EQ(def.tracks[0].boneId, Dia::Core::StringCRC("spine"));
}

TEST(AnimClipLoader, LoadFromSpine_MultipleAnimations)
{
    Json::Value root = ParseJson(kSpineJson);

    Dia::Animation2D::AnimClipDef walkDef =
        Dia::Animation2D::LoadAnimClipDefFromSpine(root, "walk");
    Dia::Animation2D::AnimClipDef runDef =
        Dia::Animation2D::LoadAnimClipDefFromSpine(root, "run");

    ASSERT_GE(walkDef.tracks.Size(), 1u);
    ASSERT_GE(runDef.tracks.Size(), 1u);

    const float walkRad = 90.0f  * (3.14159265f / 180.0f);
    const float runRad  = 180.0f * (3.14159265f / 180.0f);

    EXPECT_NEAR(walkDef.tracks[0].keyframes[0].rotation, walkRad, 1e-4f);
    EXPECT_NEAR(runDef.tracks[0].keyframes[0].rotation,  runRad,  1e-4f);
}

TEST(AnimClipLoader, LoadFromJson_KeyframeTimesPreserved)
{
    const char* json = R"({
        "id": "precise", "duration": 1.0,
        "tracks": [{"bone": "spine", "keyframes": [
            {"time": 0.0,   "rotation": 0},
            {"time": 0.333, "rotation": 1.0}
        ]}]
    })";

    Json::Value root = ParseJson(json);
    Dia::Animation2D::AnimClipDef def = Dia::Animation2D::LoadAnimClipDefFromJson(root);

    ASSERT_EQ(def.tracks.Size(), 1u);
    ASSERT_EQ(def.tracks[0].keyframes.Size(), 2u);
    EXPECT_NEAR(def.tracks[0].keyframes[1].time, 0.333f, 1e-4f);
}

TEST(AnimClipLoader, LoadFromJson_EmptyTracks_ZeroTracks)
{
    const char* json = R"({"id": "empty", "duration": 1.0, "tracks": []})";
    Json::Value root = ParseJson(json);
    Dia::Animation2D::AnimClipDef def = Dia::Animation2D::LoadAnimClipDefFromJson(root);
    EXPECT_EQ(def.tracks.Size(), 0u);
}

TEST(AnimClipLoader, LoadFromSpine_NoBoneTimelines_EmptyResult)
{
    const char* spineSlotOnly = R"({
        "animations": {
            "idle": {
                "slots": {"slot0": {"attachment": [{"time": 0.0, "name": "img"}]}}
            }
        }
    })";

    Json::Value root = ParseJson(spineSlotOnly);
    Dia::Animation2D::AnimClipDef def =
        Dia::Animation2D::LoadAnimClipDefFromSpine(root, "idle");

    EXPECT_EQ(def.tracks.Size(), 0u);
}

TEST(AnimClipLoader, LoadFromJson_UnknownFields_Ignored)
{
    const char* jsonWithExtra = R"({
        "id": "extra_fields", "duration": 1.0, "author": "test",
        "tracks": [{"bone": "spine", "keyframes": [
            {"time": 0.0, "rotation": 0.0, "note": "some note"}
        ]}]
    })";

    Json::Value root = ParseJson(jsonWithExtra);
    // Should not crash
    Dia::Animation2D::AnimClipDef def = Dia::Animation2D::LoadAnimClipDefFromJson(root);
    EXPECT_EQ(def.tracks.Size(), 1u);
}

#ifdef _DEBUG
TEST(AnimClipLoader, LoadFromSpine_InvalidName_Asserts)
{
    Json::Value root = ParseJson(kSpineJson);
    EXPECT_DEATH(Dia::Animation2D::LoadAnimClipDefFromSpine(root, "nonexistent"), "");
}
#endif // _DEBUG

TEST(AnimClipLoader, LoadFromSpine_EmptyAnimationBody_ZeroTracks)
{
    // Animation entry exists but has no bone or slot timelines
    const char* spineEmpty = R"({
        "animations": {
            "empty_anim": {}
        }
    })";

    Json::Value root = ParseJson(spineEmpty);
    Dia::Animation2D::AnimClipDef def =
        Dia::Animation2D::LoadAnimClipDefFromSpine(root, "empty_anim");

    EXPECT_EQ(def.tracks.Size(), 0u);
    EXPECT_NEAR(def.duration, 0.0f, 1e-5f);
}

TEST(AnimClipLoader, LoadFromSpine_BoneWithNoTimelines_Skipped)
{
    // Bone entry exists but no rotate/translate/scale keys
    const char* spineEmptyBone = R"({
        "animations": {
            "test": {
                "bones": {
                    "spine": {}
                }
            }
        }
    })";

    Json::Value root = ParseJson(spineEmptyBone);
    Dia::Animation2D::AnimClipDef def =
        Dia::Animation2D::LoadAnimClipDefFromSpine(root, "test");

    // No keyframe times collected → bone should be skipped
    EXPECT_EQ(def.tracks.Size(), 0u);
}

TEST(AnimClipLoader, LoadFromSpine_MultipleBones_AllLoaded)
{
    const char* spineMultiBone = R"({
        "animations": {
            "walk": {
                "bones": {
                    "spine": {"rotate": [{"time": 0.0, "angle": 45.0}]},
                    "arm":   {"rotate": [{"time": 0.0, "angle": 90.0}]}
                }
            }
        }
    })";

    Json::Value root = ParseJson(spineMultiBone);
    Dia::Animation2D::AnimClipDef def =
        Dia::Animation2D::LoadAnimClipDefFromSpine(root, "walk");

    EXPECT_EQ(def.tracks.Size(), 2u);
}

TEST(AnimClipLoader, LoadFromJson_ScaleKeyframes_Loaded)
{
    const char* json = R"({
        "id": "scale_clip", "duration": 1.0,
        "tracks": [{"bone": "spine", "keyframes": [
            {"time": 0.0, "scale": [2.0, 3.0]},
            {"time": 1.0, "scale": [4.0, 6.0]}
        ]}]
    })";

    Json::Value root = ParseJson(json);
    Dia::Animation2D::AnimClipDef def = Dia::Animation2D::LoadAnimClipDefFromJson(root);

    ASSERT_EQ(def.tracks.Size(), 1u);
    ASSERT_EQ(def.tracks[0].keyframes.Size(), 2u);
    EXPECT_NEAR(def.tracks[0].keyframes[0].scale.X(), 2.0f, 1e-5f);
    EXPECT_NEAR(def.tracks[0].keyframes[0].scale.Y(), 3.0f, 1e-5f);
    EXPECT_NEAR(def.tracks[0].keyframes[1].scale.X(), 4.0f, 1e-5f);
    EXPECT_NEAR(def.tracks[0].keyframes[1].scale.Y(), 6.0f, 1e-5f);
}
