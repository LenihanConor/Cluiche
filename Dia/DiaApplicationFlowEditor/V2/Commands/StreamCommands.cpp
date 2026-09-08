#include <DiaApplicationFlowEditor/V2/Commands/StreamCommands.h>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    // -------------------------------------------------------------------------
    // AddStreamCommand
    // -------------------------------------------------------------------------

    AddStreamCommand::AddStreamCommand(const char* idStr, Dia::Core::StringCRC kind, Dia::Core::StringCRC payloadType)
        : mIsReserved(idStr != nullptr && idStr[0] == '$')
    {
        mStream.id          = Dia::Core::StringCRC(idStr);
        mStream.kind        = kind;
        mStream.payloadType = payloadType;
    }

    void AddStreamCommand::Execute(ManifestEditorState& doc)
    {
        if (mIsReserved)
        {
            return;
        }
        doc.manifest.streams.Add(mStream);
        doc.MarkDirty();
    }

    void AddStreamCommand::Undo(ManifestEditorState& doc)
    {
        if (mIsReserved)
        {
            return;
        }
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mStream.id)
            {
                streams.RemoveAt(i);
                doc.MarkDirty();
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // RemoveStreamCommand
    // -------------------------------------------------------------------------

    RemoveStreamCommand::RemoveStreamCommand(Dia::Core::StringCRC id)
        : mId(id)
        , mSavedIndex(-1)
    {
    }

    void RemoveStreamCommand::Execute(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                mSaved      = streams[i];
                mSavedIndex = static_cast<int>(i);
                streams.RemoveAt(i);
                doc.MarkDirty();
                return;
            }
        }
    }

    void RemoveStreamCommand::Undo(ManifestEditorState& doc)
    {
        if (mSavedIndex < 0)
        {
            return;
        }
        auto& streams = doc.manifest.streams;
        unsigned int insertAt = static_cast<unsigned int>(mSavedIndex);
        if (insertAt >= streams.Size())
        {
            streams.Add(mSaved);
        }
        else
        {
            streams.AddAt(mSaved, insertAt);
        }
        doc.MarkDirty();
    }

    // -------------------------------------------------------------------------
    // SetStreamKindCommand
    // -------------------------------------------------------------------------

    SetStreamKindCommand::SetStreamKindCommand(Dia::Core::StringCRC id, Dia::Core::StringCRC newKind)
        : mId(id)
        , mNewKind(newKind)
    {
    }

    void SetStreamKindCommand::Execute(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                mOldKind       = streams[i].kind;
                streams[i].kind = mNewKind;
                doc.MarkDirty();
                return;
            }
        }
    }

    void SetStreamKindCommand::Undo(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                streams[i].kind = mOldKind;
                doc.MarkDirty();
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // SetStreamPayloadCommand
    // -------------------------------------------------------------------------

    SetStreamPayloadCommand::SetStreamPayloadCommand(Dia::Core::StringCRC id, Dia::Core::StringCRC newPayload)
        : mId(id)
        , mNewPayload(newPayload)
    {
    }

    void SetStreamPayloadCommand::Execute(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                mOldPayload            = streams[i].payloadType;
                streams[i].payloadType = mNewPayload;
                doc.MarkDirty();
                return;
            }
        }
    }

    void SetStreamPayloadCommand::Undo(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                streams[i].payloadType = mOldPayload;
                doc.MarkDirty();
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // SetStreamFromPUCommand
    // -------------------------------------------------------------------------

    SetStreamFromPUCommand::SetStreamFromPUCommand(Dia::Core::StringCRC id, Dia::Core::StringCRC newFromPU)
        : mId(id)
        , mNew(newFromPU)
    {
    }

    void SetStreamFromPUCommand::Execute(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                mOld              = streams[i].fromPU;
                streams[i].fromPU = mNew;
                doc.MarkDirty();
                return;
            }
        }
    }

    void SetStreamFromPUCommand::Undo(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                streams[i].fromPU = mOld;
                doc.MarkDirty();
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // SetStreamToPUCommand
    // -------------------------------------------------------------------------

    SetStreamToPUCommand::SetStreamToPUCommand(Dia::Core::StringCRC id, Dia::Core::StringCRC newToPU)
        : mId(id)
        , mNew(newToPU)
    {
    }

    void SetStreamToPUCommand::Execute(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                mOld            = streams[i].toPU;
                streams[i].toPU = mNew;
                doc.MarkDirty();
                return;
            }
        }
    }

    void SetStreamToPUCommand::Undo(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                streams[i].toPU = mOld;
                doc.MarkDirty();
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // SetStreamCapacityCommand
    // -------------------------------------------------------------------------

    SetStreamCapacityCommand::SetStreamCapacityCommand(Dia::Core::StringCRC id, unsigned int newCapacity)
        : mId(id)
        , mNew(newCapacity)
        , mOld(0u)
    {
    }

    void SetStreamCapacityCommand::Execute(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                mOld                 = streams[i].capacity;
                streams[i].capacity  = mNew;
                doc.MarkDirty();
                return;
            }
        }
    }

    void SetStreamCapacityCommand::Undo(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                streams[i].capacity = mOld;
                doc.MarkDirty();
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // SetStreamMaxReadersCommand
    // -------------------------------------------------------------------------

    SetStreamMaxReadersCommand::SetStreamMaxReadersCommand(Dia::Core::StringCRC id, unsigned int newMax)
        : mId(id)
        , mNew(newMax)
        , mOld(0u)
    {
    }

    void SetStreamMaxReadersCommand::Execute(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                mOld                  = streams[i].maxReaders;
                streams[i].maxReaders = mNew;
                doc.MarkDirty();
                return;
            }
        }
    }

    void SetStreamMaxReadersCommand::Undo(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                streams[i].maxReaders = mOld;
                doc.MarkDirty();
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // SetStreamOverflowCommand
    // -------------------------------------------------------------------------

    SetStreamOverflowCommand::SetStreamOverflowCommand(Dia::Core::StringCRC id, Dia::ApplicationFlow::OverflowPolicy newPolicy)
        : mId(id)
        , mNew(newPolicy)
        , mOld(Dia::ApplicationFlow::OverflowPolicy::kDropOldest)
    {
    }

    void SetStreamOverflowCommand::Execute(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                mOld                       = streams[i].overflowPolicy;
                streams[i].overflowPolicy  = mNew;
                doc.MarkDirty();
                return;
            }
        }
    }

    void SetStreamOverflowCommand::Undo(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                streams[i].overflowPolicy = mOld;
                doc.MarkDirty();
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // SetStreamMultiWriterCommand
    // -------------------------------------------------------------------------

    SetStreamMultiWriterCommand::SetStreamMultiWriterCommand(Dia::Core::StringCRC id, bool newValue)
        : mId(id)
        , mNew(newValue)
        , mOld(false)
    {
    }

    void SetStreamMultiWriterCommand::Execute(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                mOld                    = streams[i].multiWriter;
                streams[i].multiWriter  = mNew;
                doc.MarkDirty();
                return;
            }
        }
    }

    void SetStreamMultiWriterCommand::Undo(ManifestEditorState& doc)
    {
        auto& streams = doc.manifest.streams;
        for (unsigned int i = 0u; i < streams.Size(); ++i)
        {
            if (streams[i].id == mId)
            {
                streams[i].multiWriter = mOld;
                doc.MarkDirty();
                return;
            }
        }
    }

}}} // namespace Dia::ApplicationFlow::Editor
