#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
    namespace ApplicationFlow
    {
        namespace Editor
        {
            // -----------------------------------------------------------------------
            // ModuleRuntimeState
            // -----------------------------------------------------------------------
            enum class ModuleRuntimeState
            {
                Stopped,
                Loading,
                Running,
                Failed
            };

            // -----------------------------------------------------------------------
            // LiveModuleState
            // -----------------------------------------------------------------------
            struct LiveModuleState
            {
                Dia::Core::StringCRC moduleId;
                Dia::Core::StringCRC puId;
                ModuleRuntimeState   state;
            };

            // -----------------------------------------------------------------------
            // LiveAppState
            // -----------------------------------------------------------------------
            struct LiveAppState
            {
                Dia::Core::StringCRC currentStage;
                bool                 isTransitioning;
                Dia::Core::StringCRC targetStage;   // valid during transition

                LiveAppState()
                    : isTransitioning(false)
                {
                }
            };

            // -----------------------------------------------------------------------
            // LiveStreamState
            // -----------------------------------------------------------------------
            struct LiveStreamState
            {
                Dia::Core::StringCRC streamId;
                unsigned int         messagesPerSec;
                unsigned int         bytesPerSec;
            };

            // -----------------------------------------------------------------------
            // LiveStateStore
            //
            // Holds runtime state pushed from a connected game instance via
            // WebSocket messages from DiaDebugServer. All state is cleared when
            // the connection drops (Clear()).
            // -----------------------------------------------------------------------
            class LiveStateStore
            {
            public:
                static constexpr unsigned int kMaxModuleStates = 64;
                static constexpr unsigned int kMaxStreamStates = 16;

                LiveStateStore();

                // Writers — called by the WebSocket message handler
                void UpdateAppState(const LiveAppState& state);
                void UpdateModuleState(const LiveModuleState& state);
                void UpdateStreamState(const LiveStreamState& state);

                // Reset all state (called on disconnect)
                void Clear();

                // Readers
                const LiveAppState& GetAppState() const;
                ModuleRuntimeState  GetModuleState(Dia::Core::StringCRC puId,
                                                   Dia::Core::StringCRC moduleId) const;
                const LiveStreamState* GetStreamState(Dia::Core::StringCRC streamId) const;

                // Returns true when connected and at least one UpdateAppState has been received
                bool IsActive() const;

            private:
                LiveAppState mAppState;
                Dia::Core::Containers::DynamicArrayC<LiveModuleState, kMaxModuleStates> mModuleStates;
                Dia::Core::Containers::DynamicArrayC<LiveStreamState, kMaxStreamStates> mStreamStates;
                bool mIsActive;
            };

        } // namespace Editor
    } // namespace ApplicationFlow
} // namespace Dia
