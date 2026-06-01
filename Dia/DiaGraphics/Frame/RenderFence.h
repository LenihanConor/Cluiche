#pragma once
#include <cstdint>

namespace Dia { namespace Graphics {

struct RenderFence
{
    uint64_t presentedFrame = 0;
};

}} // namespace Dia::Graphics
