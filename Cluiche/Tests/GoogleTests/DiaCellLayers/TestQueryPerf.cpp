#include <gtest/gtest.h>
#include <DiaCellLayers/QueryEvaluator.h>
#include <DiaCellLayers/QueryParser.h>
#include <DiaCellLayers/QueryResolver.h>
#include <DiaCellLayers/Testing/QueryTestSupport.h>

#include <chrono>
#include <stdio.h>
#include <string>
#include <vector>

// Query performance: Measure ExecuteQuery on a large grid (6,656 cells = 104x64) with a complex query
// (6 comparisons across 4 float layers joined with and/or/not). Assert it completes within a frame budget:
// average over 50 runs < 2 ms in Release (< 8 ms in Debug).
//
// The test measures QueryEvaluator::Evaluate directly (the core path that both cells.select.query
// and cells.query.check use).

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;

namespace
{
	const uint16_t kW = 104;
	const uint16_t kH = 64;
	const uint32_t kCells = uint32_t(kW) * kH;	// 6,656
	const uint32_t kRuns = 50;
	const double kBoundMs = 2.0;		// Release (NDEBUG)
	const double kBoundMsDebug = 8.0;	// Debug
}

TEST(QueryPerfTest, EvaluateComplexQueryOnLargeGrid)
{
	// Set up a large grid with 4 float layers.
	QueryEvalFixture fix(kW, kH);
	fix.AddValues("a", std::vector<float>(kCells, 0.5f));
	fix.AddValues("b", std::vector<float>(kCells, 0.3f));
	fix.AddValues("c", std::vector<float>(kCells, 0.7f));
	fix.AddValues("d", std::vector<float>(kCells, 0.2f));
	fix.PublishSchema({ "a", "b", "c", "d" });

	// Complex query with 6 comparisons across 4 layers, joined with and/or/not.
	// This exercises the evaluator's boolean logic and layer reads.
	const char* expr = "(a > 0.4 and b < 0.5) or (c >= 0.6 and not d > 0.3)";

	// Warm up: parse and resolve once (not timed).
	ASSERT_TRUE(fix.q.Run(expr, fix.parser, fix.resolver)) << fix.Msg();

	// Measure 50 evaluations.
	double totalMs = 0.0;
	for (uint32_t i = 0; i < kRuns; ++i)
	{
		const auto t0 = std::chrono::steady_clock::now();
		ASSERT_TRUE(fix.evaluator.Evaluate(fix.parser, fix.resolver, fix.Reader(), kCells))
			<< "Evaluation failed: " << fix.evaluator.Error().mMessage;
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		totalMs += ms;
	}
	const double avgMs = totalMs / kRuns;

	printf("[ perf ] evaluate query over %u cells (%ux%u) 50 runs: avg %.3f ms (total %.1f ms)\n", kCells, kW, kH, avgMs, totalMs);

#ifdef NDEBUG
	EXPECT_LE(avgMs, kBoundMs);
#else
	SCOPED_TRACE("Debug build: asserting < " + std::to_string(kBoundMsDebug) + " ms; measured " + std::to_string(avgMs) + " ms");
	EXPECT_LE(avgMs, kBoundMsDebug);
#endif
}

// Spec open design question 1: cells.query.check runs the full evaluation on every (debounced) keystroke. Time it on a grid of 20
// layers, querying six comparisons spread across them.
TEST(QueryPerfTest, CheckOnTwentyLayerLargeGridStaysWithinFrameBudget)
{
	QueryEvalFixture fix(kW, kH);
	std::vector<std::string> names;
	for (int i = 0; i < 20; ++i) { names.push_back("l" + std::to_string(i)); }
	fix.q.fx.Add(UploaderSchemaFixture::FloatEntry("l0", 0, 1));
	for (int i = 1; i < 20; ++i) { fix.q.fx.Add(UploaderSchemaFixture::FloatEntry(names[i].c_str(), 0, 1)); }
	fix.q.Publish();
	for (int i = 0; i < 20; ++i) { ASSERT_TRUE(fix.AddValues(names[i].c_str(), std::vector<float>(kCells, 0.05f * i))); }

	const char* expr = "(l1 > 0.04 and l5 < 0.5) or (l9 >= 0.4 and not l13 > 0.9) or (l17 > 0.8 and l19 > 0.9)";
	ASSERT_TRUE(fix.q.Run(expr, fix.parser, fix.resolver)) << fix.Msg();
	double totalMs = 0.0;
	for (uint32_t i = 0; i < kRuns; ++i)
	{
		const auto t0 = std::chrono::steady_clock::now();
		ASSERT_TRUE(fix.evaluator.Evaluate(fix.parser, fix.resolver, fix.Reader(), kCells)) << fix.evaluator.Error().mMessage;
		totalMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	}
	const double avgMs = totalMs / kRuns;
	printf("[ perf ] 20-layer grid, 6 comparisons over %u cells, 50 runs: avg %.3f ms\n", kCells, avgMs);
	EXPECT_GT(fix.evaluator.Count(), 0u);
#ifdef NDEBUG
	EXPECT_LE(avgMs, kBoundMs);
#else
	EXPECT_LE(avgMs, kBoundMsDebug);
#endif
}
