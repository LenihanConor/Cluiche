#pragma once

#include <DiaObservation/Health/IHealthReporter.h>

// Forward declarations
namespace Dia { namespace ApplicationFlow { namespace Editor { struct ManifestEditorState; } } }

namespace Dia { namespace Editor {

class EditorHealthReporter : public Dia::Observation::Health::IHealthReporter
{
public:
    EditorHealthReporter(const Dia::ApplicationFlow::Editor::ManifestEditorState& state,
                         const bool& isLiveConnected);

    Dia::Core::StringCRC             GetReporterName() const override;
    Dia::Observation::Health::Health Report()          const override;

private:
    const Dia::ApplicationFlow::Editor::ManifestEditorState& mState;
    const bool& mIsLiveConnected;
};

}} // namespace Dia::Editor
