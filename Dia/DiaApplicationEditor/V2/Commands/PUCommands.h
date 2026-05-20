#pragma once
#include <DiaApplicationEditor/V2/Commands/ICommand.h>
#include <DiaApplicationEditor/V2/ManifestEditorState.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV2.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    class AddPUCommand : public ICommand
    {
    public:
        explicit AddPUCommand(Dia::Core::StringCRC instanceId, float frequencyHz = 30.0f, bool dedicatedThread = false);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "AddPU"; }
    private:
        Dia::ApplicationFlow::ProcessingUnitDeclaration mPU;
    };

    class RemovePUCommand : public ICommand
    {
    public:
        explicit RemovePUCommand(Dia::Core::StringCRC instanceId);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "RemovePU"; }
    private:
        Dia::Core::StringCRC mInstanceId;
        Dia::ApplicationFlow::ProcessingUnitDeclaration mSavedPU;
        int mSavedIndex;
    };

    class SetPUFrequencyCommand : public ICommand
    {
    public:
        SetPUFrequencyCommand(Dia::Core::StringCRC instanceId, float newFrequency);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "SetPUFrequency"; }
    private:
        Dia::Core::StringCRC mInstanceId;
        float mNewFrequency;
        float mOldFrequency;
    };

    class SetPUThreadCommand : public ICommand
    {
    public:
        SetPUThreadCommand(Dia::Core::StringCRC instanceId, bool dedicatedThread);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "SetPUThread"; }
    private:
        Dia::Core::StringCRC mInstanceId;
        bool mNewValue;
        bool mOldValue;
    };

    class ReorderPUCommand : public ICommand
    {
    public:
        ReorderPUCommand(Dia::Core::StringCRC instanceId, int newIndex);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "ReorderPU"; }
    private:
        Dia::Core::StringCRC mInstanceId;
        int mNewIndex;
        int mOldIndex;
    };

}}} // namespace Dia::ApplicationFlow::Editor
