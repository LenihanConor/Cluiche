#include <DiaApplicationEditor/V2/Commands/StageCommands.h>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    // -------------------------------------------------------------------------
    // AddStageCommand
    // -------------------------------------------------------------------------

    AddStageCommand::AddStageCommand(Dia::Core::StringCRC name, const char* manifestPath)
    {
        mStage.name         = name;
        mStage.manifestPath = manifestPath;
    }

    void AddStageCommand::Execute(ManifestEditorState& doc)
    {
        doc.manifest.stages.Add(mStage);
        doc.MarkDirty();
    }

    void AddStageCommand::Undo(ManifestEditorState& doc)
    {
        auto& stages = doc.manifest.stages;
        for (unsigned int i = 0u; i < stages.Size(); ++i)
        {
            if (stages[i].name == mStage.name)
            {
                stages.RemoveAt(i);
                doc.MarkDirty();
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // RemoveStageCommand
    // -------------------------------------------------------------------------

    RemoveStageCommand::RemoveStageCommand(Dia::Core::StringCRC name)
        : mName(name)
        , mSavedIndex(-1)
        , mWasInitialStage(false)
        , mWasAutoStage(false)
    {
    }

    void RemoveStageCommand::Execute(ManifestEditorState& doc)
    {
        auto& manifest = doc.manifest;
        auto& stages   = manifest.stages;

        for (unsigned int i = 0u; i < stages.Size(); ++i)
        {
            if (stages[i].name == mName)
            {
                mSaved      = stages[i];
                mSavedIndex = static_cast<int>(i);
                stages.RemoveAt(i);
                break;
            }
        }

        if (mSavedIndex < 0)
        {
            return;
        }

        // TODO: autoStages removed in v3 — autoAdvance now lives on StageDeclaration
        // mWasAutoStage tracking removed

        // Check if it was the initial stage
        if (manifest.initialStage == mName)
        {
            manifest.initialStage = Dia::Core::StringCRC();
            mWasInitialStage      = true;
        }

        doc.MarkDirty();
    }

    void RemoveStageCommand::Undo(ManifestEditorState& doc)
    {
        if (mSavedIndex < 0)
        {
            return;
        }

        auto& manifest = doc.manifest;
        auto& stages   = manifest.stages;

        unsigned int insertAt = static_cast<unsigned int>(mSavedIndex);
        if (insertAt >= stages.Size())
        {
            stages.Add(mSaved);
        }
        else
        {
            stages.AddAt(mSaved, insertAt);
        }

        // TODO: autoStages removed in v3 — mWasAutoStage no longer applies

        if (mWasInitialStage)
        {
            manifest.initialStage = mName;
        }

        doc.MarkDirty();
    }

    // -------------------------------------------------------------------------
    // RenameStageCommand
    // -------------------------------------------------------------------------

    RenameStageCommand::RenameStageCommand(Dia::Core::StringCRC oldName, Dia::Core::StringCRC newName)
        : mOldName(oldName)
        , mNewName(newName)
    {
    }

    void RenameStageCommand::Execute(ManifestEditorState& doc)
    {
        auto& manifest = doc.manifest;

        // Rename in stages array
        for (unsigned int i = 0u; i < manifest.stages.Size(); ++i)
        {
            if (manifest.stages[i].name == mOldName)
            {
                manifest.stages[i].name = mNewName;
                break;
            }
        }

        // TODO: autoStages removed in v3 — autoAdvance rename handled in task 2

        // Update initialStage if it references oldName
        if (manifest.initialStage == mOldName)
        {
            manifest.initialStage = mNewName;
        }

        // Update module stage references across all processing units
        for (unsigned int pu = 0u; pu < manifest.processingUnits.Size(); ++pu)
        {
            auto& modules = manifest.processingUnits[pu].modules;
            for (unsigned int m = 0u; m < modules.Size(); ++m)
            {
                auto& stageRefs = modules[m].stages;
                for (unsigned int s = 0u; s < stageRefs.Size(); ++s)
                {
                    if (stageRefs[s] == mOldName)
                    {
                        stageRefs[s] = mNewName;
                    }
                }
            }
        }

        doc.MarkDirty();
    }

    void RenameStageCommand::Undo(ManifestEditorState& doc)
    {
        auto& manifest = doc.manifest;

        for (unsigned int i = 0u; i < manifest.stages.Size(); ++i)
        {
            if (manifest.stages[i].name == mNewName)
            {
                manifest.stages[i].name = mOldName;
                break;
            }
        }

        // TODO: autoStages removed in v3 — autoAdvance undo handled in task 2

        if (manifest.initialStage == mNewName)
        {
            manifest.initialStage = mOldName;
        }

        for (unsigned int pu = 0u; pu < manifest.processingUnits.Size(); ++pu)
        {
            auto& modules = manifest.processingUnits[pu].modules;
            for (unsigned int m = 0u; m < modules.Size(); ++m)
            {
                auto& stageRefs = modules[m].stages;
                for (unsigned int s = 0u; s < stageRefs.Size(); ++s)
                {
                    if (stageRefs[s] == mNewName)
                    {
                        stageRefs[s] = mOldName;
                    }
                }
            }
        }

        doc.MarkDirty();
    }

    // -------------------------------------------------------------------------
    // SetStageTriggerCommand
    // -------------------------------------------------------------------------

    SetStageTriggerCommand::SetStageTriggerCommand(Dia::Core::StringCRC name, bool isAuto)
        : mName(name)
        , mNewIsAuto(isAuto)
        , mOldIsAuto(false)
    {
    }

    void SetStageTriggerCommand::Execute(ManifestEditorState& doc)
    {
        // TODO: replaced in task 6 — SetStageTriggerCommand now sets StageDeclaration::autoAdvance
        (void)doc;
    }

    void SetStageTriggerCommand::Undo(ManifestEditorState& doc)
    {
        // TODO: replaced in task 6 — SetStageTriggerCommand now sets StageDeclaration::autoAdvance
        (void)doc;
    }

    // -------------------------------------------------------------------------
    // SetInitialStageCommand
    // -------------------------------------------------------------------------

    SetInitialStageCommand::SetInitialStageCommand(Dia::Core::StringCRC newInitial)
        : mNew(newInitial)
    {
    }

    void SetInitialStageCommand::Execute(ManifestEditorState& doc)
    {
        mOld                    = doc.manifest.initialStage;
        doc.manifest.initialStage = mNew;
        doc.MarkDirty();
    }

    void SetInitialStageCommand::Undo(ManifestEditorState& doc)
    {
        doc.manifest.initialStage = mOld;
        doc.MarkDirty();
    }

    // -------------------------------------------------------------------------
    // ReorderStageCommand
    // -------------------------------------------------------------------------

    ReorderStageCommand::ReorderStageCommand(Dia::Core::StringCRC name, int newIndex)
        : mName(name)
        , mNewIndex(newIndex)
        , mOldIndex(-1)
    {
    }

    void ReorderStageCommand::Execute(ManifestEditorState& doc)
    {
        auto& stages = doc.manifest.stages;
        const unsigned int size = stages.Size();

        // Find current index
        int currentIndex = -1;
        for (unsigned int i = 0u; i < size; ++i)
        {
            if (stages[i].name == mName)
            {
                currentIndex = static_cast<int>(i);
                break;
            }
        }

        if (currentIndex < 0)
        {
            return;
        }

        mOldIndex = currentIndex;

        int targetIndex = mNewIndex;
        if (targetIndex < 0)
        {
            targetIndex = 0;
        }
        else if (targetIndex >= static_cast<int>(size))
        {
            targetIndex = static_cast<int>(size) - 1;
        }

        if (targetIndex == currentIndex)
        {
            return;
        }

        Dia::ApplicationFlow::StageDeclaration entry = stages[static_cast<unsigned int>(currentIndex)];
        stages.RemoveAt(static_cast<unsigned int>(currentIndex));
        stages.AddAt(entry, static_cast<unsigned int>(targetIndex));

        doc.MarkDirty();
    }

    void ReorderStageCommand::Undo(ManifestEditorState& doc)
    {
        if (mOldIndex < 0)
        {
            return;
        }

        auto& stages = doc.manifest.stages;
        const unsigned int size = stages.Size();

        int currentIndex = -1;
        for (unsigned int i = 0u; i < size; ++i)
        {
            if (stages[i].name == mName)
            {
                currentIndex = static_cast<int>(i);
                break;
            }
        }

        if (currentIndex < 0)
        {
            return;
        }

        int targetIndex = mOldIndex;
        if (targetIndex >= static_cast<int>(size))
        {
            targetIndex = static_cast<int>(size) - 1;
        }

        if (targetIndex == currentIndex)
        {
            return;
        }

        Dia::ApplicationFlow::StageDeclaration entry = stages[static_cast<unsigned int>(currentIndex)];
        stages.RemoveAt(static_cast<unsigned int>(currentIndex));
        stages.AddAt(entry, static_cast<unsigned int>(targetIndex));

        doc.MarkDirty();
    }

}}} // namespace Dia::ApplicationFlow::Editor
