
#include "UnitTests/Tests/Core/UnitTestTimeAbsolute.h"

#include <limits.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>
#include <DiaMaths/Core/FloatMaths.h>

#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestTimeAbsolute::UnitTestTimeAbsolute(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestTimeAbsolute::UnitTestTimeAbsolute(void)
		: UnitTestCore()
	{}

	void UnitTestTimeAbsolute::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			Dia::Core::TimeAbsolute time1(Dia::Core::TimeAbsolute::Zero());
			Dia::Core::TimeAbsolute time2(time1);	

			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1 == time2, "TimeAbsolute(const TimeAbsolute&)");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::CreateFromDays(1);
			
			long long microSeconds = 24LL * 60L * 60LL * 1000LL * 1000LL;
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			time1.AsIntInMicroseconds();
			UNIT_TEST_ASSERT_EXPECTED_END();		
			
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 24 * 60 * 60 * 1000, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 24 * 60 * 60, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 24 * 60, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 24, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 1, "TimeAbsolute");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 24.0f * 60.0f * 60.0f * 1000.0f * 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 24.0f * 60.0f * 60.0f * 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 24.0f * 60.0f * 60.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMinutes() == 24.0f * 60.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInHours() == 24.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInDays() == 1.0f, "TimeAbsolute");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeAbsolute");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::CreateFromHours(1);
			
			long long microSeconds = 60L * 60LL * 1000LL * 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 60 * 60 * 1000, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 60 * 60, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 60, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 1, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeAbsolute");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 60.0f * 60.0f * 1000.0f * 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 60.0f * 60.0f * 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 60.0f * 60.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMinutes() == 60.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInHours() == 1.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), (1.0f/24.0f)), "TimeAbsolute");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeAbsolute");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::CreateFromMinutes(1);
			
			long long microSeconds = 60LL * 1000LL * 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMicroseconds() == microSeconds, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 60 * 1000, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 60, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 1, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeAbsolute");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 60.0f * 1000.0f * 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 60.0f * 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 60.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMinutes() == 1.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / 60.0f), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f)), "TimeAbsolute");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeAbsolute");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::CreateFromSeconds(1);
			
			long long microSeconds = 1000LL * 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 1000, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 1, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeAbsolute");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 1000.0f * 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 1.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMinutes(), 1.0f / (60.0f)), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / (60.0f * 60.0f)), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f * 60.0f)), "TimeAbsolute");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeAbsolute");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::CreateFromMilliseconds(1);
			
			long long microSeconds = 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMicroseconds() == microSeconds, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 1, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeAbsolute");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 1.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInSeconds(), 1.0f / 1000.0f), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMinutes(), 1.0f / (60.0f * 1000.0f)), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / (60.0f * 60.0f * 1000.0f)), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f * 60.0f * 1000.0f)), "TimeAbsolute");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeAbsolute");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
	
			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::CreateFromMicroseconds(1);
			
			UNIT_TEST_POSITIVE(time1.AsIntInMicroseconds() == 1, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeAbsolute");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 1.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMilliseconds(), 1.0f / 1000.0f), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInSeconds(), 1.0f / (1000.0f * 1000.0f)), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMinutes(), 1.0f / (60.0f * 1000.0f * 1000.0f)), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / (60.0f * 60.0f * 1000.0f * 1000.0f)), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f * 60.0f * 1000.0f * 1000.0f)), "TimeAbsolute");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == 1, "TimeAbsolute");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::CreateFromDays(1.0f);
			
			long long microSeconds = 24LL * 60L * 60LL * 1000LL * 1000LL;
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			time1.AsIntInMicroseconds();
			UNIT_TEST_ASSERT_EXPECTED_END();		
			
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 24 * 60 * 60 * 1000, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 24 * 60 * 60, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 24 * 60, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 24, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 1, "TimeAbsolute");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 24.0f * 60.0f * 60.0f * 1000.0f * 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 24.0f * 60.0f * 60.0f * 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 24.0f * 60.0f * 60.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMinutes() == 24.0f * 60.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInHours() == 24.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInDays() == 1.0f, "TimeAbsolute");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeAbsolute");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::CreateFromHours(1.0f);
			
			long long microSeconds = 60L * 60LL * 1000LL * 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 60 * 60 * 1000, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 60 * 60, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 60, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 1, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeAbsolute");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 60.0f * 60.0f * 1000.0f * 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 60.0f * 60.0f * 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 60.0f * 60.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMinutes() == 60.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInHours() == 1.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), (1.0f/24.0f)), "TimeAbsolute");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeAbsolute");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::CreateFromMinutes(1.0f);
			
			long long microSeconds = 60LL * 1000LL * 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMicroseconds() == microSeconds, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 60 * 1000, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 60, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 1, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeAbsolute");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 60.0f * 1000.0f * 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 60.0f * 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 60.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMinutes() == 1.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / 60.0f), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f)), "TimeAbsolute");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeAbsolute");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::CreateFromSeconds(1.0f);
			
			long long microSeconds = 1000LL * 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 1000, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 1, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeAbsolute");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 1000.0f * 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 1.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMinutes(), 1.0f / (60.0f)), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / (60.0f * 60.0f)), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f * 60.0f)), "TimeAbsolute");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeAbsolute");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::CreateFromMilliseconds(1.0f);
			
			long long microSeconds = 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMicroseconds() == microSeconds, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 1, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeAbsolute");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 1000.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 1.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInSeconds(), 1.0f / 1000.0f), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMinutes(), 1.0f / (60.0f * 1000.0f)), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / (60.0f * 60.0f * 1000.0f)), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f * 60.0f * 1000.0f)), "TimeAbsolute");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeAbsolute");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
	
			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::CreateFromMicroseconds(1.0f);
			
			UNIT_TEST_POSITIVE(time1.AsIntInMicroseconds() == 1, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeAbsolute");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 1.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMilliseconds(), 1.0f / 1000.0f), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInSeconds(), 1.0f / (1000.0f * 1000.0f)), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMinutes(), 1.0f / (60.0f * 1000.0f * 1000.0f)), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / (60.0f * 60.0f * 1000.0f * 1000.0f)), "TimeAbsolute");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f * 60.0f * 1000.0f * 1000.0f)), "TimeAbsolute");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == 1, "TimeAbsolute");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::Zero();			
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeAbsolute");
			
			Dia::Core::TimeAbsolute time2 = Dia::Core::TimeAbsolute::MaximumTime();
			UNIT_TEST_POSITIVE(time2.AsLongLongInMicroseconds() == LLONG_MAX, "TimeAbsolute");

			Dia::Core::TimeAbsolute time3 = Dia::Core::TimeAbsolute::MinimumTime();			
			UNIT_TEST_POSITIVE(time3.AsIntInDays() == -1, "TimeAbsolute");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
		
			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromSeconds(30.0f);			
			Dia::Core::TimeAbsolute time2 = Dia::Core::TimeAbsolute::CreateFromMinutes(2.0f);			
			
			Dia::Core::TimeAbsolute time3 = time1 + time2;
			UNIT_TEST_POSITIVE(time3.AsFloatInSeconds() == 150.0f, "TimeAbsolute");

			Dia::Core::TimeAbsolute time4 = time2 - time1;
			Dia::Core::TimeAbsolute time5 = time1 - time2;
			UNIT_TEST_POSITIVE(time4.AsFloatInSeconds() == 90.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time5.AsFloatInSeconds() == -90.0f, "TimeAbsolute");
			
			Dia::Core::TimeAbsolute time6 = time2 - time1;
			UNIT_TEST_POSITIVE(time6.AsFloatInSeconds() == 90.0f, "TimeAbsolute");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
		
			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::CreateFromSeconds(30.0f);			
			Dia::Core::TimeAbsolute time2 = Dia::Core::TimeAbsolute::CreateFromSeconds(2.0f);			
			Dia::Core::TimeAbsolute time3 = Dia::Core::TimeAbsolute::CreateFromSeconds(2.0f);	
			Dia::Core::TimeRelative time4 = Dia::Core::TimeRelative::CreateFromSeconds(2.0f);

			time1 += time4;
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 32.0f, "TimeAbsolute");

			time1 -= time4;
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 30.0f, "TimeAbsolute");

			UNIT_TEST_NEGATIVE(time1 < time2, "TimeAbsolute");
			UNIT_TEST_NEGATIVE(time2 < time3, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time2 < time1, "TimeAbsolute");
			
			UNIT_TEST_NEGATIVE(time1 <= time2, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time2 <= time3, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time2 <= time1, "TimeAbsolute");

			UNIT_TEST_NEGATIVE(time1 == time2, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time2 == time3, "TimeAbsolute");
			UNIT_TEST_NEGATIVE(time2 == time1, "TimeAbsolute");

			UNIT_TEST_POSITIVE(time1 != time2, "TimeAbsolute");
			UNIT_TEST_NEGATIVE(time2 != time3, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time2 != time1, "TimeAbsolute");
			
			UNIT_TEST_POSITIVE(time1 >= time2, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time2 >= time3, "TimeAbsolute");
			UNIT_TEST_NEGATIVE(time2 >= time1, "TimeAbsolute");

			UNIT_TEST_POSITIVE(time1 > time2, "TimeAbsolute");
			UNIT_TEST_NEGATIVE(time2 > time3, "TimeAbsolute");
			UNIT_TEST_NEGATIVE(time2 > time1, "TimeAbsolute");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
				
			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::CreateFromMinutes(-2.0f);			
			
			UNIT_TEST_POSITIVE(time1.AsFloatInMinutes() == -2.0f, "TimeAbsolute");
			UNIT_TEST_POSITIVE(time1.AsPositiveTime().AsFloatInMinutes() == 2.0f, "TimeAbsolute");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
				
			Dia::Core::TimeAbsolute time1 = Dia::Core::TimeAbsolute::GetSystemTime();			
			
		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}
