// F2StreamIndependence: check C1. Draws a fixed interleave across the 8 RNG streams and compares
// each stream's sequence with the same stream drawn alone from an identical seed. A divergence
// means streams share state. Spec: docs/specs/applications/cow/systems/f2-core-runtime/f2-core-runtime.md
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace Dia { namespace Maths { class RandomStream; } }

namespace CoW {

constexpr int kF2IndependenceStreamCount = 8;
constexpr int kF2IndependenceInterleaveLength = 64;

// A fresh, independently seeded set of 8 streams. Abstract so tests can inject a leaky set.
class F2IStreamSet
{
public:
    virtual ~F2IStreamSet() = default;
    virtual uint32_t Draw(int streamIndex) = 0;
};

using F2StreamSetFactory = std::function<std::unique_ptr<F2IStreamSet>()>;

struct F2StreamIndependenceResult
{
    bool        passed = false;
    int         independentCount = 0;       // streams whose interleaved sequence matched the baseline
    int         firstDivergingStream = -1;  // index into names, -1 if none
    int         firstDivergingDraw = -1;    // per-stream draw index of first mismatch
    std::string summary;                    // "8 / 8 streams independent over 64 draws" or the failure
};

// Core check: builds fresh stream sets from the factory (one interleaved, one per baseline stream).
F2StreamIndependenceResult CheckStreamIndependence(const F2StreamSetFactory& factory,
                                                   const char* const (&names)[kF2IndependenceStreamCount]);

// Live overload: builds fresh RandomStreams from the live streams' seeds. Live streams are only
// read (GetSeed), never drawn from or restored, so their state is untouched.
F2StreamIndependenceResult CheckStreamIndependence(Dia::Maths::RandomStream* const (&live)[kF2IndependenceStreamCount]);

} // namespace CoW
