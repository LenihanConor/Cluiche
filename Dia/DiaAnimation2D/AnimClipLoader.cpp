#include "AnimClipLoader.h"
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Core/Assert.h>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

namespace Dia { namespace Animation2D {

AnimClipDef LoadAnimClipDefFromJson(const Json::Value& root) {
    DIA_ASSERT(root.isMember("id"), "AnimClip JSON missing 'id'");
    DIA_ASSERT(root.isMember("duration"), "AnimClip JSON missing 'duration'");
    DIA_ASSERT(root.isMember("tracks"), "AnimClip JSON missing 'tracks'");

    AnimClipDef def;
    def.id = Dia::Core::StringCRC(root["id"].asCString());
    def.duration = root["duration"].asFloat();

    const Json::Value& tracks = root["tracks"];
    for (const Json::Value& track : tracks) {
        DIA_ASSERT(track.isMember("bone"), "Track JSON missing 'bone'");
        DIA_ASSERT(track.isMember("keyframes"), "Track JSON missing 'keyframes'");

        KeyframeTrack kt;
        kt.boneId = Dia::Core::StringCRC(track["bone"].asCString());

        const Json::Value& keyframes = track["keyframes"];
        for (const Json::Value& kf : keyframes) {
            Keyframe k;
            k.time = kf["time"].asFloat();

            if (kf.isMember("position") && kf["position"].isArray()) {
                k.position = Dia::Maths::Vector2D(kf["position"][0].asFloat(), kf["position"][1].asFloat());
            } else {
                k.position = Dia::Maths::Vector2D(0.0f, 0.0f);
            }
            if (kf.isMember("rotation")) {
                k.rotation = kf["rotation"].asFloat();
            } else {
                k.rotation = 0.0f;
            }
            if (kf.isMember("scale") && kf["scale"].isArray()) {
                k.scale = Dia::Maths::Vector2D(kf["scale"][0].asFloat(), kf["scale"][1].asFloat());
            } else {
                k.scale = Dia::Maths::Vector2D(1.0f, 1.0f);
            }

            kt.keyframes.Add(k);
        }
        def.tracks.Add(kt);
    }

    return def;
}

AnimClipDef LoadAnimClipDefFromSpine(const Json::Value& spineRoot, const Dia::Core::StringCRC& animationName) {
    DIA_ASSERT(spineRoot.isMember("animations"), "Spine JSON missing 'animations'");

    const Json::Value& animations = spineRoot["animations"];

    const Json::Value* animNode = nullptr;
    for (const auto& memberName : animations.getMemberNames()) {
        if (Dia::Core::StringCRC(memberName.c_str()) == animationName) {
            animNode = &animations[memberName];
            break;
        }
    }
    DIA_ASSERT(animNode != nullptr, "Animation name not found in Spine JSON");

    AnimClipDef def;
    def.id = animationName;
    def.duration = 0.0f;

    if (!animNode->isMember("bones")) return def;

    const Json::Value& bones = (*animNode)["bones"];
    const float kDegToRad = 3.14159265358979f / 180.0f;

    for (const auto& boneName : bones.getMemberNames()) {
        const Json::Value& boneTimelines = bones[boneName];
        Dia::Core::StringCRC boneId(boneName.c_str());

        std::vector<float> allTimes;
        auto collectTimes = [&](const char* key) {
            if (boneTimelines.isMember(key)) {
                for (const auto& entry : boneTimelines[key]) {
                    allTimes.push_back(entry["time"].asFloat());
                }
            }
        };
        collectTimes("rotate");
        collectTimes("translate");
        collectTimes("scale");

        std::sort(allTimes.begin(), allTimes.end());
        allTimes.erase(std::unique(allTimes.begin(), allTimes.end()), allTimes.end());

        if (allTimes.empty()) continue;

        KeyframeTrack kt;
        kt.boneId = boneId;

        for (float t : allTimes) {
            if (t > def.duration) def.duration = t;

            Keyframe kf;
            kf.time = t;

            float rotDeg = 0.0f;
            if (boneTimelines.isMember("rotate")) {
                const Json::Value& rtl = boneTimelines["rotate"];
                for (const auto& entry : rtl) {
                    if (entry["time"].asFloat() <= t + 1e-6f)
                        rotDeg = entry["angle"].asFloat();
                    else break;
                }
            }
            kf.rotation = rotDeg * kDegToRad;

            float tx = 0.0f, ty = 0.0f;
            if (boneTimelines.isMember("translate")) {
                const Json::Value& ttl = boneTimelines["translate"];
                for (const auto& entry : ttl) {
                    if (entry["time"].asFloat() <= t + 1e-6f) {
                        tx = entry.isMember("x") ? entry["x"].asFloat() : 0.0f;
                        ty = entry.isMember("y") ? entry["y"].asFloat() : 0.0f;
                    } else break;
                }
            }
            kf.position = Dia::Maths::Vector2D(tx, -ty);

            float sx = 1.0f, sy = 1.0f;
            if (boneTimelines.isMember("scale")) {
                const Json::Value& stl = boneTimelines["scale"];
                for (const auto& entry : stl) {
                    if (entry["time"].asFloat() <= t + 1e-6f) {
                        sx = entry.isMember("x") ? entry["x"].asFloat() : 1.0f;
                        sy = entry.isMember("y") ? entry["y"].asFloat() : 1.0f;
                    } else break;
                }
            }
            kf.scale = Dia::Maths::Vector2D(sx, sy);

            kt.keyframes.Add(kf);
        }
        def.tracks.Add(kt);
    }

    return def;
}

} }
