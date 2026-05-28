////////////////////////////////////////////////////////////////////////////////
// Filename: IApplicationInspectable.h
// DiaApplicationFlow — read-only introspection interface
//
// Provides debug tools and tests with read-only access to Application runtime
// state without coupling to Application internals.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/Streams/IStreamStore.h>   // StreamKind
#include <DiaApplicationFlow/Streams/OverflowPolicy.h> // OverflowPolicy
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace ApplicationFlow {

    struct ModuleStateInfo {
        Dia::Core::StringCRC instanceId;
        Dia::Core::StringCRC typeId;
        ModuleState          state;
        PUAffinity           allowedPUs  = PUAffinity::kAny;
        const char*          description = nullptr;
    };

    struct StreamInfo {
        Dia::Core::StringCRC id;
        Dia::Core::StringCRC type;         // payload type StringCRC (kept for compat)
        Dia::Core::StringCRC fromPU;
        Dia::Core::StringCRC toPU;
        bool                 multiWriter;
        // F4 additions:
        StreamKind           kind;
        OverflowPolicy       overflowPolicy;
        unsigned long long   currentSequence;
        unsigned int         attachedReaderCount;
        unsigned int         attachedTapCount;
    };

    struct TransitionInfo {
        bool                                                              inProgress;
        Dia::Core::StringCRC                                              fromStage;
        Dia::Core::StringCRC                                              toStage;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>   modulesStarting;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>   modulesStopping;
        bool                                                              heldByGuards = false;
    };

    class IApplicationInspectable {
    public:
        virtual ~IApplicationInspectable() = default;

        virtual Dia::Core::StringCRC GetCurrentStage() const = 0;
        virtual bool IsTransitioning() const = 0;
        virtual TransitionInfo GetTransitionInfo() const = 0;

        virtual void GetAllStages(
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16>& out) const = 0;

        virtual void GetProcessingUnits(
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>& out) const = 0;

        virtual void GetActiveModules(
            const Dia::Core::StringCRC& puId,
            Dia::Core::Containers::DynamicArrayC<ModuleStateInfo, 64>& out) const = 0;

        virtual void GetStreamInfo(
            Dia::Core::Containers::DynamicArrayC<StreamInfo, 16>& out) const = 0;

        virtual bool IsShuttingDown() const = 0;

        // Tap attachment path — returns the store for the given stream id, or null.
        // Safe to call at any time after Application::Start() returns true.
        virtual IStreamStore* FindStream(const Dia::Core::StringCRC& id) = 0;
    };

}} // namespace Dia::ApplicationFlow
