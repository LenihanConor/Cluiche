#pragma once

// Test helper: a scripted ICodeSideSource. Ships with DiaValueSchema (Testing/ convention).
// ValueParam pointers must outlive the source (use static/local arrays).

#include <DiaValueSchema/CodeSideCollector.h>
#include <string>
#include <vector>

namespace Dia::ValueSchema::Testing
{
	class FakeCodeSideSource : public ICodeSideSource
	{
	public:
		struct Metric { std::string mName, mLabel, mDescription, mUnit; bool mHasInfo = false; };
		struct Command
		{
			std::string mName, mDescription, mCategory, mOwner, mExample;
			const Dia::API::ValueParam* mParams = nullptr;
			unsigned int mParamCount = 0;
			bool mNoArgs = false;
		};

		std::vector<Metric>		mMetrics;
		std::vector<Command>	mCommands;
		unsigned int			mMetricGen = 0;
		unsigned int			mCommandGen = 0;

		void AddMetric(const char* name)
		{
			Metric m; m.mName = name; mMetrics.push_back(m); ++mMetricGen;
		}
		void AddMetric(const char* name, const char* label, const char* description, const char* unit)
		{
			Metric m; m.mName = name; m.mLabel = label ? label : ""; m.mDescription = description ? description : "";
			m.mUnit = unit ? unit : ""; m.mHasInfo = true; mMetrics.push_back(m); ++mMetricGen;
		}
		void AddCommand(const char* name, const char* owner, const char* description, const Dia::API::ValueParam* params,
			unsigned int paramCount, bool noArgs = false, const char* category = "", const char* example = "")
		{
			Command c; c.mName = name; c.mOwner = owner; c.mDescription = description; c.mParams = params;
			c.mParamCount = paramCount; c.mNoArgs = noArgs; c.mCategory = category; c.mExample = example;
			mCommands.push_back(c); ++mCommandGen;
		}

		void ForEachMetric(MetricVisitor visitor, void* user) override
		{
			for (const Metric& m : mMetrics)
			{
				CodeSideMetric out;
				out.mName = m.mName.c_str();
				out.mHasInfo = m.mHasInfo;
				out.mLabel = m.mHasInfo ? m.mLabel.c_str() : nullptr;
				out.mDescription = m.mHasInfo ? m.mDescription.c_str() : nullptr;
				out.mUnit = m.mHasInfo ? m.mUnit.c_str() : nullptr;
				visitor(out, user);
			}
		}
		void ForEachCommand(CommandVisitor visitor, void* user) override
		{
			for (const Command& c : mCommands)
			{
				CodeSideCommand out;
				out.mName = c.mName.c_str();
				out.mDescription = c.mDescription.c_str();
				out.mCategory = c.mCategory.c_str();
				out.mOwner = c.mOwner.c_str();
				out.mExample = c.mExample.c_str();
				out.mParams = c.mParams;
				out.mParamCount = c.mParamCount;
				out.mNoArgs = c.mNoArgs;
				visitor(out, user);
			}
		}
		unsigned int MetricGeneration() override { return mMetricGen; }
		unsigned int CommandGeneration() override { return mCommandGen; }
	};
}
