#include <DiaApplicationFlowEditor/V2/Commands/StageCommands.h>

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

        // Cascade rename through all stage transitions[] arrays
        for (unsigned int i = 0u; i < manifest.stages.Size(); ++i)
        {
            auto& transitions = manifest.stages[i].transitions;
            for (unsigned int t = 0u; t < transitions.Size(); ++t)
            {
                if (transitions[t] == mOldName)
                    transitions[t] = mNewName;
            }
        }

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

        // Undo: cascade rename back through all stage transitions[] arrays
        for (unsigned int i = 0u; i < manifest.stages.Size(); ++i)
        {
            auto& transitions = manifest.stages[i].transitions;
            for (unsigned int t = 0u; t < transitions.Size(); ++t)
            {
                if (transitions[t] == mNewName)
                    transitions[t] = mOldName;
            }
        }

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
        for (unsigned int i = 0u; i < doc.manifest.stages.Size(); ++i)
        {
            if (doc.manifest.stages[i].name == mName)
            {
                mOldIsAuto                       = doc.manifest.stages[i].autoAdvance;
                doc.manifest.stages[i].autoAdvance = mNewIsAuto;
                doc.MarkDirty();
                return;
            }
        }
    }

    void SetStageTriggerCommand::Undo(ManifestEditorState& doc)
    {
        for (unsigned int i = 0u; i < doc.manifest.stages.Size(); ++i)
        {
            if (doc.manifest.stages[i].name == mName)
            {
                doc.manifest.stages[i].autoAdvance = mOldIsAuto;
                doc.MarkDirty();
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // AddStageTransitionCommand
    // -------------------------------------------------------------------------

    AddStageTransitionCommand::AddStageTransitionCommand(
        Dia::Core::StringCRC stageName, Dia::Core::StringCRC targetName)
        : mStageName(stageName)
        , mTargetName(targetName)
    {
    }

    void AddStageTransitionCommand::Execute(ManifestEditorState& doc)
    {
        for (unsigned int i = 0u; i < doc.manifest.stages.Size(); ++i)
        {
            if (doc.manifest.stages[i].name == mStageName)
            {
                auto& t = doc.manifest.stages[i].transitions;
                // No-op if already present
                for (unsigned int j = 0u; j < t.Size(); ++j)
                    if (t[j] == mTargetName) return;
                t.Add(mTargetName);
                doc.MarkDirty();
                return;
            }
        }
    }

    void AddStageTransitionCommand::Undo(ManifestEditorState& doc)
    {
        for (unsigned int i = 0u; i < doc.manifest.stages.Size(); ++i)
        {
            if (doc.manifest.stages[i].name == mStageName)
            {
                auto& t = doc.manifest.stages[i].transitions;
                for (unsigned int j = 0u; j < t.Size(); ++j)
                {
                    if (t[j] == mTargetName)
                    {
                        t.RemoveAt(j);
                        doc.MarkDirty();
                        return;
                    }
                }
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // RemoveStageTransitionCommand
    // -------------------------------------------------------------------------

    RemoveStageTransitionCommand::RemoveStageTransitionCommand(
        Dia::Core::StringCRC stageName, Dia::Core::StringCRC targetName)
        : mStageName(stageName)
        , mTargetName(targetName)
    {
    }

    void RemoveStageTransitionCommand::Execute(ManifestEditorState& doc)
    {
        for (unsigned int i = 0u; i < doc.manifest.stages.Size(); ++i)
        {
            if (doc.manifest.stages[i].name == mStageName)
            {
                auto& t = doc.manifest.stages[i].transitions;
                for (unsigned int j = 0u; j < t.Size(); ++j)
                {
                    if (t[j] == mTargetName)
                    {
                        t.RemoveAt(j);
                        doc.MarkDirty();
                        return;
                    }
                }
                return;
            }
        }
    }

    void RemoveStageTransitionCommand::Undo(ManifestEditorState& doc)
    {
        for (unsigned int i = 0u; i < doc.manifest.stages.Size(); ++i)
        {
            if (doc.manifest.stages[i].name == mStageName)
            {
                auto& t = doc.manifest.stages[i].transitions;
                // Re-add (no-op if already present — idempotent undo)
                for (unsigned int j = 0u; j < t.Size(); ++j)
                    if (t[j] == mTargetName) return;
                t.Add(mTargetName);
                doc.MarkDirty();
                return;
            }
        }
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
