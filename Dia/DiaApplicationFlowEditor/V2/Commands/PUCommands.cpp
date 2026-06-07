#include <DiaApplicationFlowEditor/V2/Commands/PUCommands.h>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    // ==============================================================================
    // AddPUCommand
    // ==============================================================================

    AddPUCommand::AddPUCommand(Dia::Core::StringCRC instanceId, float frequencyHz, bool dedicatedThread)
    {
        mPU.instanceId      = instanceId;
        mPU.frequencyHz     = frequencyHz;
        mPU.dedicatedThread = dedicatedThread;
    }

    void AddPUCommand::Execute(ManifestEditorState& doc)
    {
        doc.manifest.processingUnits.Add(mPU);
        doc.MarkDirty();
    }

    void AddPUCommand::Undo(ManifestEditorState& doc)
    {
        auto& pus = doc.manifest.processingUnits;
        for (unsigned int i = 0; i < pus.Size(); ++i)
        {
            if (pus[i].instanceId == mPU.instanceId)
            {
                pus.RemoveAt(i);
                return;
            }
        }
    }

    // ==============================================================================
    // RemovePUCommand
    // ==============================================================================

    RemovePUCommand::RemovePUCommand(Dia::Core::StringCRC instanceId)
        : mInstanceId(instanceId)
        , mSavedIndex(-1)
    {}

    void RemovePUCommand::Execute(ManifestEditorState& doc)
    {
        auto& pus = doc.manifest.processingUnits;
        for (unsigned int i = 0; i < pus.Size(); ++i)
        {
            if (pus[i].instanceId == mInstanceId)
            {
                mSavedPU    = pus[i];
                mSavedIndex = static_cast<int>(i);
                pus.RemoveAt(i);
                doc.MarkDirty();
                return;
            }
        }
    }

    void RemovePUCommand::Undo(ManifestEditorState& doc)
    {
        if (mSavedIndex < 0)
            return;

        auto& pus = doc.manifest.processingUnits;
        unsigned int idx = static_cast<unsigned int>(mSavedIndex);
        if (idx >= pus.Size())
            pus.Add(mSavedPU);
        else
            pus.AddAt(mSavedPU, idx);
    }

    // ==============================================================================
    // SetPUFrequencyCommand
    // ==============================================================================

    SetPUFrequencyCommand::SetPUFrequencyCommand(Dia::Core::StringCRC instanceId, float newFrequency)
        : mInstanceId(instanceId)
        , mNewFrequency(newFrequency)
        , mOldFrequency(0.0f)
    {}

    void SetPUFrequencyCommand::Execute(ManifestEditorState& doc)
    {
        auto& pus = doc.manifest.processingUnits;
        for (unsigned int i = 0; i < pus.Size(); ++i)
        {
            if (pus[i].instanceId == mInstanceId)
            {
                mOldFrequency      = pus[i].frequencyHz;
                pus[i].frequencyHz = mNewFrequency;
                doc.MarkDirty();
                return;
            }
        }
    }

    void SetPUFrequencyCommand::Undo(ManifestEditorState& doc)
    {
        auto& pus = doc.manifest.processingUnits;
        for (unsigned int i = 0; i < pus.Size(); ++i)
        {
            if (pus[i].instanceId == mInstanceId)
            {
                pus[i].frequencyHz = mOldFrequency;
                return;
            }
        }
    }

    // ==============================================================================
    // SetPUThreadCommand
    // ==============================================================================

    SetPUThreadCommand::SetPUThreadCommand(Dia::Core::StringCRC instanceId, bool dedicatedThread)
        : mInstanceId(instanceId)
        , mNewValue(dedicatedThread)
        , mOldValue(false)
    {}

    void SetPUThreadCommand::Execute(ManifestEditorState& doc)
    {
        auto& pus = doc.manifest.processingUnits;
        for (unsigned int i = 0; i < pus.Size(); ++i)
        {
            if (pus[i].instanceId == mInstanceId)
            {
                mOldValue              = pus[i].dedicatedThread;
                pus[i].dedicatedThread = mNewValue;
                doc.MarkDirty();
                return;
            }
        }
    }

    void SetPUThreadCommand::Undo(ManifestEditorState& doc)
    {
        auto& pus = doc.manifest.processingUnits;
        for (unsigned int i = 0; i < pus.Size(); ++i)
        {
            if (pus[i].instanceId == mInstanceId)
            {
                pus[i].dedicatedThread = mOldValue;
                return;
            }
        }
    }

    // ==============================================================================
    // ReorderPUCommand
    // ==============================================================================

    ReorderPUCommand::ReorderPUCommand(Dia::Core::StringCRC instanceId, int newIndex)
        : mInstanceId(instanceId)
        , mNewIndex(newIndex)
        , mOldIndex(-1)
    {}

    void ReorderPUCommand::Execute(ManifestEditorState& doc)
    {
        auto& pus = doc.manifest.processingUnits;
        for (unsigned int i = 0; i < pus.Size(); ++i)
        {
            if (pus[i].instanceId == mInstanceId)
            {
                mOldIndex = static_cast<int>(i);
                if (mOldIndex == mNewIndex)
                    return;

                ProcessingUnitDeclaration saved = pus[i];
                pus.RemoveAt(i);
                unsigned int dest = static_cast<unsigned int>(mNewIndex);
                if (dest >= pus.Size())
                    pus.Add(saved);
                else
                    pus.AddAt(saved, dest);
                doc.MarkDirty();
                return;
            }
        }
    }

    void ReorderPUCommand::Undo(ManifestEditorState& doc)
    {
        if (mOldIndex < 0)
            return;

        auto& pus = doc.manifest.processingUnits;
        for (unsigned int i = 0; i < pus.Size(); ++i)
        {
            if (pus[i].instanceId == mInstanceId)
            {
                ProcessingUnitDeclaration saved = pus[i];
                pus.RemoveAt(i);
                unsigned int dest = static_cast<unsigned int>(mOldIndex);
                if (dest >= pus.Size())
                    pus.Add(saved);
                else
                    pus.AddAt(saved, dest);
                return;
            }
        }
    }

}}} // namespace Dia::ApplicationFlow::Editor
