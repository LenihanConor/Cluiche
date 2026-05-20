#include <DiaApplicationEditor/V2/Commands/ModuleCommands.h>

namespace
{
    Dia::ApplicationFlow::ProcessingUnitDeclaration* FindPU(
        Dia::ApplicationFlow::ApplicationManifestV2& manifest,
        Dia::Core::StringCRC puId)
    {
        auto& pus = manifest.processingUnits;
        for (unsigned int i = 0; i < pus.Size(); ++i)
        {
            if (pus[i].instanceId == puId)
                return &pus[i];
        }
        return nullptr;
    }

    Dia::ApplicationFlow::ModuleDeclaration* FindModule(
        Dia::ApplicationFlow::ProcessingUnitDeclaration& pu,
        Dia::Core::StringCRC moduleId)
    {
        auto& modules = pu.modules;
        for (unsigned int i = 0; i < modules.Size(); ++i)
        {
            if (modules[i].instanceId == moduleId)
                return &modules[i];
        }
        return nullptr;
    }
}

namespace Dia { namespace ApplicationFlow { namespace Editor {

    // ==============================================================================
    // AddModuleCommand
    // ==============================================================================

    AddModuleCommand::AddModuleCommand(Dia::Core::StringCRC puId,
                                       Dia::Core::StringCRC instanceId,
                                       Dia::Core::StringCRC typeId)
        : mPUId(puId)
    {
        mModule.instanceId = instanceId;
        mModule.typeId     = typeId;
    }

    void AddModuleCommand::Execute(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu)
            return;
        pu->modules.Add(mModule);
        doc.MarkDirty();
    }

    void AddModuleCommand::Undo(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu)
            return;
        auto& modules = pu->modules;
        for (unsigned int i = 0; i < modules.Size(); ++i)
        {
            if (modules[i].instanceId == mModule.instanceId)
            {
                modules.RemoveAt(i);
                return;
            }
        }
    }

    // ==============================================================================
    // RemoveModuleCommand
    // ==============================================================================

    RemoveModuleCommand::RemoveModuleCommand(Dia::Core::StringCRC puId,
                                             Dia::Core::StringCRC instanceId)
        : mPUId(puId)
        , mInstanceId(instanceId)
        , mSavedIndex(-1)
    {}

