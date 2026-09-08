#pragma once

#include <DiaApplicationFlowEditor/V2/Commands/ICommand.h>
#include <DiaApplicationFlowEditor/V2/ManifestEditorState.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String256.h>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    class AddStageCommand : public ICommand
    {
    public:
        explicit AddStageCommand(Dia::Core::StringCRC name, const char* manifestPath = "");
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "AddStage"; }
    private:
        Dia::ApplicationFlow::StageDeclaration mStage;
    };

    class RemoveStageCommand : public ICommand
    {
    public:
        explicit RemoveStageCommand(Dia::Core::StringCRC name);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "RemoveStage"; }
    private:
        Dia::Core::StringCRC mName;
        Dia::ApplicationFlow::StageDeclaration mSaved;
        int mSavedIndex;
        bool mWasInitialStage;
    };

    class RenameStageCommand : public ICommand
    {
    public:
        RenameStageCommand(Dia::Core::StringCRC oldName, Dia::Core::StringCRC newName);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "RenameStage"; }
    private:
        Dia::Core::StringCRC mOldName;
        Dia::Core::StringCRC mNewName;
    };

    class SetStageTriggerCommand : public ICommand
    {
    public:
        SetStageTriggerCommand(Dia::Core::StringCRC name, bool isAuto);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "SetStageTrigger"; }
    private:
        Dia::Core::StringCRC mName;
        bool mNewIsAuto;
        bool mOldIsAuto;
    };

    class AddStageTransitionCommand : public ICommand
    {
    public:
        AddStageTransitionCommand(Dia::Core::StringCRC stageName, Dia::Core::StringCRC targetName);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "AddStageTransition"; }
    private:
        Dia::Core::StringCRC mStageName;
        Dia::Core::StringCRC mTargetName;
    };

    class RemoveStageTransitionCommand : public ICommand
    {
    public:
        RemoveStageTransitionCommand(Dia::Core::StringCRC stageName, Dia::Core::StringCRC targetName);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "RemoveStageTransition"; }
    private:
        Dia::Core::StringCRC mStageName;
        Dia::Core::StringCRC mTargetName;
    };

    class SetInitialStageCommand : public ICommand
    {
    public:
        explicit SetInitialStageCommand(Dia::Core::StringCRC newInitial);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "SetInitialStage"; }
    private:
        Dia::Core::StringCRC mNew;
        Dia::Core::StringCRC mOld;
    };

    class ReorderStageCommand : public ICommand
    {
    public:
        ReorderStageCommand(Dia::Core::StringCRC name, int newIndex);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "ReorderStage"; }
    private:
        Dia::Core::StringCRC mName;
        int mNewIndex;
        int mOldIndex;
    };

}}} // namespace Dia::ApplicationFlow::Editor
