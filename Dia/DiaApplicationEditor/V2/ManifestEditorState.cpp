#include <DiaApplicationEditor/V2/ManifestEditorState.h>
#include <DiaCore/Core/Assert.h>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    ManifestEditorState::ManifestEditorState()
        : isDirty(false)
        , hasManifest(false)
    {
        filePath[0] = '\0';
    }

    const ModuleProvenance* ManifestEditorState::FindProvenance(
        Dia::Core::StringCRC puId,
        Dia::Core::StringCRC moduleId) const
    {
        for (unsigned int i = 0; i < provenance.Size(); ++i)
        {
            const ModuleProvenance& entry = provenance[i];
            if (entry.puId == puId && entry.moduleId == moduleId)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    void ManifestEditorState::SetProvenance(
        Dia::Core::StringCRC puId,
        Dia::Core::StringCRC moduleId,
        const char* sourceFile)
    {
        // Update existing entry if one already exists for this (puId, moduleId) pair
        for (unsigned int i = 0; i < provenance.Size(); ++i)
        {
            ModuleProvenance& entry = provenance[i];
            if (entry.puId == puId && entry.moduleId == moduleId)
            {
                entry.sourceFile = sourceFile;
                return;
            }
        }

        // No existing entry — add a new one
        DIA_ASSERT(!provenance.IsFull(), "ManifestEditorState: provenance array is full, cannot add new entry");

        if (!provenance.IsFull())
        {
            ModuleProvenance newEntry;
            newEntry.puId     = puId;
            newEntry.moduleId = moduleId;
            newEntry.sourceFile = sourceFile;
            provenance.Add(newEntry);
        }
    }

}}} // namespace Dia::ApplicationFlow::Editor
