////////////////////////////////////////////////////////////////////////////////
// Filename: Module.cpp
// DiaApplicationFlow — v2 Module base class
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplicationFlow/Module.h"
#include "DiaApplicationFlow/Application.h"
#include "DiaApplicationFlow/IApplicationControl.h"
#include "DiaApplicationFlow/ProcessingUnit.h"
#include "DiaApplicationFlow/LifecycleEvent.h"

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Health/HealthRegistry.h>

namespace Dia { namespace ApplicationFlow {

    // Returns the instance id of this module's processing unit for log output,
    // or "<unassigned>" if the PU pointer hasn't been set yet.
    static const char* PuName(const ProcessingUnit* pu)
    {
        return pu ? pu->GetInstanceId().AsChar() : "<unassigned>";
    }

    static const char* ModuleStateName(ModuleState s)
    {
        switch(s)
        {
            case ModuleState::kInactive:  return "kInactive";
            case ModuleState::kStarting:  return "kStarting";
            case ModuleState::kActive:    return "kActive";
            case ModuleState::kStopping:  return "kStopping";
            case ModuleState::kFailed:    return "kFailed";
            default: return "unknown";
        }
    }

    //--------------------------------------------------------------------------
    Module::Module(const Dia::Core::StringCRC& instanceId)
        : mInstanceId(instanceId)
        , mProcessingUnit(nullptr)
        , mApplication(nullptr)
        , mState(ModuleState::kInactive)
        , mStateElapsedMs(0.0f)
        , mStartLogged(false)
        , mStopLogged(false)
    {
    }

    //--------------------------------------------------------------------------
    // Helpers (file-local) for atomic state access.
    //--------------------------------------------------------------------------

    //--------------------------------------------------------------------------
    const Dia::Core::StringCRC& Module::GetInstanceId() const
    {
        return mInstanceId;
    }

    //--------------------------------------------------------------------------
    ProcessingUnit* Module::GetProcessingUnit() const
    {
        return mProcessingUnit;
    }

    //--------------------------------------------------------------------------
    IApplicationControl* Module::GetApplication() const
    {
        // Application implements IApplicationControl; upcast hides the full
        // Application surface (stream registration, introspection) from
        // runtime module code.
        return static_cast<IApplicationControl*>(mApplication);
    }

    //--------------------------------------------------------------------------
    ModuleState Module::GetState() const
    {
        return mState.load(std::memory_order_acquire);
    }

    //--------------------------------------------------------------------------
    void Module::SetProcessingUnit(ProcessingUnit* pu)
    {
        mProcessingUnit = pu;
    }

    //--------------------------------------------------------------------------
    void Module::SetApplication(Application* app)
    {
        mApplication = app;
    }

    //--------------------------------------------------------------------------
    void Module::SetLifecycleEmitter(Application* app)
    {
        // We reuse mApplication for lifecycle emission (it's the same pointer).
        // This method exists so the call site is explicit.
        (void)app;  // already set via SetApplication
    }

    //--------------------------------------------------------------------------
    void Module::EmitModuleStateChanged(ModuleState prev, ModuleState next)
    {
        if (!mApplication)
            return;
        LifecycleEvent ev;
        ev.kind             = LifecycleEventKind::kModuleStateChanged;
        ev.moduleInstanceId = mInstanceId;
        ev.previousState    = prev;
        ev.newState         = next;
        mApplication->EmitLifecycleEvent(ev);
    }

    //--------------------------------------------------------------------------
    void Module::TransitionTo(const Dia::Core::StringCRC& stageId)
    {
        DIA_ASSERT(mApplication != nullptr,
                   "Module '%s' (PU '%s') TransitionTo called before Application was set",
                   mInstanceId.AsChar(), PuName(mProcessingUnit));
        if (mApplication)
            mApplication->TransitionTo(stageId);
    }

