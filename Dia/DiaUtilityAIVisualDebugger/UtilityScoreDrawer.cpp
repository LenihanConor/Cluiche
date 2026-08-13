////////////////////////////////////////////////////////////////////////////////
// Filename: UtilityScoreDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaUtilityAIVisualDebugger/UtilityScoreDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaUtilityAI/UtilitySet.h>

namespace Dia::UtilityAI
{

UtilityScoreDrawer::UtilityScoreDrawer(const UtilitySet& utilitySet)
    : mUtilitySet(utilitySet)
{}

Dia::Core::StringCRC UtilityScoreDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kUtilityAIScores;
}

void UtilityScoreDrawer::Draw(Dia::Core::IDebugDraw& /*draw*/)
{
    // No world-space representation.
}

} // namespace Dia::UtilityAI

#endif // DIA_DEBUG
