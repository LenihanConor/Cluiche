// F2Fingerprint: plain value snapshot of the determinism-relevant runtime state (8 RNG streams
// plus GameSpeedController state), compared field by field (no hashing) so a mismatch names
// the exact field. Spec: docs/specs/applications/cow/systems/f2-core-runtime/f2-core-runtime.md
#pragma once

#include <Shared/GameSpeedController.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Dia { namespace Maths { class RandomStream; } }

namespace CoW {

class RandomStreamModule;
class GameSpeedControllerModule;

constexpr int kF2FingerprintStreamCount = 8;

struct F2StreamFingerprint
{
    uint32_t seed      = 0;
    uint64_t drawCount = 0;
};

struct F2CompareResult
{
    std::vector<std::string> differingFields;   // e.g. "stream A1 drawCount", "dayCounter"

    bool IsIdentical() const { return differingFields.empty(); }
    // "identical" or "3 fields differ: a, b, c"
    std::string Summary() const;
};

struct F2Fingerprint
{
    F2StreamFingerprint streams[kF2FingerprintStreamCount];   // order of RandomStreamModule::kStreamNames
    uint64_t            dayCounter        = 0;
    SpeedTier           speedTier         = SpeedTier::Paused;
    PauseSource         pauseSource       = PauseSource::Player;
    uint8_t             modalDepth        = 0;
    SpeedTier           preModalSpeedTier = SpeedTier::Normal;

    // Core capture: null stream entries are recorded as zeros.
    static F2Fingerprint Capture(Dia::Maths::RandomStream* const (&streams)[kF2FingerprintStreamCount],
                                 const GameSpeedState& speedState);
    // Thin module overload.
    static F2Fingerprint Capture(RandomStreamModule& randomStreams, const GameSpeedControllerModule& gameSpeed);

    static F2CompareResult Compare(const F2Fingerprint& a, const F2Fingerprint& b);
};

inline bool operator==(const F2Fingerprint& a, const F2Fingerprint& b) { return F2Fingerprint::Compare(a, b).IsIdentical(); }
inline bool operator!=(const F2Fingerprint& a, const F2Fingerprint& b) { return !(a == b); }

} // namespace CoW
