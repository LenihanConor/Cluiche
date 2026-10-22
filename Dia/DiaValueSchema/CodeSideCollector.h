#pragma once

// CodeSideCollector: turns code-side registrations (MetricRegistry metrics with MetricInfo, DiaAPI commands
// with ValueParam descriptions) into ValueInfo entries in a ValueSchemaRegistry (pre-Publish only).
// Public header is STL-free. Tests drive it through ICodeSideSource (Testing/FakeCodeSideSource.h);
// production uses LiveCodeSideSource.
//
// Mapping:
//   metric      : for "dia.metric",  key = metric name,      kind Metric,  type Float
//   command     : for "dia.command", key = command name,     kind Command, type String;
//                 mUi.owner / mUi.category / mUi.example, mRelated = ordered "<command>.<arg>" keys.
//                 ONE entry per registered command, noArgs ones included.
//   command arg : for "dia.command", key = "<command>.<arg>", kind Arg
// Entries whose text exceeds ValueInfo capacities are rejected (Error log, counted), never truncated.

#include <DiaValueSchema/ValueInfo.h>
#include <DiaAPI/CommandRegistry/ValueParam.h>

namespace Dia::ValueSchema
{
	class ValueSchemaRegistry;

	struct CodeSideMetric
	{
		const char*	mName = nullptr;
		bool		mHasInfo = false;
		const char*	mLabel = nullptr;
		const char*	mDescription = nullptr;
		const char*	mUnit = nullptr;
	};

	struct CodeSideCommand
	{
		const char*							mName = nullptr;
		const char*							mDescription = nullptr;
		const char*							mCategory = nullptr;
		const char*							mOwner = nullptr;
		const char*							mExample = nullptr;
		const Dia::API::ValueParam*			mParams = nullptr;
		unsigned int						mParamCount = 0;
		bool								mNoArgs = false;
	};

	class ICodeSideSource
	{
	public:
		typedef void (*MetricVisitor)(const CodeSideMetric& metric, void* user);
		typedef void (*CommandVisitor)(const CodeSideCommand& command, void* user);

		virtual ~ICodeSideSource() = default;
		virtual void ForEachMetric(MetricVisitor visitor, void* user) = 0;
		// Plain commands first, then JSON commands.
		virtual void ForEachCommand(CommandVisitor visitor, void* user) = 0;
		virtual unsigned int MetricGeneration() = 0;
		virtual unsigned int CommandGeneration() = 0;
	};

	// Wraps MetricRegistry::Instance() and the DiaAPI command registry.
	class LiveCodeSideSource : public ICodeSideSource
	{
	public:
		void ForEachMetric(MetricVisitor visitor, void* user) override;
		void ForEachCommand(CommandVisitor visitor, void* user) override;
		unsigned int MetricGeneration() override;
		unsigned int CommandGeneration() override;
	};

	struct UndescribedItem
	{
		Dia::Core::Containers::String128	mName;
		Dia::Core::Containers::String64		mOwner;
	};

	struct CodeSideReport
	{
		unsigned int	mMetricsCollected = 0;			// metric entries registered
		unsigned int	mMetricsDescribed = 0;			// of those, with a non-empty description
		unsigned int	mMetricsUndescribed = 0;
		unsigned int	mCommandsCollected = 0;			// command entries registered
		unsigned int	mArgsCollected = 0;				// arg entries registered
		unsigned int	mArgsDescribed = 0;				// of those, with a description
		unsigned int	mArgsUndescribed = 0;
		unsigned int	mWarningCount = 0;
		unsigned int	mRejectedCount = 0;				// capacity / registry rejections (not duplicates)
		unsigned int	mDuplicateCount = 0;			// DuplicateKey results (first wins)
		unsigned int	mMetricGeneration = 0;			// source generations at collection time
		unsigned int	mCommandGeneration = 0;

		ValueList<UndescribedItem>	mUndescribedMetrics;
		ValueList<UndescribedItem>	mUndescribedCommands;	// neither params nor noArgs
		ValueList<UndescribedItem>	mUndescribedParams;		// name = "<command>.<arg>"
	};

	class CodeSideCollector
	{
	public:
		static constexpr const char* kMetricFor = "dia.metric";
		static constexpr const char* kCommandFor = "dia.command";

		CodeSideReport Collect(ICodeSideSource& source, ValueSchemaRegistry& registry);
	};
}
