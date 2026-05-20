#pragma once
#include <DiaApplicationEditor/V2/ManifestEditorState.h>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    enum class LoadStatus { Ok, MalformedJson, WrongVersion, LockedFile, SchemaError };

    struct LoadResult
    {
        LoadStatus status;
        char errorMessage[256];
    };

    class ManifestLoader
    {
    public:
        // Loads a .diaapp v2 JSON file into state.
        // On success: populates state.manifest, sets state.filePath, clears dirty and sets hasManifest=true.
        // On failure: state is unmodified. Returns appropriate LoadStatus.
        static LoadResult Load(const char* path, ManifestEditorState& state);
    };

}}} // namespace Dia::ApplicationFlow::Editor
