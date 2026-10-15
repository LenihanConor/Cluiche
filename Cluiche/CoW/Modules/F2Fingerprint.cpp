#include "Modules/F2Fingerprint.h"
#include "Modules/RandomStreamModule.h"
#include "Modules/GameSpeedControllerModule.h"
#include <DiaMaths/Core/RandomStream.h>

namespace CoW {

std::string F2CompareResult::Summary() const
{
    if (differingFields.empty())
        return "identical";

    std::string text = std::to_string(differingFields.size()) + (differingFields.size() == 1 ? " field differs: " : " fields differ: ");
    for (size_t i = 0; i < differingFields.size(); ++i)
    {
        if (i > 0)
            text += ", ";
        text += differingFields[i];
    }
    return text;
}

F2Fingerprint F2Fingerprint::Capture(Dia::Maths::RandomStream* const (&streams)[kF2FingerprintStreamCount],
                                     const GameSpeedState& speedState)
{
    F2Fingerprint fp;
    for (int i = 0; i < kF2FingerprintStreamCount; ++i)
    {
        if (streams[i])
        {
            fp.streams[i].seed      = streams[i]->GetSeed();
            fp.streams[i].drawCount = streams[i]->GetDrawCount();
        }
    }
    fp.dayCounter        = speedState.dayCounter;
    fp.speedTier         = speedState.speedTier;
    fp.pauseSource       = speedState.pauseSource;
    fp.modalDepth        = speedState.modalDepth;
    fp.preModalSpeedTier = speedState.preModalSpeedTier;
    return fp;
}

F2Fingerprint F2Fingerprint::Capture(RandomStreamModule& randomStreams, const GameSpeedControllerModule& gameSpeed)
{
    Dia::Maths::RandomStream* streams[kF2FingerprintStreamCount] = {};
    for (int i = 0; i < kF2FingerprintStreamCount; ++i)
        streams[i] = randomStreams.GetStreamByName(Dia::Core::StringCRC(RandomStreamModule::kStreamNames[i]));
    return Capture(streams, gameSpeed.GetController().GetState());
}

F2CompareResult F2Fingerprint::Compare(const F2Fingerprint& a, const F2Fingerprint& b)
{
    static_assert(kF2FingerprintStreamCount == RandomStreamModule::kStreamCount, "stream count mismatch");

    F2CompareResult result;
    for (int i = 0; i < kF2FingerprintStreamCount; ++i)
    {
        const std::string prefix = std::string("stream ") + RandomStreamModule::kStreamNames[i];
        if (a.streams[i].seed != b.streams[i].seed)
            result.differingFields.push_back(prefix + " seed");
        if (a.streams[i].drawCount != b.streams[i].drawCount)
            result.differingFields.push_back(prefix + " drawCount");
    }
    if (a.dayCounter != b.dayCounter)               result.differingFields.push_back("dayCounter");
    if (a.speedTier != b.speedTier)                 result.differingFields.push_back("speedTier");
    if (a.pauseSource != b.pauseSource)             result.differingFields.push_back("pauseSource");
    if (a.modalDepth != b.modalDepth)               result.differingFields.push_back("modalDepth");
    if (a.preModalSpeedTier != b.preModalSpeedTier) result.differingFields.push_back("preModalSpeedTier");
    return result;
}

F2CompareResult F2Fingerprint::CompareRestored(const F2Fingerprint& saved, const F2Fingerprint& loaded)
{
    F2CompareResult result;
    for (int i = 0; i < kF2FingerprintStreamCount; ++i)
    {
        const std::string prefix = std::string("stream ") + RandomStreamModule::kStreamNames[i];
        if (saved.streams[i].seed != loaded.streams[i].seed)
            result.differingFields.push_back(prefix + " seed");
        if (saved.streams[i].drawCount != loaded.streams[i].drawCount)
            result.differingFields.push_back(prefix + " drawCount");
    }
    if (saved.dayCounter != loaded.dayCounter)
        result.differingFields.push_back("dayCounter");

    const GameSpeedState defaults;
    if (loaded.speedTier != defaults.speedTier)     result.differingFields.push_back("speedTier");
    if (loaded.pauseSource != defaults.pauseSource) result.differingFields.push_back("pauseSource");
    if (loaded.modalDepth != defaults.modalDepth)   result.differingFields.push_back("modalDepth");
    // Persisted since GameSpeed save v2.
    if (saved.preModalSpeedTier != loaded.preModalSpeedTier) result.differingFields.push_back("preModalSpeedTier");
    return result;
}

} // namespace CoW
