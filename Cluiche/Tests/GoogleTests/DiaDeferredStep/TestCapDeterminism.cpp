#include <gtest/gtest.h>
#include <DiaDeferredStep/CapConfig.h>
#include <DiaDeferredStep/CapGate.h>
#include <DiaDeferredStep/CappedParallelExecutor.h>
#include <DiaParallelExecutor/SerialExecutor.h>
#include <DiaDeferredStep/Testing/ReorderingExecutor.h>
#include <DiaDeferredStep/Testing/ReferenceGatherStep.h>
#include <DiaThreading/JobSystem.h>
#include <chrono>
#include <future>
#include <memory>
#include <vector>

using namespace Dia::DeferredStep;
using Dia::DeferredStep::Testing::ReferenceGatherStep;
using Dia::DeferredStep::Testing::ReorderingExecutor;

namespace
{
	using Dia::Threading::JobSystem;

	const uint32_t kSeed = 0xC0FFEEu;
	const uint32_t kGridSizes[] = { 1, 7, 1000, 4099 };
	const uint32_t kRangeCounts[] = { 1, 3, 17 };
	const uint32_t kCaps[] = { 1, 2, 8 };
	const int      kIterations = 20;

	std::vector<uint8_t> ReferenceBytes(uint32_t cells)
	{
		ReferenceGatherStep step(cells, kSeed);
		step.Capture();
		Dia::ParallelExecutor::SerialExecutor serial;
		step.Run(serial);
		return step.GetResultBytes();
	}

	void RunAsCoordinator(CapGate& gate, std::function<void()> body)
	{
		auto promise = std::make_shared<std::promise<void>>();
		std::future<void> done = promise->get_future();
		GateItem item;
		item.Prepare(GateItem::Kind::kCoordinator, [promise, body]
		{
			try { body(); promise->set_value(); }
			catch (...) { promise->set_exception(std::current_exception()); }
		});
		gate.Admit(item);
		if (done.wait_for(std::chrono::seconds(10)) != std::future_status::ready)
		{
			ADD_FAILURE() << "coordinator did not finish within 10 s";
			std::exit(1);
		}
		gate.WaitItem(item);
		done.get();
	}

	CapConfig CapOf(uint32_t m)
	{
		CapConfig c;
		c.workerCap = m;
		c.rangesPerWorker = 4;
		return c;
	}
}

TEST(DeferredStepCapDeterminismTest, ReferenceIsStableAcrossSerialRuns)
{
	for (uint32_t cells : kGridSizes)
	{
		EXPECT_EQ(ReferenceBytes(cells), ReferenceBytes(cells)) << "cells=" << cells;
	}
}

TEST(DeferredStepCapDeterminismTest, CappedExecutorMatchesSerialAtEveryCapAndRangeCount)
{
	for (uint32_t m : kCaps)
	{
		JobSystem jobs;
		jobs.Initialize(8);
		CapGate gate(jobs, CapOf(m));
		for (uint32_t cells : kGridSizes)
		{
			const std::vector<uint8_t> ref = ReferenceBytes(cells);
			for (uint32_t forced : kRangeCounts)
			{
				for (int iter = 0; iter < kIterations; ++iter)
				{
					ReferenceGatherStep step(cells, kSeed);
					step.Capture();
					RunAsCoordinator(gate, [&]
					{
						CappedParallelExecutor exec(gate, LaunchInfo());
						exec.SetForcedRangeCount(forced);
						step.Run(exec);
					});
					ASSERT_EQ(step.GetResultBytes(), ref) << "m=" << m << " cells=" << cells << " ranges=" << forced << " iter=" << iter;
				}
			}
			for (int iter = 0; iter < kIterations; ++iter)
			{
				ReferenceGatherStep step(cells, kSeed);
				step.Capture();
				RunAsCoordinator(gate, [&]
				{
					CappedParallelExecutor exec(gate, LaunchInfo());
					step.Run(exec);
				});
				ASSERT_EQ(step.GetResultBytes(), ref) << "m=" << m << " cells=" << cells << " natural iter=" << iter;
			}
		}
		jobs.Shutdown();
	}
}

TEST(DeferredStepCapDeterminismTest, ReorderingExecutorMatchesSerial)
{
	for (uint32_t cells : kGridSizes)
	{
		const std::vector<uint8_t> ref = ReferenceBytes(cells);
		for (uint32_t forced : kRangeCounts)
		{
			ReferenceGatherStep step(cells, kSeed);
			step.Capture();
			ReorderingExecutor exec(forced);
			step.Run(exec);
			ASSERT_EQ(step.GetResultBytes(), ref) << "cells=" << cells << " ranges=" << forced;
		}
	}
}

TEST(DeferredStepCapDeterminismTest, ReorderingExecutorRunsRangesInReverse)
{
	ReorderingExecutor exec(4);
	std::vector<uint32_t> begins;
	exec.ParallelFor(100, 1, [&](uint32_t b, uint32_t) { begins.push_back(b); });
	ASSERT_EQ(begins.size(), 4u);
	EXPECT_GT(begins[0], begins[1]);
	EXPECT_GT(begins[1], begins[2]);
	EXPECT_GT(begins[2], begins[3]);
	EXPECT_EQ(exec.GetLastRangeCount(), 4u);
}

TEST(DeferredStepCapDeterminismTest, RangeCountDiffersButOutputIdentical)
{
	JobSystem jobs;
	jobs.Initialize(8);
	CapGate gate(jobs, CapOf(8));
	const uint32_t cells = 4099;
	const std::vector<uint8_t> ref = ReferenceBytes(cells);
	uint32_t seen[3] = {};
	for (int i = 0; i < 3; ++i)
	{
		ReferenceGatherStep step(cells, kSeed);
		step.Capture();
		RunAsCoordinator(gate, [&]
		{
			CappedParallelExecutor exec(gate, LaunchInfo());
			exec.SetForcedRangeCount(kRangeCounts[i]);
			step.Run(exec);
			seen[i] = exec.GetLastRangeCount();
		});
		EXPECT_EQ(step.GetResultBytes(), ref) << "ranges=" << kRangeCounts[i];
	}
	EXPECT_NE(seen[0], seen[1]);
	EXPECT_NE(seen[1], seen[2]);
	EXPECT_NE(seen[0], seen[2]);
	jobs.Shutdown();
}
