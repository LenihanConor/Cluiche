////////////////////////////////////////////////////////////////////////////////
// Filename: Module.cpp
// DiaApplicationFlow — v2 Module base class
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplicationFlow/Module.h"
#include "DiaApplicationFlow/Application.h"
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
        , mState(ModuleState::kInactive)
        , mStateElapsedMs(0.0f)
        , mStartLogged(false)
        , mStopLogged(false)
    {
    }

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
    ModuleState Module::GetState() const
    {
        return mState;
    }

    //--------------------------------------------------------------------------
    void Module::SetProcessingUnit(ProcessingUnit* pu)
    {
        mProcessingUnit = pu;
    }

    //--------------------------------------------------------------------------
    void Module::TransitionTo(const Dia::Core::StringCRC& stageId)
    {
        DIA_ASSERT(mProcessingUnit != nullptr, "Module '%s' (PU '%s') TransitionTo called with no ProcessingUnit set", mInstanceId.AsChar());
        Application* app = mProcessingUnit->GetApplication();
        DIA_ASSERT(app != nullptr, "Module '%s' (PU '%s') TransitionTo: ProcessingUnit has no Application set", mInstanceId.AsChar());
        if (app)
            app->TransitionTo(stageId);
    }

    //--------------------------------------------------------------------------
    void Module::BeginStart()
    {
        DIA_ASSERT(mState == ModuleState::kInactive, "Module '%s' (PU '%s') BeginStart called in wrong state", mInstanceId.AsChar(), PuName(mProcessingUnit));

        mState          = ModuleState::kStarting;
        mStateElapsedMs = 0.0f;
        mStartLogged    = false;

        DIA_LOG_INFO("Application", "Module '%s' (PU '%s') BeginStart", mInstanceId.AsChar(), PuName(mProcessingUnit));
    }

    //--------------------------------------------------------------------------
    void Module::BeginStop()
    {
        DIA_ASSERT(mState == ModuleState::kActive, "Module '%s' (PU '%s') BeginStop called in wrong state", mInstanceId.AsChar(), PuName(mProcessingUnit));

        mState          = ModuleState::kStopping;
        mStateElapsedMs = 0.0f;
        mStopLogged     = false;

        DIA_LOG_INFO("Application", "Module '%s' (PU '%s') BeginStop", mInstanceId.AsChar(), PuName(mProcessingUnit));
    }

    //--------------------------------------------------------------------------
    void Module::FrameTick(float deltaTime, float startTimeoutMs, float stopTimeoutMs)
    {
        mStateElapsedMs += deltaTime * 1000.0f;

        switch (mState)
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
                    mState          = ModuleState::kActive;
                    mStateElapsedMs = 0.0f;
                }
                else if (result == StartResult::kFailed)
                {
                    DIA_LOG_ERROR("Application", "Module '%s' (PU '%s') DoStart FAILED", mInstanceId.AsChar(), PuName(mProcessingUnit));
                    mState = ModuleState::kFailed;
                }
                else // kLoading — still starting; check timeout
                {
                    if (mStateElapsedMs > startTimeoutMs)
                    {
                        DIA_LOG_ERROR("Application", "Module '%s' (PU '%s') DoStart TIMEOUT (%.1f ms)", mInstanceId.AsChar(), PuName(mProcessingUnit), mStateElapsedMs);
                        mState = ModuleState::kFailed;
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
                    mState          = ModuleState::kInactive;
                    mStateElapsedMs = 0.0f;
                }
                else // kStopping — still winding down; check timeout
                {
                    if (mStateElapsedMs > stopTimeoutMs)
                    {
                        DIA_LOG_WARNING("Application", "Module '%s' (PU '%s') DoStop TIMEOUT (%.1f ms) -> forcing Inactive", mInstanceId.AsChar(), PuName(mProcessingUnit), mStateElapsedMs);
                        mState          = ModuleState::kInactive;
                        mStateElapsedMs = 0.0f;
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
