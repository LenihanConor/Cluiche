#include "DiaApplicationEditor/V2/LiveStateStore.h"

namespace Dia
{
    namespace ApplicationFlow
    {
        namespace Editor
        {
            // -----------------------------------------------------------------------
            // Constructor
            // -----------------------------------------------------------------------
            LiveStateStore::LiveStateStore()
                : mAppState()
                , mIsActive(false)
            {
            }

            // -----------------------------------------------------------------------
            // UpdateAppState
            // -----------------------------------------------------------------------
            void LiveStateStore::UpdateAppState(const LiveAppState& state)
            {
                mAppState  = state;
                mIsActive  = true;
            }

            // -----------------------------------------------------------------------
            // UpdateModuleState
            // -----------------------------------------------------------------------
            void LiveStateStore::UpdateModuleState(const LiveModuleState& state)
            {
                // Scan for an existing entry that matches both puId and moduleId
                for (unsigned int i = 0; i < mModuleStates.Size(); ++i)
                {
                    LiveModuleState& entry = mModuleStates[i];
                    if (entry.puId == state.puId && entry.moduleId == state.moduleId)
                    {
                        entry.state = state.state;
                        return;
                    }
                }

                // No existing entry found — add new
                if (!mModuleStates.IsFull())
                {
                    mModuleStates.Add(state);
                }
                else
                {
                    // Array full: overwrite last entry (drop rather than crash)
                    const unsigned int lastIndex = mModuleStates.Size() - 1;
                    mModuleStates[lastIndex] = state;
                }
            }

            // -----------------------------------------------------------------------
            // UpdateStreamState
            // -----------------------------------------------------------------------
            void LiveStateStore::UpdateStreamState(const LiveStreamState& state)
            {
                // Scan for an existing entry that matches streamId
                for (unsigned int i = 0; i < mStreamStates.Size(); ++i)
                {
                    LiveStreamState& entry = mStreamStates[i];
                    if (entry.streamId == state.streamId)
                    {
                        entry = state;
                        return;
                    }
                }

                // No existing entry found — add new
                if (!mStreamStates.IsFull())
                {
                    mStreamStates.Add(state);
                }
                else
                {
                    // Array full: overwrite last entry (drop rather than crash)
                    const unsigned int lastIndex = mStreamStates.Size() - 1;
                    mStreamStates[lastIndex] = state;
                }
            }

            // -----------------------------------------------------------------------
            // Clear
            // -----------------------------------------------------------------------
            void LiveStateStore::Clear()
            {
                mModuleStates.RemoveAll();
                mStreamStates.RemoveAll();
                mAppState  = LiveAppState();
                mIsActive  = false;
            }

            // -----------------------------------------------------------------------
            // GetAppState
            // -----------------------------------------------------------------------
            const LiveAppState& LiveStateStore::GetAppState() const
            {
                return mAppState;
            }

            // -----------------------------------------------------------------------
            // GetModuleState
            // -----------------------------------------------------------------------
            ModuleRuntimeState LiveStateStore::GetModuleState(
                Dia::Core::StringCRC puId,
                Dia::Core::StringCRC moduleId) const
            {
                for (unsigned int i = 0; i < mModuleStates.Size(); ++i)
                {
                    const LiveModuleState& entry = mModuleStates[i];
                    if (entry.puId == puId && entry.moduleId == moduleId)
                    {
                        return entry.state;
                    }
                }

                return ModuleRuntimeState::Stopped;
            }

            // -----------------------------------------------------------------------
            // GetStreamState
            // -----------------------------------------------------------------------
            const LiveStreamState* LiveStateStore::GetStreamState(
                Dia::Core::StringCRC streamId) const
            {
                for (unsigned int i = 0; i < mStreamStates.Size(); ++i)
                {
                    const LiveStreamState& entry = mStreamStates[i];
                    if (entry.streamId == streamId)
                    {
                        return &entry;
                    }
                }

                return nullptr;
            }

            // -----------------------------------------------------------------------
            // IsActive
            // -----------------------------------------------------------------------
            bool LiveStateStore::IsActive() const
            {
                return mIsActive;
            }

        } // namespace Editor
    } // namespace ApplicationFlow
} // namespace Dia
