#include "DiaApplicationEditor/EditorHealthReporter.h"
#include <DiaApplicationEditor/V2/ManifestEditorState.h>

namespace Dia { namespace Editor {

EditorHealthReporter::EditorHealthReporter(
    const Dia::ApplicationFlow::Editor::ManifestEditorState& state,
    const bool& isLiveConnected)
    : mState(state)
    , mIsLiveConnected(isLiveConnected)
{}

Dia::Core::StringCRC EditorHealthReporter::GetReporterName() const
{
    return Dia::Core::StringCRC("DiaApplicationFlowEditor");
}

Dia::Observation::Health::Health EditorHealthReporter::Report() const
{
    using namespace Dia::Observation::Health;

    if (!mState.hasManifest)
    {
        return Health{ HealthStatus::kDegraded, 0u, 1u, Dia::Core::StringCRC("NoManifestLoaded") };
    }

    return Health{ HealthStatus::kOK, 0u, 0u, Dia::Core::StringCRC("") };
}

}} // namespace Dia::Editor
