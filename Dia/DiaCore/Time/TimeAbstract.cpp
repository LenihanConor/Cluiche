
#include "DiaCore/Time/TimeAbstract.h"

#include <math.h>
#include <limits.h>
#include <float.h>

#include "DiaCore/Core/Assert.h"

#pragma warning( disable : 4244 )

namespace Dia
{
	namespace Core 
	{
		
		const int TimeAbstract::intFromMicroToMilliseconds		= 1000 ;
		const int TimeAbstract::intFromMicroToSeconds			= 1000 * 1000;
		const int TimeAbstract::intFromMicroToMinutes			= 1000 * 1000 * 60;
		const long long TimeAbstract::intFromMicroToHours		= static_cast<long long>(1000) * static_cast<long long>(1000) * static_cast<long long>(60 * 60);
		const long long  TimeAbstract::intFromMicroToDays		= static_cast<long long >(1000) * static_cast<long long >(1000) * static_cast<long long >(60 * 60 * 24);

		const float TimeAbstract::floatFromMicroToMilliseconds	= 1000.0f;
		const float TimeAbstract::floatFromMicroToSeconds		= 1000.0f * 1000.0f;
		const float TimeAbstract::floatFromMicroToMinutes		= 1000.0f * 1000.0f * 60.0f;
		const float TimeAbstract::floatFromMicroToHours		= 1000.0f * 1000.0f * 60.0f * 60.0f;
		const float TimeAbstract::floatFromMicroToDays			= 1000.0f * 1000.0f * 60.0f * 60.0f * 24.0f;

		//----------------------------------------------------------------------------------------
		TimeAbstract::TimeAbstract(long long time)
			: m_Time(time)
		{}
		
		//----------------------------------------------------------------------------------------
		TimeAbstract::TimeAbstract(const TimeAbstract& xRef)
			: m_Time(xRef.m_Time)
		{}
		
		//----------------------------------------------------------------------------------------
		TimeAbstract& TimeAbstract::operator=(const TimeAbstract& time)
		{
			m_Time = time.m_Time;
			return *this;
		}
		
		//----------------------------------------------------------------------------------------
		long long TimeAbstract::ZeroVal()
		{
			return int(0);
		}
		
		//----------------------------------------------------------------------------------------
		long long TimeAbstract::MaximumTimeVal()
		{
			return static_cast<long long>(0xFFFFFFFFFFFFFFFF >> 1);
		}
		
		long long TimeAbstract::MinimumTimeVal()
		{	
			return FromDays(-1);
		} 
		
		//----------------------------------------------------------------------------------------
		int TimeAbstract::AsIntInMicroseconds() const
		{
			DIA_ASSERT(m_Time >= INT_MIN && m_Time <= INT_MAX, "m_Time out bounding an int");
			
			return static_cast<int>(m_Time);
		}

		//----------------------------------------------------------------------------------------
		int TimeAbstract::AsIntInMilliseconds() const
		{
			long long result = m_Time / intFromMicroToMilliseconds;
			
			DIA_ASSERT(result >= INT_MIN && result <= INT_MAX, "m_Time out bounding an int");

			return static_cast<int>(result);
		}
		
		//----------------------------------------------------------------------------------------
		int TimeAbstract::AsIntInSeconds() const
		{
			long long result = m_Time / intFromMicroToSeconds;
			
			DIA_ASSERT(result >= INT_MIN && result <= INT_MAX, "m_Time out bounding an int");

			return static_cast<int>(result);
		}
		
		//----------------------------------------------------------------------------------------
		int TimeAbstract::AsIntInMinutes() const
		{
			long long result = m_Time / intFromMicroToMinutes;
			
			DIA_ASSERT(result >= INT_MIN && result <= INT_MAX, "m_Time out bounding an int");

			return static_cast<int>(result);
		}
		
		//----------------------------------------------------------------------------------------
		int TimeAbstract::AsIntInHours() const
		{
			long long result = m_Time / intFromMicroToHours;
			
			DIA_ASSERT(result >= INT_MIN && result <= INT_MAX, "m_Time out bounding an int");

			return static_cast<int>(result);
		}
		
		//----------------------------------------------------------------------------------------
		int TimeAbstract::AsIntInDays() const
		{
			long long result = m_Time / intFromMicroToDays;
			
			DIA_ASSERT(result >= INT_MIN && result <= INT_MAX, "m_Time out bounding an int");

			return static_cast<int>(result);
		}
				
		//----------------------------------------------------------------------------------------
		long long TimeAbstract::AsLongLongInMicroseconds() const
		{
			return m_Time;
		}
		
