#pragma once
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String256.h>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    // Tracks which source file a module definition came from.
    // Empty sourceFile means it came from the base .diaapp (not a .diastage import).
    struct ModuleProvenance
    {
        Dia::Core::StringCRC puId;
        Dia::Core::StringCRC moduleId;
        Dia::Core::Containers::String256 sourceFile;  // e.g. "dummy_stage.diaapp"
    };

    // Editor's in-memory representation of an open .diaapp v2 file.
    // Wraps ApplicationManifestV3 (structural model) with editor-specific metadata.
    struct ManifestEditorState
    {
        static constexpr unsigned int kMaxProvenance = 64;

        Dia::ApplicationFlow::ApplicationManifestV3 manifest;

        char filePath[512];   // absolute path; empty if no file loaded
        bool isDirty;
        bool hasManifest;

        Dia::Core::Containers::DynamicArrayC<ModuleProvenance, kMaxProvenance> provenance;

        ManifestEditorState();

        // Returns provenance entry for the given (puId, moduleId), or nullptr if none.
        const ModuleProvenance* FindProvenance(Dia::Core::StringCRC puId,
                                               Dia::Core::StringCRC moduleId) const;

        // Sets or updates provenance for a module.
        void SetProvenance(Dia::Core::StringCRC puId,
                           Dia::Core::StringCRC moduleId,
                           const char* sourceFile);

        void MarkDirty()  { isDirty = true; }
        void MarkClean()  { isDirty = false; }
    };

}}} // namespace Dia::ApplicationFlow::Editor
