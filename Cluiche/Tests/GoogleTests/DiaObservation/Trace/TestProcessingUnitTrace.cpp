////////////////////////////////////////////////////////////////////////////////
// Filename: TestProcessingUnitTrace.cpp
// GoogleTest suite - a ProcessingUnit's dedicated thread registers its tracer
// span buffer so DIA_TRACE_ZONE on that thread reaches the trace file.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaObservation/Testing/UniqueTestPath.h>
#include <DiaObservation/Trace/Tracer.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaCore/CRC/StringCRC.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <thread>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using Dia::Observation::Trace::Tracer;

TEST(ProcessingUnitTrace, DedicatedThreadTraceZonesAreRecorded)
{
	const Dia::Observation::Testing::UniqueTestPath path("test_pu_trace.jsonl");
	Tracer::Instance().Stop();
	ASSERT_TRUE(Tracer::Instance().Start(path, Dia::Observation::Trace::Category::kAll, "pu-trace", 0));

	ProcessingUnit pu(StringCRC("tracesimpu"), 200.0f, true);
	std::thread t(std::ref(pu));
	std::this_thread::sleep_for(std::chrono::milliseconds(150));   // let it tick a few times
	pu.RequestStop();
	t.join();                                                       // thread exit unregisters
	Tracer::Instance().Stop();                                      // final drain

	std::ifstream f(path);
	const std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	std::remove(path);

	EXPECT_NE(content.find("\"name\":\"pu.update\""), std::string::npos)
		<< "pu.update trace zone from the PU thread must be recorded";
}
