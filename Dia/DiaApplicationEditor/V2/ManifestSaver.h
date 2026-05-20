#pragma once
#include <DiaApplicationEditor/V2/ManifestEditorState.h>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    enum class SaveStatus { Ok, WriteError, BackupError };

    struct SaveResult
    {
        SaveStatus status;
        char errorMessage[256];
    };

    class ManifestSaver
    {
    public:
        // Saves state.manifest to state.filePath.
        // Steps: create .bak, serialize to .tmp, rename to final path.
        // On success: clears state.isDirty.
        static SaveResult Save(ManifestEditorState& state);

        // Serializes the manifest to a JSON string (canonical formatting).
        // Used by both Save() and for unit testing the output format.
        static void SerializeToJson(const ManifestEditorState& state, char* outBuffer, unsigned int bufferSize);
    };

}}} // namespace Dia::ApplicationFlow::Editor
