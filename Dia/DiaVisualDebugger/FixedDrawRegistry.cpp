////////////////////////////////////////////////////////////////////////////////
// Filename: FixedDrawRegistry.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaVisualDebugger/FixedDrawRegistry.h"

#ifdef DIA_DEBUG

#include <DiaCore/Core/Assert.h>
#include <DiaGraphics/Frame/DebugFrameDataVisitor.h>

namespace Dia
{
    namespace Debug
    {
        FixedDrawRegistry::~FixedDrawRegistry()
        {
            for (unsigned int i = 0; i < mEntries.Size(); ++i)
                delete mEntries[i].buffer;
        }

        void FixedDrawRegistry::Register(
            Dia::Core::StringCRC name,
            const void*          sourceObject,
            IObjectRenderer*     renderer,
            unsigned int         capacity,
            int                  priority)
        {
            DIA_ASSERT(FindIndex(name) < 0, "FixedDrawRegistry::Register — name already registered");
            DIA_ASSERT(renderer != nullptr, "FixedDrawRegistry::Register — renderer must not be null");
            DIA_ASSERT(sourceObject != nullptr, "FixedDrawRegistry::Register — sourceObject must not be null");

            FixedEntry entry;
            entry.name         = name;
            entry.sourceObject = sourceObject;
            entry.renderer     = renderer;
            entry.buffer       = new FixedPrimitiveBuffer(capacity);
            entry.priority     = priority;
            entry.enabled      = true;
            entry.dirty        = true;
            mEntries.Add(entry);
            mSortDirty = true;
        }

        void FixedDrawRegistry::Unregister(Dia::Core::StringCRC name)
        {
            int index = FindIndex(name);
            if (index < 0)
                return;
            delete mEntries[static_cast<unsigned int>(index)].buffer;
            mEntries.RemoveAt(static_cast<unsigned int>(index));
            mSortDirty = true;
        }

        void FixedDrawRegistry::EnableLayer(Dia::Core::StringCRC name)
        {
            int index = FindIndex(name);
            if (index < 0)
                return;
            mEntries[static_cast<unsigned int>(index)].enabled = true;
        }

        void FixedDrawRegistry::DisableLayer(Dia::Core::StringCRC name)
        {
            int index = FindIndex(name);
            if (index < 0)
                return;
            mEntries[static_cast<unsigned int>(index)].enabled = false;
        }

        bool FixedDrawRegistry::IsLayerEnabled(Dia::Core::StringCRC name) const
        {
            int index = FindIndex(name);
            if (index < 0)
                return false;
            return mEntries[static_cast<unsigned int>(index)].enabled;
        }

        bool FixedDrawRegistry::HasLayer(Dia::Core::StringCRC name) const
        {
            return FindIndex(name) >= 0;
        }

        void FixedDrawRegistry::Invalidate(Dia::Core::StringCRC name)
        {
            DIA_ASSERT(!mIsDrawing, "FixedDrawRegistry::Invalidate — must not be called during DrawFixed()");
            int index = FindIndex(name);
            DIA_ASSERT(index >= 0, "FixedDrawRegistry::Invalidate — layer not registered");
            if (index < 0)
                return;
            mEntries[static_cast<unsigned int>(index)].dirty = true;
        }

        void FixedDrawRegistry::DrawFixed(const Dia::Graphics::DebugFrameDataVisitor& visitor)
        {
            mIsDrawing = true;

            if (mSortDirty)
                SortByPriority();

            for (unsigned int i = 0; i < mEntries.Size(); ++i)
            {
                FixedEntry& entry = mEntries[i];
                if (entry.dirty)
                    RebuildEntry(entry);
                if (entry.enabled)
                    entry.buffer->AcceptVisitor(visitor);
            }

            mIsDrawing = false;
        }

        void FixedDrawRegistry::RebuildEntry(FixedEntry& entry)
        {
            entry.buffer->Clear();
            entry.renderer->BuildPrimitives(entry.sourceObject, *entry.buffer);
            entry.dirty = false;
        }

        void FixedDrawRegistry::SortByPriority()
        {
            // Insertion sort — stable, O(N²) acceptable for kMaxFixed = 32
            for (unsigned int i = 1; i < mEntries.Size(); ++i)
            {
                FixedEntry key = mEntries[i];
                int j = static_cast<int>(i) - 1;
                while (j >= 0 && mEntries[static_cast<unsigned int>(j)].priority > key.priority)
                {
                    mEntries[static_cast<unsigned int>(j + 1)] = mEntries[static_cast<unsigned int>(j)];
                    --j;
                }
                mEntries[static_cast<unsigned int>(j + 1)] = key;
            }
            mSortDirty = false;
        }

        int FixedDrawRegistry::FindIndex(Dia::Core::StringCRC name) const
        {
            for (unsigned int i = 0; i < mEntries.Size(); ++i)
            {
                if (mEntries[i].name == name)
                    return static_cast<int>(i);
            }
            return -1;
        }

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