    //--------------------------------------------------------------------------
    void Module::BeginStart()
    {
        DIA_ASSERT(mState.load(std::memory_order_acquire) == ModuleState::kInactive,
                   "Module '%s' (PU '%s') BeginStart called in wrong state",
                   mInstanceId.AsChar(), PuName(mProcessingUnit));

        mStateElapsedMs = 0.0f;
        mStartLogged    = false;
        mState.store(ModuleState::kStarting, std::memory_order_release);
        EmitModuleStateChanged(ModuleState::kInactive, ModuleState::kStarting);

        mLifecycleReporter.SetName(mInstanceId);
        Dia::Observation::Health::HealthRegistry::Instance().Register(&mLifecycleReporter);

        DIA_LOG_INFO("module", "module.state.transition module_id=%s from=%s to=%s",
                     mInstanceId.AsChar(),
                     ModuleStateName(ModuleState::kInactive),
                     ModuleStateName(ModuleState::kStarting));
        DIA_LOG_INFO("Application", "Module '%s' (PU '%s') BeginStart", mInstanceId.AsChar(), PuName(mProcessingUnit));
    }

    //--------------------------------------------------------------------------
    void Module::BeginStop()
    {
        DIA_ASSERT(mState.load(std::memory_order_acquire) == ModuleState::kActive,
                   "Module '%s' (PU '%s') BeginStop called in wrong state",
                   mInstanceId.AsChar(), PuName(mProcessingUnit));

        mStateElapsedMs = 0.0f;
        mStopLogged     = false;
        mState.store(ModuleState::kStopping, std::memory_order_release);
        EmitModuleStateChanged(ModuleState::kActive, ModuleState::kStopping);

        DIA_LOG_INFO("module", "module.state.transition module_id=%s from=%s to=%s",
                     mInstanceId.AsChar(),
                     ModuleStateName(ModuleState::kActive),
                     ModuleStateName(ModuleState::kStopping));
        DIA_LOG_INFO("Application", "Module '%s' (PU '%s') BeginStop", mInstanceId.AsChar(), PuName(mProcessingUnit));
    }

