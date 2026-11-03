#pragma once

// Describe mode (DiaValueSchema Feature 6 task 4, AC 1): `--describe-schema <path> [--stage <name>]`.
//
// DESIGN NOTE (AD-001, 2026-10-21; answers Open Q1 and Q2)
//  * Flag placement (Q2): `--automation` is NOT an application-base feature. Each app's Main scans argv itself
//    (CluicheTest/Main.cpp) and `--dev-alias` has a Dia helper that Main calls (RegisterDevAliases, ManifestBootstrap).
//    Describe mode follows the second shape: Main calls ParseDescribeArgs(argc, argv, request) and, when active,
//    DescribeSession::Begin(request) before Application::Start(); after the main loop it returns
//    DescribeSession::FinalExitCode(). One extra line pair in each Main (CluicheTest, CoW); no Application change.
//    DescribeSession is a small process-level handoff (request in, exit code out) in the same spirit as the PathStore
//    dev aliases; it is startup state, not a service locator. It is the only global in this feature.
//  * Barrier (Q1): the Application has no "all PUs finished Init" signal, but every module exposes its ModuleState
//    through IApplicationInspectable::GetActiveModules (atomic, readable from any thread). "Init finished on every PU"
//    is "no module is kStarting/kStopping, none kFailed, no stage transition in flight". ValueSchemaModule (kAny,
//    runs wherever the manifest puts it) polls that from DoUpdate through ApplicationDescribeBarrier (in
//    ValueSchemaModule.cpp), requiring two consecutive ready polls to ride out the main-thread window between a stage
//    commit and BeginStart of the incoming modules. No new Application mechanism.
//  * Clean exit: the module calls IApplicationControl::RequestShutdown() (thread-safe); the app's own loop finishes
//    (Application::Update returns false) and Main returns DescribeSession::FinalExitCode(). Active-but-never-completed
//    (e.g. the manifest does not host ValueSchemaModule) is exit code kExitNotRun.
//  * `--stage <name>`: the barrier adapter queues IApplicationControl::TransitionTo(stage) once (the same primitive
//    AutomationService::navigate_to ends in), then waits until GetCurrentStage()==stage and the barrier above holds.
//    Requires ValueSchemaModule to be active in that stage (declared for "all" or the stage).
//  * Testable core: DescribeModeRunner (barrier-agnostic, fakes in TestDescribeMode.cpp) writes through
//    ValueSchemaExporter, once, and reports status + exit code. No window, renderer or debug server is involved.
//
// Observation: Info on write, Error (with the message) on failure; the exporter owns its profile scope and gauges.

#include <DiaValueSchema/DescribeSession.h>
#include <DiaValueSchema/ValueInfo.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/CodeSideCollector.h>
#include <DiaValueSchema/ComponentFieldCollector.h>
#include <DiaCore/Strings/String256.h>

namespace Dia::ValueSchema
{
	enum class BarrierState { kWaiting, kReady, kAborted };

	// "Every processing unit has finished Init" (and the requested stage is entered). Polled until not kWaiting.
	class IDescribeBarrier
	{
	public:
		virtual ~IDescribeBarrier() = default;
		virtual BarrierState Poll() = 0;
	};

	// What to export, resolved only once the barrier is ready (Sim-Hosted Command Help task 4b). Sim-hosted command names reach
	// Main after the module's DoStart, so the export cannot use the DoStart snapshot: the provider rebuilds it when the
	// catalogue has settled. Pointers must stay valid until the next Acquire call. Either report may be null.
	struct DescribeView
	{
		const ValueSchemaService*		mService = nullptr;
		const CodeSideReport*			mCodeSide = nullptr;
		const ComponentFieldReport*		mComponents = nullptr;
	};

	class IDescribeSnapshotProvider
	{
	public:
		virtual ~IDescribeSnapshotProvider() = default;
		// False = not ready yet (the runner stays kWaiting and asks again next tick). True fills 'out'.
		virtual bool Acquire(DescribeView& out) = 0;
	};

	enum class DescribeStatus { kWaiting, kDone, kFailed };

	class DescribeModeRunner
	{
	public:
		static constexpr int kExitOk = DescribeExit::kOk;
		static constexpr int kExitWriteFailed = DescribeExit::kWriteFailed;
		static constexpr int kExitAborted = DescribeExit::kAborted;
		static constexpr int kExitNotRun = DescribeExit::kNotRun;
		static constexpr int kExitTimedOut = DescribeExit::kTimedOut;

		explicit DescribeModeRunner(const DescribeRequest& request) : mRequest(request) {}

		// Polls the barrier until ready, then writes the export once. Either report may be null. After the first
		// terminal status further calls return it unchanged without polling or writing. An inactive request never writes.
		DescribeStatus Tick(IDescribeBarrier& barrier, const ValueSchemaService& service, const CodeSideReport* codeSide,
			const ComponentFieldReport* components);

		// Same, but the data to export is requested from 'provider' after the barrier is ready (and only then).
		DescribeStatus Tick(IDescribeBarrier& barrier, IDescribeSnapshotProvider& provider);

		// Terminal failure from outside the barrier (timeout). No effect once finished.
		void Fail(const char* message, int exitCode = kExitTimedOut);

		bool			IsFinished() const	{ return mStatus != DescribeStatus::kWaiting; }
		DescribeStatus	GetStatus() const	{ return mStatus; }
		int				GetExitCode() const	{ return mExitCode; }
		const char*		GetMessage() const	{ return mMessage.AsCStr(); }
		const DescribeRequest& GetRequest() const { return mRequest; }

	private:
		DescribeRequest							mRequest;
		DescribeStatus							mStatus = DescribeStatus::kWaiting;
		int										mExitCode = kExitOk;
		Dia::Core::Containers::String256		mMessage;
	};

}