		//----------------------------------------------------------------------------------------
		float TimeAbstract::AsFloatInMicroseconds() const
		{
			DIA_ASSERT(m_Time >= -FLT_MAX && m_Time <= FLT_MAX, "m_Time out bounding an int");

			return static_cast<float>(m_Time);
		}
		
		//----------------------------------------------------------------------------------------
		float TimeAbstract::AsFloatInMilliseconds() const
		{
			float result = static_cast<float>(m_Time) / floatFromMicroToMilliseconds;
			
			DIA_ASSERT(result >= -FLT_MAX && result <= FLT_MAX, "m_Time out bounding an float");

			return result;
		}
		
		//----------------------------------------------------------------------------------------
		float TimeAbstract::AsFloatInSeconds() const
		{
			float result = static_cast<float>(m_Time) / floatFromMicroToSeconds;
			
			DIA_ASSERT(result >= -FLT_MAX && result <= FLT_MAX, "m_Time out bounding an float");

			return result;
		}
		
		//----------------------------------------------------------------------------------------
		float TimeAbstract::AsFloatInMinutes() const
		{
			float result = static_cast<float>(m_Time) / floatFromMicroToMinutes;
			
			DIA_ASSERT(result >= -FLT_MAX && result <= FLT_MAX, "m_Time out bounding an float");

			return result;
		}
		
		//----------------------------------------------------------------------------------------
		float TimeAbstract::AsFloatInHours() const
		{
			float result = static_cast<float>(m_Time) / floatFromMicroToHours;
			
			DIA_ASSERT(result >= -FLT_MAX && result <= FLT_MAX, "m_Time out bounding an float");

			return result;
		}
		
		//----------------------------------------------------------------------------------------
		float TimeAbstract::AsFloatInDays() const
		{
			float result = static_cast<float>(m_Time) / floatFromMicroToDays;
			
			DIA_ASSERT(result >= -FLT_MAX && result <= FLT_MAX, "m_Time out bounding an float");

			return result;
		}
			
		//----------------------------------------------------------------------------------------
		long long TimeAbstract::FromMicroseconds(const int microseconds)
		{
			return static_cast<int>(microseconds);
		}

		//----------------------------------------------------------------------------------------
		long long TimeAbstract::FromMilliseconds(const int milliseconds)
		{
			return static_cast<long long>(milliseconds) * intFromMicroToMilliseconds;
		}
		
		//----------------------------------------------------------------------------------------
		long long TimeAbstract::FromSeconds(const int seconds)
		{
			return static_cast<long long>(seconds) * intFromMicroToSeconds;
		}
		
		//----------------------------------------------------------------------------------------
		long long TimeAbstract::FromMinutes(const int minutes)
		{
			return static_cast<long long>(minutes) * intFromMicroToMinutes;
		}
		
		//----------------------------------------------------------------------------------------
		long long TimeAbstract::FromHours(const int hours)
		{
			return static_cast<long long>(hours) * intFromMicroToHours;
		}
		
		//----------------------------------------------------------------------------------------
		long long TimeAbstract::FromDays(const int days)
		{
			return static_cast<long long>(days) * intFromMicroToDays;
		}
		
		//----------------------------------------------------------------------------------------
		long long TimeAbstract::FromMicroseconds(const float milliseconds)
		{
			return static_cast<long long>(milliseconds);
		}

		//----------------------------------------------------------------------------------------
		long long TimeAbstract::FromMilliseconds(const float milliseconds)
		{
			return static_cast<long long>(milliseconds * floatFromMicroToMilliseconds);
		}
		
		//----------------------------------------------------------------------------------------
		long long TimeAbstract::FromSeconds(const float seconds)
		{
			return static_cast<long long>(seconds * floatFromMicroToSeconds);
		}
		
		//----------------------------------------------------------------------------------------
		long long TimeAbstract::FromMinutes(const float minutes)
		{
			return static_cast<long long>(minutes * floatFromMicroToMinutes);
		}
		
		//----------------------------------------------------------------------------------------
		long long TimeAbstract::FromHours(const float hours)
		{
			return static_cast<long long>(hours * floatFromMicroToHours);
		}
		
		//----------------------------------------------------------------------------------------
		long long TimeAbstract::FromDays(const float days)
		{
			return static_cast<long long>(days * floatFromMicroToDays);
		}
				
		//----------------------------------------------------------------------------------------
		long long TimeAbstract::AsPositiveTime() const
		{
			return m_Time > 0 ? m_Time : -m_Time;
		}
	}	
} // namespace Axiom

