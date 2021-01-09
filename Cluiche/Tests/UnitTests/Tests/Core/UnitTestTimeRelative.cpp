
#include "UnitTests/Tests/Core/UnitTestTimeRelative.h"

#include <limits.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>
#include <DiaMaths/Core/FloatMaths.h>

#include "UnitTests/Infrastructure/UnitTestMacros.h"
namespace UnitTests
{	
	UnitTestTimeRelative::UnitTestTimeRelative(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestTimeRelative::UnitTestTimeRelative(void)
		: UnitTestCore()
	{}

	void UnitTestTimeRelative::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			Dia::Core::TimeRelative time1(Dia::Core::TimeRelative::Zero());
			Dia::Core::TimeRelative time2(time1);	

			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1 == time2, "TimeRelative(const TimeRelative&)");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromDays(1);
			
			long long microSeconds = 24LL * 60L * 60LL * 1000LL * 1000LL;
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			time1.AsIntInMicroseconds();
			UNIT_TEST_ASSERT_EXPECTED_END();		
			
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 24 * 60 * 60 * 1000, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 24 * 60 * 60, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 24 * 60, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 24, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 1, "TimeRelative");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 24.0f * 60.0f * 60.0f * 1000.0f * 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 24.0f * 60.0f * 60.0f * 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 24.0f * 60.0f * 60.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMinutes() == 24.0f * 60.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInHours() == 24.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInDays() == 1.0f, "TimeRelative");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeRelative");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromHours(1);
			
			long long microSeconds = 60L * 60LL * 1000LL * 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 60 * 60 * 1000, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 60 * 60, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 60, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 1, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeRelative");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 60.0f * 60.0f * 1000.0f * 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 60.0f * 60.0f * 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 60.0f * 60.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMinutes() == 60.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInHours() == 1.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), (1.0f/24.0f)), "TimeRelative");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeRelative");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromMinutes(1);
			
			long long microSeconds = 60LL * 1000LL * 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMicroseconds() == microSeconds, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 60 * 1000, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 60, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 1, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeRelative");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 60.0f * 1000.0f * 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 60.0f * 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 60.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMinutes() == 1.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / 60.0f), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f)), "TimeRelative");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeRelative");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromSeconds(1);
			
			long long microSeconds = 1000LL * 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 1000, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 1, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeRelative");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 1000.0f * 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 1.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMinutes(), 1.0f / (60.0f)), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / (60.0f * 60.0f)), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f * 60.0f)), "TimeRelative");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeRelative");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromMilliseconds(1);
			
			long long microSeconds = 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMicroseconds() == microSeconds, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 1, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeRelative");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 1.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInSeconds(), 1.0f / 1000.0f), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMinutes(), 1.0f / (60.0f * 1000.0f)), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / (60.0f * 60.0f * 1000.0f)), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f * 60.0f * 1000.0f)), "TimeRelative");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeRelative");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
	
			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromMicroseconds(1);
			
			UNIT_TEST_POSITIVE(time1.AsIntInMicroseconds() == 1, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeRelative");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 1.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMilliseconds(), 1.0f / 1000.0f), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInSeconds(), 1.0f / (1000.0f * 1000.0f)), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMinutes(), 1.0f / (60.0f * 1000.0f * 1000.0f)), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / (60.0f * 60.0f * 1000.0f * 1000.0f)), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f * 60.0f * 1000.0f * 1000.0f)), "TimeRelative");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == 1, "TimeRelative");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromDays(1.0f);
			
			long long microSeconds = 24LL * 60L * 60LL * 1000LL * 1000LL;
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			time1.AsIntInMicroseconds();
			UNIT_TEST_ASSERT_EXPECTED_END();		
			
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 24 * 60 * 60 * 1000, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 24 * 60 * 60, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 24 * 60, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 24, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 1, "TimeRelative");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 24.0f * 60.0f * 60.0f * 1000.0f * 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 24.0f * 60.0f * 60.0f * 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 24.0f * 60.0f * 60.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMinutes() == 24.0f * 60.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInHours() == 24.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInDays() == 1.0f, "TimeRelative");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeRelative");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromHours(1.0f);
			
			long long microSeconds = 60L * 60LL * 1000LL * 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 60 * 60 * 1000, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 60 * 60, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 60, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 1, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeRelative");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 60.0f * 60.0f * 1000.0f * 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 60.0f * 60.0f * 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 60.0f * 60.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMinutes() == 60.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInHours() == 1.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), (1.0f/24.0f)), "TimeRelative");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeRelative");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromMinutes(1.0f);
			
			long long microSeconds = 60LL * 1000LL * 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMicroseconds() == microSeconds, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 60 * 1000, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 60, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 1, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeRelative");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 60.0f * 1000.0f * 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 60.0f * 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 60.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMinutes() == 1.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / 60.0f), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f)), "TimeRelative");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeRelative");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromSeconds(1.0f);
			
			long long microSeconds = 1000LL * 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 1000, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 1, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeRelative");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 1000.0f * 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds() == 1.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMinutes(), 1.0f / (60.0f)), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / (60.0f * 60.0f)), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f * 60.0f)), "TimeRelative");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeRelative");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromMilliseconds(1.0f);
			
			long long microSeconds = 1000LL;

			UNIT_TEST_POSITIVE(time1.AsIntInMicroseconds() == microSeconds, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 1, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeRelative");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 1000.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsFloatInMilliseconds() == 1.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInSeconds(), 1.0f / 1000.0f), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMinutes(), 1.0f / (60.0f * 1000.0f)), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / (60.0f * 60.0f * 1000.0f)), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f * 60.0f * 1000.0f)), "TimeRelative");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == microSeconds, "TimeRelative");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
	
			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromMicroseconds(1.0f);
			
			UNIT_TEST_POSITIVE(time1.AsIntInMicroseconds() == 1, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMilliseconds() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInSeconds() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInMinutes() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInHours() == 0, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeRelative");

			UNIT_TEST_POSITIVE(time1.AsFloatInMicroseconds() == 1.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMilliseconds(), 1.0f / 1000.0f), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInSeconds(), 1.0f / (1000.0f * 1000.0f)), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInMinutes(), 1.0f / (60.0f * 1000.0f * 1000.0f)), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInHours(), 1.0f / (60.0f * 60.0f * 1000.0f * 1000.0f)), "TimeRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(time1.AsFloatInDays(), 1.0f / (24.0f * 60.0f * 60.0f * 1000.0f * 1000.0f)), "TimeRelative");
			
			UNIT_TEST_POSITIVE(time1.AsLongLongInMicroseconds() == 1, "TimeRelative");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::Zero();			
			UNIT_TEST_POSITIVE(time1.AsIntInDays() == 0, "TimeRelative");
			
			Dia::Core::TimeRelative time2 = Dia::Core::TimeRelative::MaximumTime();
			UNIT_TEST_POSITIVE(time2.AsLongLongInMicroseconds() == LLONG_MAX, "TimeRelative");

			Dia::Core::TimeRelative time3 = Dia::Core::TimeRelative::MinimumTime();			
			UNIT_TEST_POSITIVE(time3.AsIntInDays() == -1, "TimeRelative");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
				
			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromMinutes(-2.0f);			
			
			UNIT_TEST_POSITIVE(time1.AsFloatInMinutes() == -2.0f, "TimeRelative");
			UNIT_TEST_POSITIVE(time1.AsPositiveTime().AsFloatInMinutes() == 2.0f, "TimeRelative");
			
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
		
			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromSeconds(30.0f);			
			Dia::Core::TimeRelative time2 = Dia::Core::TimeRelative::CreateFromSeconds(2.0f);			
			Dia::Core::TimeRelative time3 = Dia::Core::TimeRelative::CreateFromSeconds(2.0f);	

			UNIT_TEST_NEGATIVE(time1 < time2, "TimeRelative");
			UNIT_TEST_NEGATIVE(time2 < time3, "TimeRelative");
			UNIT_TEST_POSITIVE(time2 < time1, "TimeRelative");
			
			UNIT_TEST_NEGATIVE(time1 <= time2, "TimeRelative");
			UNIT_TEST_POSITIVE(time2 <= time3, "TimeRelative");
			UNIT_TEST_POSITIVE(time2 <= time1, "TimeRelative");

			UNIT_TEST_NEGATIVE(time1 == time2, "TimeRelative");
			UNIT_TEST_POSITIVE(time2 == time3, "TimeRelative");
			UNIT_TEST_NEGATIVE(time2 == time1, "TimeRelative");

			UNIT_TEST_POSITIVE(time1 != time2, "TimeRelative");
			UNIT_TEST_NEGATIVE(time2 != time3, "TimeRelative");
			UNIT_TEST_POSITIVE(time2 != time1, "TimeRelative");
			
			UNIT_TEST_POSITIVE(time1 >= time2, "TimeRelative");
			UNIT_TEST_POSITIVE(time2 >= time3, "TimeRelative");
			UNIT_TEST_NEGATIVE(time2 >= time1, "TimeRelative");

			UNIT_TEST_POSITIVE(time1 > time2, "TimeRelative");
			UNIT_TEST_NEGATIVE(time2 > time3, "TimeRelative");
			UNIT_TEST_NEGATIVE(time2 > time1, "TimeRelative");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
		
			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromSeconds(30.0f);			
			Dia::Core::TimeRelative time2 = Dia::Core::TimeRelative::CreateFromSeconds(2.0f);			
			
			Dia::Core::TimeRelative time3 = time1 + time2;
			UNIT_TEST_POSITIVE(time3.AsFloatInSeconds() == 32.0f, "TimeRelative");
			
			time3 = time1 - time2;
			UNIT_TEST_POSITIVE(time3.AsFloatInSeconds() == 28.0f, "TimeRelative");
	
			Dia::Core::TimeAbsolute time4 = Dia::Core::TimeAbsolute::CreateFromSeconds(2.0f);
			Dia::Core::TimeAbsolute time5 = time4 + time2;
			UNIT_TEST_POSITIVE(time5.AsFloatInSeconds() == 4.0f, "TimeRelative");

			time5 = time4 - time2;
			UNIT_TEST_POSITIVE(time5.AsFloatInSeconds() == 0.0f, "TimeRelative");

			Dia::Core::TimeAbsolute time6 = Dia::Core::TimeAbsolute::CreateFromSeconds(3.0f);
			time6 += time2;
			UNIT_TEST_POSITIVE(time6.AsFloatInSeconds() == 5.0f, "TimeRelative");
		
			time6 -= time2;
			UNIT_TEST_POSITIVE(time6.AsFloatInSeconds() == 3.0f, "TimeRelative");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
		
			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromSeconds(30.0f);				
			
			Dia::Core::TimeRelative time2 = time1 * 2;
			UNIT_TEST_POSITIVE(time2.AsFloatInSeconds()  == 60.0f, "TimeRelative");
			
			Dia::Core::TimeRelative time3 = time1 * 2.0f;
			UNIT_TEST_POSITIVE(time3.AsFloatInSeconds()  == 60.0f, "TimeRelative");
			
			Dia::Core::TimeRelative time4 = time1 / 2;
			UNIT_TEST_POSITIVE(time4.AsFloatInSeconds()  == 15.0f, "TimeRelative");

			Dia::Core::TimeRelative time5 = time1 / 2.0f;
			UNIT_TEST_POSITIVE(time5.AsFloatInSeconds()  == 15.0f, "TimeRelative");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
		
			Dia::Core::TimeRelative time1 = Dia::Core::TimeRelative::CreateFromSeconds(30.0f);				
			
			time1 *= 2;
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds()  == 60.0f, "TimeRelative");
			
			time1 *= 2.0f;
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds()  == 120.0f, "TimeRelative");
			
			time1 /= 2;
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds()  == 60.0f, "TimeRelative");

			time1 /= 2.0f;
			UNIT_TEST_POSITIVE(time1.AsFloatInSeconds()  == 30.0f, "TimeRelative");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}