    //--------------------------------------------------------------------------
    void Module::FrameTick(float deltaTime, float startTimeoutMs, float stopTimeoutMs)
    {
        mStateElapsedMs += deltaTime * 1000.0f;

        // Acquire: pairs with BeginStart/BeginStop release stores so any
        // pre-transition resets (mStateElapsedMs, mStartLogged, mStopLogged)
        // are visible on this thread before we branch on the state.
        const ModuleState state = mState.load(std::memory_order_acquire);

        switch (state)
        {
            case ModuleState::kStarting:
            {
                if (!mStartLogged)
                {
                    DIA_LOG_INFO("Application", "Module '%s' (PU '%s') DoStart", mInstanceId.AsChar(), PuName(mProcessingUnit));
                    mStartLogged = true;
                }

                const StartResult result = DoStart();

                if (result == StartResult::kReady)
                {
                    DIA_LOG_INFO("Application", "Module '%s' (PU '%s') DoStart complete -> Active", mInstanceId.AsChar(), PuName(mProcessingUnit));
                    mLifecycleReporter.SetOK();
                    mStateElapsedMs = 0.0f;
                    mState.store(ModuleState::kActive, std::memory_order_release);
                    EmitModuleStateChanged(ModuleState::kStarting, ModuleState::kActive);
                    DIA_LOG_INFO("module", "module.state.transition module_id=%s from=%s to=%s",
                                 mInstanceId.AsChar(),
                                 ModuleStateName(ModuleState::kStarting),
                                 ModuleStateName(ModuleState::kActive));
                    DIA_LOG_INFO("module", "module.start.complete module_id=%s duration_ms=%.1f",
                                 mInstanceId.AsChar(), mStateElapsedMs);
                }
                else if (result == StartResult::kFailed)
                {
                    DIA_LOG_ERROR("Application", "Module '%s' (PU '%s') DoStart FAILED", mInstanceId.AsChar(), PuName(mProcessingUnit));
                    mLifecycleReporter.SetFailing(Dia::Core::StringCRC("module.failed"));
                    mState.store(ModuleState::kFailed, std::memory_order_release);
                    EmitModuleStateChanged(ModuleState::kStarting, ModuleState::kFailed);
                    DIA_LOG_INFO("module", "module.state.transition module_id=%s from=%s to=%s",
                                 mInstanceId.AsChar(),
                                 ModuleStateName(ModuleState::kStarting),
                                 ModuleStateName(ModuleState::kFailed));
                }
                else // kLoading — still starting; check timeout
                {
                    if (mStateElapsedMs > startTimeoutMs * 0.5f)
                        mLifecycleReporter.SetDegraded(Dia::Core::StringCRC("module.start.slow"));

                    if (mStateElapsedMs > startTimeoutMs)
                    {
                        DIA_LOG_ERROR("Application", "Module '%s' (PU '%s') DoStart TIMEOUT (%.1f ms)", mInstanceId.AsChar(), PuName(mProcessingUnit), mStateElapsedMs);
                        mLifecycleReporter.SetFailing(Dia::Core::StringCRC("module.start.timeout"));
                        mState.store(ModuleState::kFailed, std::memory_order_release);
                        DIA_LOG_INFO("module", "module.state.transition module_id=%s from=%s to=%s",
                                     mInstanceId.AsChar(),
                                     ModuleStateName(ModuleState::kStarting),
                                     ModuleStateName(ModuleState::kFailed));
                    }
                }
                break;
            }

            case ModuleState::kActive:
            {
                DIA_PROFILE_SCOPE("module.update", Dia::Observation::Profile::Category::kDiaApplicationFlow);
                DIA_TRACE_ZONE("module.update");
                DoUpdate(deltaTime);
                break;
            }

            case ModuleState::kStopping:
            {
                if (!mStopLogged)
                {
                    DIA_LOG_INFO("Application", "Module '%s' (PU '%s') DoStop", mInstanceId.AsChar(), PuName(mProcessingUnit));
                    mStopLogged = true;
                }

                const StopResult result = DoStop();

                if (result == StopResult::kDone)
                {
                    DIA_LOG_INFO("Application", "Module '%s' (PU '%s') DoStop complete -> Inactive", mInstanceId.AsChar(), PuName(mProcessingUnit));
                    mLifecycleReporter.SetOK();
                    Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mLifecycleReporter);
                    mStateElapsedMs = 0.0f;
                    mState.store(ModuleState::kInactive, std::memory_order_release);
                    EmitModuleStateChanged(ModuleState::kStopping, ModuleState::kInactive);
                    DIA_LOG_INFO("module", "module.state.transition module_id=%s from=%s to=%s",
                                 mInstanceId.AsChar(),
                                 ModuleStateName(ModuleState::kStopping),
                                 ModuleStateName(ModuleState::kInactive));
                    DIA_LOG_INFO("module", "module.stop.complete module_id=%s duration_ms=%.1f",
                                 mInstanceId.AsChar(), mStateElapsedMs);
                }
                else // kStopping — still winding down; check timeout
                {
                    if (mStateElapsedMs > stopTimeoutMs)
                    {
                        DIA_LOG_WARNING("Application", "Module '%s' (PU '%s') DoStop TIMEOUT (%.1f ms) -> forcing Inactive", mInstanceId.AsChar(), PuName(mProcessingUnit), mStateElapsedMs);
                        mLifecycleReporter.SetOK();
                        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mLifecycleReporter);
                        mStateElapsedMs = 0.0f;
                        mState.store(ModuleState::kInactive, std::memory_order_release);
                        EmitModuleStateChanged(ModuleState::kStopping, ModuleState::kInactive);
                        DIA_LOG_INFO("module", "module.state.transition module_id=%s from=%s to=%s",
                                     mInstanceId.AsChar(),
                                     ModuleStateName(ModuleState::kStopping),
                                     ModuleStateName(ModuleState::kInactive));
                    }
                }
                break;
            }

            case ModuleState::kInactive:
            case ModuleState::kFailed:
            default:
                // No-op: framework should not tick a module in these states.
                break;
        }
    }

}} // namespace Dia::ApplicationFlow
