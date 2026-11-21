#include "F2StreamIndependence.h"
#include "RandomStreamModule.h"
#include <DiaMaths/Core/RandomStream.h>
#include <vector>

namespace CoW {

namespace {

// Fixed, uneven interleave: every stream appears several times, none exactly equally.
int InterleaveStream(int k)
{
    return (k * k + k / 3 + k / 8) % kF2IndependenceStreamCount;
}

class FreshStreamSet : public F2IStreamSet
{
public:
    explicit FreshStreamSet(const unsigned int (&seeds)[kF2IndependenceStreamCount])
    {
        for (int i = 0; i < kF2IndependenceStreamCount; ++i)
            mStreams[i] = std::make_unique<Dia::Maths::RandomStream>(seeds[i]);
    }
    uint32_t Draw(int i) override { return static_cast<uint32_t>(mStreams[i]->RandomInt()); }

private:
    std::unique_ptr<Dia::Maths::RandomStream> mStreams[kF2IndependenceStreamCount];
};

} // namespace

F2StreamIndependenceResult CheckStreamIndependence(const F2StreamSetFactory& factory,
                                                   const char* const (&names)[kF2IndependenceStreamCount])
{
    F2StreamIndependenceResult result;

    std::vector<uint32_t> interleaved[kF2IndependenceStreamCount];
    {
        std::unique_ptr<F2IStreamSet> set = factory();
        for (int k = 0; k < kF2IndependenceInterleaveLength; ++k)
        {
            const int s = InterleaveStream(k);
            interleaved[s].push_back(set->Draw(s));
        }
    }

    for (int s = 0; s < kF2IndependenceStreamCount; ++s)
    {
        std::unique_ptr<F2IStreamSet> alone = factory();
        int mismatch = -1;
        for (int d = 0; d < static_cast<int>(interleaved[s].size()); ++d)
        {
            if (alone->Draw(s) != interleaved[s][d] && mismatch < 0)
                mismatch = d;
        }
        if (mismatch < 0)
            ++result.independentCount;
        else if (result.firstDivergingStream < 0)
        {
            result.firstDivergingStream = s;
            result.firstDivergingDraw = mismatch;
        }
    }

    result.passed = (result.independentCount == kF2IndependenceStreamCount);
    if (result.passed)
    {
        result.summary = std::to_string(result.independentCount) + " / " + std::to_string(kF2IndependenceStreamCount)
                       + " streams independent over " + std::to_string(kF2IndependenceInterleaveLength) + " draws";
    }
    else
    {
        result.summary = "stream " + std::string(names[result.firstDivergingStream]) + " diverged at draw "
                       + std::to_string(result.firstDivergingDraw) + " ("
                       + std::to_string(result.independentCount) + " / " + std::to_string(kF2IndependenceStreamCount)
                       + " independent)";
    }
    return result;
}

F2StreamIndependenceResult CheckStreamIndependence(Dia::Maths::RandomStream* const (&live)[kF2IndependenceStreamCount])
{
    unsigned int seeds[kF2IndependenceStreamCount] = {};
    for (int i = 0; i < kF2IndependenceStreamCount; ++i)
    {
        if (!live[i])
        {
            F2StreamIndependenceResult r;
            r.summary = std::string("stream ") + RandomStreamModule::kStreamNames[i] + " missing";
            r.firstDivergingStream = i;
            return r;
        }
        seeds[i] = live[i]->GetSeed();
    }
    static constexpr const char* const kNames[kF2IndependenceStreamCount] = {
        RandomStreamModule::kStreamNames[0], RandomStreamModule::kStreamNames[1],
        RandomStreamModule::kStreamNames[2], RandomStreamModule::kStreamNames[3],
        RandomStreamModule::kStreamNames[4], RandomStreamModule::kStreamNames[5],
        RandomStreamModule::kStreamNames[6], RandomStreamModule::kStreamNames[7] };
    return CheckStreamIndependence(
        [&seeds]() -> std::unique_ptr<F2IStreamSet> { return std::make_unique<FreshStreamSet>(seeds); }, kNames);
}

} // namespace CoW
