////////////////////////////////////////////////////////////////////////////////
// Filename: Module.cpp
// DiaApplicationFlow — v2 Module base class
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplicationFlow/Module.h"
#include "DiaApplicationFlow/Application.h"
#include "DiaApplicationFlow/IApplicationControl.h"
#include "DiaApplicationFlow/ProcessingUnit.h"

#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>

namespace Dia { namespace ApplicationFlow {

    // Returns the instance id of this module's processing unit for log output,
    // or "<unassigned>" if the PU pointer hasn't been set yet.
    static const char* PuName(const ProcessingUnit* pu)
    {
        return pu ? pu->GetInstanceId().AsChar() : "<unassigned>";
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
        // Release: publish mStateElapsedMs / mStartLogged resets before the
        // dedicated PU thread observes kStarting and begins ticking DoStart().
        mState.store(ModuleState::kStarting, std::memory_order_release);

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
        // Release: publish resets before the PU thread observes kStopping.
        mState.store(ModuleState::kStopping, std::memory_order_release);

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
                    mStateElapsedMs = 0.0f;
                    mState.store(ModuleState::kActive, std::memory_order_release);
                }
                else if (result == StartResult::kFailed)
                {
                    DIA_LOG_ERROR("Application", "Module '%s' (PU '%s') DoStart FAILED", mInstanceId.AsChar(), PuName(mProcessingUnit));
                    mState.store(ModuleState::kFailed, std::memory_order_release);
                }
                else // kLoading — still starting; check timeout
                {
                    if (mStateElapsedMs > startTimeoutMs)
                    {
                        DIA_LOG_ERROR("Application", "Module '%s' (PU '%s') DoStart TIMEOUT (%.1f ms)", mInstanceId.AsChar(), PuName(mProcessingUnit), mStateElapsedMs);
                        mState.store(ModuleState::kFailed, std::memory_order_release);
                    }
                }
                break;
            }

            case ModuleState::kActive:
            {
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
                    mStateElapsedMs = 0.0f;
                    mState.store(ModuleState::kInactive, std::memory_order_release);
                }
                else // kStopping — still winding down; check timeout
                {
                    if (mStateElapsedMs > stopTimeoutMs)
                    {
                        DIA_LOG_WARNING("Application", "Module '%s' (PU '%s') DoStop TIMEOUT (%.1f ms) -> forcing Inactive", mInstanceId.AsChar(), PuName(mProcessingUnit), mStateElapsedMs);
                        mStateElapsedMs = 0.0f;
                        mState.store(ModuleState::kInactive, std::memory_order_release);
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
