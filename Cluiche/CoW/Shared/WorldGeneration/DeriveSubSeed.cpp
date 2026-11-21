#include "DeriveSubSeed.h"

namespace CoW {
namespace WorldGeneration {

uint64_t DeriveSubSeed(uint64_t parent, uint64_t key)
{
    uint64_t z = parent ^ (key * 0x9E3779B97F4A7C15ull);
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

uint64_t DeriveSubSeed(uint64_t parent, Dia::Core::StringCRC step)
{
    return DeriveSubSeed(parent, static_cast<uint64_t>(step.Value()));
}

} // namespace WorldGeneration
} // namespace CoW
