////////////////////////////////////////////////////////////////////////////////
// Filename: TestProcessingUnitProfile.cpp
// GoogleTest suite - a ProcessingUnit's dedicated thread registers its profiler
// scope buffer (named after the PU) so DIA_PROFILE_SCOPE on that thread is recorded.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaObservation/Profile/Profiler.h>
#include <DiaObservation/Testing/ProfileFixture.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaCore/CRC/StringCRC.h>

#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using Dia::Observation::Profile::Profiler;

TEST(ProcessingUnitProfile, DedicatedThreadScopesAreRecordedAndThreadIsNamed)
{
	Dia::Observation::Testing::ProfileFixture fixture;

	ProcessingUnit pu(StringCRC("profsimpu"), 200.0f, true);
	std::thread t(std::ref(pu));
	std::this_thread::sleep_for(std::chrono::milliseconds(100));   // let it tick a few times

	Profiler::Instance().BeginFrame();                              // trigger a drain of closed scopes
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	pu.RequestStop();
	t.join();                                                       // thread exit unregisters + flushes
	Profiler::Instance().Stop();

	std::ifstream f(fixture.mJsonlPath);
	const std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

	EXPECT_NE(content.find(std::to_string(StringCRC("pu.update").Value())), std::string::npos)
		<< "pu.update scope from the PU thread must be recorded";
	EXPECT_NE(content.find("\"record_type\":\"thread_name\""), std::string::npos);
	EXPECT_NE(content.find("\"name\":\"profsimpu\""), std::string::npos)
		<< "PU thread must be named after the PU";
}
