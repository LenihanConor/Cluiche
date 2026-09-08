////////////////////////////////////////////////////////////////////////////////
// Filename: PickTrigger.h
// Description: Enum identifying what caused a pick query.
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia::Picking {

enum class PickTrigger : unsigned int
{
    kClick       = 0,
    kHover       = 1,
    kRightClick  = 2,
    kAreaSelect  = 3,
    kCount       = 4
};

} // namespace Dia::Picking
