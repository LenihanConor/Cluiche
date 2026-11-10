#include <DiaValueSchema/DescribeMode.h>
#include <DiaValueSchema/ValueSchemaExporter.h>

#include <DiaObservation/Log/DiaLog.h>

#include <atomic>
#include <cstdio>
#include <cstring>

namespace Dia::ValueSchema
{
	namespace
	{
		bool IsFlag(const char* s) { return s != nullptr && s[0] == '-' && s[1] == '-'; }

		DescribeRequest		sRequest;
		std::atomic<bool>	sComplete{ false };
		std::atomic<int>	sExitCode{ 0 };
	}

	bool ParseDescribeArgs(int argc, const char* const argv[], DescribeRequest& out)
	{
		out = DescribeRequest();
		bool haveDescribe = false;
		for (int i = 1; i < argc; ++i)
		{
			const char* arg = argv[i];
			if (arg == nullptr) { continue; }
			const bool isDescribe = std::strcmp(arg, "--describe-schema") == 0;
			const bool isStage = std::strcmp(arg, "--stage") == 0;
			if (!isDescribe && !isStage) { continue; }

			const char* value = (i + 1 < argc) ? argv[i + 1] : nullptr;
			if (value == nullptr || value[0] == '\0' || IsFlag(value))
			{
				if (isDescribe) { std::printf("--describe-schema needs a file path\n"); out = DescribeRequest(); return false; }
				continue;	// --stage without a value: ignored (only meaningful with --describe-schema)
			}
			++i;
			if (isDescribe)
			{
				if (std::strlen(value) >= out.mPath.Capacity())
				{
					std::printf("--describe-schema path is too long (max %u characters)\n", static_cast<unsigned int>(out.mPath.Capacity() - 1));
					out = DescribeRequest();
					return false;
				}
				SetText(out.mPath, value);
				haveDescribe = true;
			}
			else if (std::strlen(value) < out.mStage.Capacity())
			{
				SetText(out.mStage, value);
			}
		}
		if (!haveDescribe) { out = DescribeRequest(); return true; }
		out.mActive = true;
		return true;
	}

	bool BeginDescribeFromArgs(int argc, const char* const argv[], const char* appName)
	{
		DescribeRequest request;
		if (!ParseDescribeArgs(argc, argv, request)) { return false; }
		if (!request.mActive) { return true; }
		SetText(request.mApp, appName);
#ifdef _DEBUG
		SetText(request.mConfig, "Debug");
#else
		SetText(request.mConfig, "Release");
#endif
		DescribeSession::Begin(request);
		return true;
	}

	namespace
	{
		// Fixed view for the overload that takes a service directly.
		class StaticSnapshot : public IDescribeSnapshotProvider
		{
		public:
			explicit StaticSnapshot(const DescribeView& view) : mView(view) {}
			bool Acquire(DescribeView& out) override { out = mView; return true; }
		private:
			DescribeView mView;
		};
	}

	DescribeStatus DescribeModeRunner::Tick(IDescribeBarrier& barrier, const ValueSchemaService& service,
		const CodeSideReport* codeSide, const ComponentFieldReport* components)
	{
		DescribeView view;
		view.mService = &service;
		view.mCodeSide = codeSide;
		view.mComponents = components;
		StaticSnapshot provider(view);
		return Tick(barrier, provider);
	}

	DescribeStatus DescribeModeRunner::Tick(IDescribeBarrier& barrier, IDescribeSnapshotProvider& provider)
	{
		if (!mRequest.mActive || IsFinished()) { return mStatus; }

		const BarrierState state = barrier.Poll();
		if (state == BarrierState::kWaiting) { return mStatus; }
		if (state == BarrierState::kAborted)
		{
			Fail("describe mode aborted: a module failed or the requested stage could not be entered", kExitAborted);
			return mStatus;
		}

		DescribeView view;
		if (!provider.Acquire(view)) { return mStatus; }
		if (view.mService == nullptr)
		{
			Fail("no schema snapshot to export", kExitAborted);
			return mStatus;
		}

		ExportHeader header;
		header.mApp = mRequest.mApp.AsCStr();
		header.mConfig = mRequest.mConfig.AsCStr();
		const ValueSchemaExporter exporter;
		const Json::Value doc = exporter.Build(*view.mService, view.mCodeSide, view.mComponents, header);
		if (!ValueSchemaExporter::WriteFile(mRequest.mPath.AsCStr(), doc))
		{
			Fail("could not write the schema export", kExitWriteFailed);
			return mStatus;
		}
		mStatus = DescribeStatus::kDone;
		mExitCode = kExitOk;
		DIA_LOG_INFO("VSC", "Describe mode: wrote %s", mRequest.mPath.AsCStr());
		return mStatus;
	}

	void DescribeModeRunner::Fail(const char* message, int exitCode)
	{
		if (IsFinished()) { return; }
		mStatus = DescribeStatus::kFailed;
		mExitCode = exitCode != 0 ? exitCode : kExitAborted;
		SetText(mMessage, message);
		DIA_LOG_ERROR("VSC", "Describe mode failed (exit %d): %s [%s]", mExitCode, mMessage.AsCStr(), mRequest.mPath.AsCStr());
	}

	void DescribeSession::Begin(const DescribeRequest& request)
	{
		sRequest = request;
		sComplete.store(false);
		sExitCode.store(0);
	}

	bool DescribeSession::IsActive()						{ return sRequest.mActive; }
	const DescribeRequest& DescribeSession::GetRequest()	{ return sRequest; }
	bool DescribeSession::IsComplete()						{ return sComplete.load(); }

	void DescribeSession::Complete(int exitCode)
	{
		sExitCode.store(exitCode);
		sComplete.store(true);
	}

	int DescribeSession::FinalExitCode()
	{
		if (!sRequest.mActive) { return 0; }
		if (sComplete.load()) { return sExitCode.load(); }
		std::printf("--describe-schema: the app exited before the export was written "
			"(is ValueSchemaModule declared in the manifest?)\n");
		return DescribeExit::kNotRun;
	}

	void DescribeSession::Reset()
	{
		sRequest = DescribeRequest();
		sComplete.store(false);
		sExitCode.store(0);
	}
}
