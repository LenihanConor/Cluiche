#pragma once

#include <DiaApplicationEditor/V2/Commands/ICommand.h>
#include <DiaApplicationEditor/V2/ManifestEditorState.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV2.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    class AddStreamCommand : public ICommand
    {
    public:
        AddStreamCommand(const char* idStr, Dia::Core::StringCRC kind, Dia::Core::StringCRC payloadType);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "AddStream"; }
    private:
        Dia::ApplicationFlow::StreamDeclaration mStream;
        bool mIsReserved;
    };

    class RemoveStreamCommand : public ICommand
    {
    public:
        explicit RemoveStreamCommand(Dia::Core::StringCRC id);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "RemoveStream"; }
    private:
        Dia::Core::StringCRC mId;
        Dia::ApplicationFlow::StreamDeclaration mSaved;
        int mSavedIndex;
    };

    class SetStreamKindCommand : public ICommand
    {
    public:
        SetStreamKindCommand(Dia::Core::StringCRC id, Dia::Core::StringCRC newKind);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "SetStreamKind"; }
    private:
        Dia::Core::StringCRC mId;
        Dia::Core::StringCRC mNewKind;
        Dia::Core::StringCRC mOldKind;
    };

    class SetStreamPayloadCommand : public ICommand
    {
    public:
        SetStreamPayloadCommand(Dia::Core::StringCRC id, Dia::Core::StringCRC newPayload);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "SetStreamPayload"; }
    private:
        Dia::Core::StringCRC mId;
        Dia::Core::StringCRC mNewPayload;
        Dia::Core::StringCRC mOldPayload;
    };

    class SetStreamFromPUCommand : public ICommand
    {
    public:
        SetStreamFromPUCommand(Dia::Core::StringCRC id, Dia::Core::StringCRC newFromPU);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "SetStreamFromPU"; }
    private:
        Dia::Core::StringCRC mId;
        Dia::Core::StringCRC mNew;
        Dia::Core::StringCRC mOld;
    };

    class SetStreamToPUCommand : public ICommand
    {
    public:
        SetStreamToPUCommand(Dia::Core::StringCRC id, Dia::Core::StringCRC newToPU);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "SetStreamToPU"; }
    private:
        Dia::Core::StringCRC mId;
        Dia::Core::StringCRC mNew;
        Dia::Core::StringCRC mOld;
    };

    class SetStreamCapacityCommand : public ICommand
    {
    public:
        SetStreamCapacityCommand(Dia::Core::StringCRC id, unsigned int newCapacity);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "SetStreamCapacity"; }
    private:
        Dia::Core::StringCRC mId;
        unsigned int mNew;
        unsigned int mOld;
    };

    class SetStreamMaxReadersCommand : public ICommand
    {
    public:
        SetStreamMaxReadersCommand(Dia::Core::StringCRC id, unsigned int newMax);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "SetStreamMaxReaders"; }
    private:
        Dia::Core::StringCRC mId;
        unsigned int mNew;
        unsigned int mOld;
    };

    class SetStreamOverflowCommand : public ICommand
    {
    public:
        SetStreamOverflowCommand(Dia::Core::StringCRC id, Dia::ApplicationFlow::OverflowPolicy newPolicy);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "SetStreamOverflow"; }
    private:
        Dia::Core::StringCRC mId;
        Dia::ApplicationFlow::OverflowPolicy mNew;
        Dia::ApplicationFlow::OverflowPolicy mOld;
    };

    class SetStreamMultiWriterCommand : public ICommand
    {
    public:
        SetStreamMultiWriterCommand(Dia::Core::StringCRC id, bool newValue);
        void Execute(ManifestEditorState& doc) override;
        void Undo(ManifestEditorState& doc) override;
        const char* GetDescription() const override { return "SetStreamMultiWriter"; }
    private:
        Dia::Core::StringCRC mId;
        bool mNew;
        bool mOld;
    };

}}} // namespace Dia::ApplicationFlow::Editor
