////////////////////////////////////////////////////////////////////////////////
// Filename: IApplicationInspectable.h
// DiaApplicationFlow — read-only introspection interface
//
// Provides debug tools and tests with read-only access to Application runtime
// state without coupling to Application internals.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace ApplicationFlow {

    struct ModuleStateInfo {
        Dia::Core::StringCRC instanceId;
        Dia::Core::StringCRC typeId;
        ModuleState          state;
    };

    struct StreamInfo {
        Dia::Core::StringCRC id;
        Dia::Core::StringCRC type;
        Dia::Core::StringCRC fromPU;
        Dia::Core::StringCRC toPU;
        bool                 multiWriter;
    };

    struct TransitionInfo {
        bool                                                              inProgress;
        Dia::Core::StringCRC                                              fromStage;
        Dia::Core::StringCRC                                              toStage;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>   modulesStarting;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>   modulesStopping;
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
    };

}} // namespace Dia::ApplicationFlow
