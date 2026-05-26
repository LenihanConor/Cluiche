#pragma once
#include <DiaApplicationEditor/V2/Commands/ICommand.h>
#include <DiaApplicationEditor/V2/ManifestEditorState.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    class AddModuleCommand : public ICommand
    {
    public:
        AddModuleCommand(Dia::Core::StringCRC puId, Dia::Core::StringCRC instanceId, Dia::Core::StringCRC typeId);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "AddModule"; }
    private:
        Dia::Core::StringCRC mPUId;
        Dia::ApplicationFlow::ModuleDeclaration mModule;
    };

    class RemoveModuleCommand : public ICommand
    {
    public:
        RemoveModuleCommand(Dia::Core::StringCRC puId, Dia::Core::StringCRC instanceId);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "RemoveModule"; }
    private:
        Dia::Core::StringCRC mPUId;
        Dia::Core::StringCRC mInstanceId;
        Dia::ApplicationFlow::ModuleDeclaration mSavedModule;
        int mSavedIndex;
    };

    class AddModuleDepCommand : public ICommand
    {
    public:
        AddModuleDepCommand(Dia::Core::StringCRC puId, Dia::Core::StringCRC moduleId, Dia::Core::StringCRC depId);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "AddModuleDep"; }
    private:
        Dia::Core::StringCRC mPUId;
        Dia::Core::StringCRC mModuleId;
        Dia::Core::StringCRC mDepId;
    };

    class RemoveModuleDepCommand : public ICommand
    {
    public:
        RemoveModuleDepCommand(Dia::Core::StringCRC puId, Dia::Core::StringCRC moduleId, Dia::Core::StringCRC depId);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "RemoveModuleDep"; }
    private:
        Dia::Core::StringCRC mPUId;
        Dia::Core::StringCRC mModuleId;
        Dia::Core::StringCRC mDepId;
        int mSavedDepIndex;
    };

    class SetModuleStagesCommand : public ICommand
    {
    public:
        SetModuleStagesCommand(Dia::Core::StringCRC puId, Dia::Core::StringCRC moduleId,
                               const Dia::Core::StringCRC* newStages, unsigned int count);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "SetModuleStages"; }
    private:
        Dia::Core::StringCRC mPUId;
        Dia::Core::StringCRC mModuleId;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> mNewStages;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> mOldStages;
    };

    class SetModuleStartTimeoutCommand : public ICommand
    {
    public:
        SetModuleStartTimeoutCommand(Dia::Core::StringCRC puId, Dia::Core::StringCRC moduleId, float newTimeout);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "SetModuleStartTimeout"; }
    private:
        Dia::Core::StringCRC mPUId;
        Dia::Core::StringCRC mModuleId;
        float mNewTimeout;
        float mOldTimeout;
    };

    class SetModuleStopTimeoutCommand : public ICommand
    {
    public:
        SetModuleStopTimeoutCommand(Dia::Core::StringCRC puId, Dia::Core::StringCRC moduleId, float newTimeout);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "SetModuleStopTimeout"; }
    private:
        Dia::Core::StringCRC mPUId;
        Dia::Core::StringCRC mModuleId;
        float mNewTimeout;
        float mOldTimeout;
    };

    class AddModuleChannelCommand : public ICommand
    {
    public:
        AddModuleChannelCommand(Dia::Core::StringCRC puId, Dia::Core::StringCRC moduleId,
                                Dia::Core::StringCRC streamId, Dia::Core::StringCRC role);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "AddModuleChannel"; }
    private:
        Dia::Core::StringCRC mPUId;
        Dia::Core::StringCRC mModuleId;
        Dia::Core::StringCRC mStreamId;
        Dia::Core::StringCRC mRole;
    };

    class RemoveModuleChannelCommand : public ICommand
    {
    public:
        RemoveModuleChannelCommand(Dia::Core::StringCRC puId, Dia::Core::StringCRC moduleId,
                                   Dia::Core::StringCRC streamId, Dia::Core::StringCRC role);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "RemoveModuleChannel"; }
    private:
        Dia::Core::StringCRC mPUId;
        Dia::Core::StringCRC mModuleId;
        Dia::Core::StringCRC mStreamId;
        Dia::Core::StringCRC mRole;
        int mSavedIndex;
    };


}}} // namespace Dia::ApplicationFlow::Editor