    void RemoveModuleCommand::Execute(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu)
            return;
        auto& modules = pu->modules;
        for (unsigned int i = 0; i < modules.Size(); ++i)
        {
            if (modules[i].instanceId == mInstanceId)
            {
                mSavedModule = modules[i];
                mSavedIndex  = static_cast<int>(i);
                modules.RemoveAt(i);
                doc.MarkDirty();
                return;
            }
        }
    }

    void RemoveModuleCommand::Undo(ManifestEditorState& doc)
    {
        if (mSavedIndex < 0)
            return;
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu)
            return;
        auto& modules = pu->modules;
        unsigned int idx = static_cast<unsigned int>(mSavedIndex);
        if (idx >= modules.Size())
            modules.Add(mSavedModule);
        else
            modules.AddAt(mSavedModule, idx);
    }

    // ==============================================================================
    // AddModuleDepCommand
    // ==============================================================================

    AddModuleDepCommand::AddModuleDepCommand(Dia::Core::StringCRC puId,
                                             Dia::Core::StringCRC moduleId,
                                             Dia::Core::StringCRC depId)
        : mPUId(puId)
        , mModuleId(moduleId)
        , mDepId(depId)
    {}

    void AddModuleDepCommand::Execute(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu)
            return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod)
            return;
        mod->dependencies.Add(mDepId);
        doc.MarkDirty();
    }

    void AddModuleDepCommand::Undo(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu)
            return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod)
            return;
        auto& deps = mod->dependencies;
        for (unsigned int i = 0; i < deps.Size(); ++i)
        {
            if (deps[i] == mDepId)
            {
                deps.RemoveAt(i);
                return;
            }
        }
    }

    // ==============================================================================
    // RemoveModuleDepCommand
    // ==============================================================================

    RemoveModuleDepCommand::RemoveModuleDepCommand(Dia::Core::StringCRC puId,
                                                   Dia::Core::StringCRC moduleId,
                                                   Dia::Core::StringCRC depId)
        : mPUId(puId)
        , mModuleId(moduleId)
        , mDepId(depId)
        , mSavedDepIndex(-1)
    {}

    void RemoveModuleDepCommand::Execute(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu)
            return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod)
            return;
        auto& deps = mod->dependencies;
        for (unsigned int i = 0; i < deps.Size(); ++i)
        {
            if (deps[i] == mDepId)
            {
                mSavedDepIndex = static_cast<int>(i);
                deps.RemoveAt(i);
                doc.MarkDirty();
                return;
            }
        }
    }

    void RemoveModuleDepCommand::Undo(ManifestEditorState& doc)
    {
        if (mSavedDepIndex < 0)
            return;
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu)
            return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod)
            return;
        auto& deps = mod->dependencies;
        unsigned int idx = static_cast<unsigned int>(mSavedDepIndex);
        if (idx >= deps.Size())
            deps.Add(mDepId);
        else
            deps.AddAt(mDepId, idx);
    }

    // ==============================================================================
    // SetModuleStagesCommand
    // ==============================================================================

    SetModuleStagesCommand::SetModuleStagesCommand(Dia::Core::StringCRC puId,
                                                   Dia::Core::StringCRC moduleId,
                                                   const Dia::Core::StringCRC* newStages,
                                                   unsigned int count)
        : mPUId(puId)
        , mModuleId(moduleId)
    {
        for (unsigned int i = 0; i < count; ++i)
            mNewStages.Add(newStages[i]);
    }

    void SetModuleStagesCommand::Execute(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu)
            return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod)
            return;

        mOldStages.RemoveAll();
        auto& stages = mod->stages;
        for (unsigned int i = 0; i < stages.Size(); ++i)
            mOldStages.Add(stages[i]);

        stages.RemoveAll();
        for (unsigned int i = 0; i < mNewStages.Size(); ++i)
            stages.Add(mNewStages[i]);

        doc.MarkDirty();
    }

    void SetModuleStagesCommand::Undo(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu)
            return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod)
            return;

        auto& stages = mod->stages;
        stages.RemoveAll();
        for (unsigned int i = 0; i < mOldStages.Size(); ++i)
            stages.Add(mOldStages[i]);
    }

    // ==============================================================================
    // SetModuleStartTimeoutCommand
    // ==============================================================================

    SetModuleStartTimeoutCommand::SetModuleStartTimeoutCommand(Dia::Core::StringCRC puId,
                                                               Dia::Core::StringCRC moduleId,
                                                               float newTimeout)
        : mPUId(puId)
        , mModuleId(moduleId)
        , mNewTimeout(newTimeout)
        , mOldTimeout(0.0f)
    {}

    void SetModuleStartTimeoutCommand::Execute(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu)
            return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod)
            return;
        mOldTimeout           = mod->startTimeoutMs;
        mod->startTimeoutMs   = mNewTimeout;
        doc.MarkDirty();
    }

    void SetModuleStartTimeoutCommand::Undo(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu)
            return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod)
            return;
        mod->startTimeoutMs = mOldTimeout;
    }

    // ==============================================================================
    // SetModuleStopTimeoutCommand
    // ==============================================================================

    SetModuleStopTimeoutCommand::SetModuleStopTimeoutCommand(Dia::Core::StringCRC puId,
                                                             Dia::Core::StringCRC moduleId,
                                                             float newTimeout)
        : mPUId(puId)
        , mModuleId(moduleId)
        , mNewTimeout(newTimeout)
        , mOldTimeout(0.0f)
    {}

    void SetModuleStopTimeoutCommand::Execute(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu)
            return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod)
            return;
        mOldTimeout         = mod->stopTimeoutMs;
        mod->stopTimeoutMs  = mNewTimeout;
        doc.MarkDirty();
    }

    void SetModuleStopTimeoutCommand::Undo(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu)
            return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod)
            return;
        mod->stopTimeoutMs = mOldTimeout;
    }

    // ==============================================================================
    // AddModuleReadCommand / RemoveModuleReadCommand
    // ==============================================================================

    AddModuleReadCommand::AddModuleReadCommand(Dia::Core::StringCRC puId,
                                               Dia::Core::StringCRC moduleId,
                                               Dia::Core::StringCRC streamId)
        : mPUId(puId), mModuleId(moduleId), mStreamId(streamId) {}

    void AddModuleReadCommand::Execute(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu) return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod) return;
        mod->reads.Add(mStreamId);
        doc.MarkDirty();
    }

    void AddModuleReadCommand::Undo(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu) return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod) return;
        auto& reads = mod->reads;
        for (unsigned int i = 0; i < reads.Size(); ++i)
        {
            if (reads[i] == mStreamId) { reads.RemoveAt(i); return; }
        }
    }

    RemoveModuleReadCommand::RemoveModuleReadCommand(Dia::Core::StringCRC puId,
                                                     Dia::Core::StringCRC moduleId,
                                                     Dia::Core::StringCRC streamId)
        : mPUId(puId), mModuleId(moduleId), mStreamId(streamId), mSavedIndex(-1) {}

    void RemoveModuleReadCommand::Execute(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu) return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod) return;
        auto& reads = mod->reads;
        for (unsigned int i = 0; i < reads.Size(); ++i)
        {
            if (reads[i] == mStreamId)
            {
                mSavedIndex = static_cast<int>(i);
                reads.RemoveAt(i);
                doc.MarkDirty();
                return;
            }
        }
    }

    void RemoveModuleReadCommand::Undo(ManifestEditorState& doc)
    {
        if (mSavedIndex < 0) return;
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu) return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod) return;
        auto& reads = mod->reads;
        unsigned int idx = static_cast<unsigned int>(mSavedIndex);
        if (idx >= reads.Size()) reads.Add(mStreamId);
        else                     reads.AddAt(mStreamId, idx);
    }

    // ==============================================================================
    // AddModuleWriteCommand / RemoveModuleWriteCommand
    // ==============================================================================

    AddModuleWriteCommand::AddModuleWriteCommand(Dia::Core::StringCRC puId,
                                                 Dia::Core::StringCRC moduleId,
                                                 Dia::Core::StringCRC streamId)
        : mPUId(puId), mModuleId(moduleId), mStreamId(streamId) {}

    void AddModuleWriteCommand::Execute(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu) return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod) return;
        mod->writes.Add(mStreamId);
        doc.MarkDirty();
    }

    void AddModuleWriteCommand::Undo(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu) return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod) return;
        auto& writes = mod->writes;
        for (unsigned int i = 0; i < writes.Size(); ++i)
        {
            if (writes[i] == mStreamId) { writes.RemoveAt(i); return; }
        }
    }

    RemoveModuleWriteCommand::RemoveModuleWriteCommand(Dia::Core::StringCRC puId,
                                                       Dia::Core::StringCRC moduleId,
                                                       Dia::Core::StringCRC streamId)
        : mPUId(puId), mModuleId(moduleId), mStreamId(streamId), mSavedIndex(-1) {}

    void RemoveModuleWriteCommand::Execute(ManifestEditorState& doc)
    {
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu) return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod) return;
        auto& writes = mod->writes;
        for (unsigned int i = 0; i < writes.Size(); ++i)
        {
            if (writes[i] == mStreamId)
            {
                mSavedIndex = static_cast<int>(i);
                writes.RemoveAt(i);
                doc.MarkDirty();
                return;
            }
        }
    }

    void RemoveModuleWriteCommand::Undo(ManifestEditorState& doc)
    {
        if (mSavedIndex < 0) return;
        ProcessingUnitDeclaration* pu = FindPU(doc.manifest, mPUId);
        if (!pu) return;
        ModuleDeclaration* mod = FindModule(*pu, mModuleId);
        if (!mod) return;
        auto& writes = mod->writes;
        unsigned int idx = static_cast<unsigned int>(mSavedIndex);
        if (idx >= writes.Size()) writes.Add(mStreamId);
        else                      writes.AddAt(mStreamId, idx);
    }

}}} // namespace Dia::ApplicationFlow::Editor
