#include "DiaCore/Time/TimeAbsolute.h"

#include "DiaCore/Time/TimeRelative.h"
#include "DiaCore/Time/SystemClock.h"

namespace Dia
{
	namespace Core
	{
		TimeAbsolute::TimeAbsolute()									
		: TimeAbstract(0)
		{}
		
		TimeAbsolute::TimeAbsolute(long long time)
		: TimeAbstract(time)
		{}

		TimeAbsolute::TimeAbsolute(const TimeAbsolute& time)			
		: TimeAbstract(time)
		{}
				
		bool TimeAbsolute::operator<(const TimeAbsolute& time) const
		{
			return m_Time < time.m_Time;
		}
		
		bool TimeAbsolute::operator<=(const TimeAbsolute& time) const
		{
			return m_Time <= time.m_Time;
		}
		
		bool TimeAbsolute::operator==(const TimeAbsolute& time) const
		{
			return m_Time == time.m_Time;
		}
		
		bool TimeAbsolute::operator!=(const TimeAbsolute& time) const
		{
			return m_Time != time.m_Time;
		}
		
		bool TimeAbsolute::operator>=(const TimeAbsolute& time) const
		{
			return m_Time >= time.m_Time;
		}
		
		bool TimeAbsolute::operator>(const TimeAbsolute& time) const
		{
			return m_Time > time.m_Time;
		}
				
		//static 
		const TimeAbsolute TimeAbsolute::Zero()
		{
			return TimeAbsolute(TimeAbstract::ZeroVal());
		}
		
		//static 
		const TimeAbsolute TimeAbsolute::MaximumTime()
		{
			return TimeAbsolute(TimeAbstract::MaximumTimeVal());
		}
		
		//static 
		const TimeAbsolute TimeAbsolute::MinimumTime()
		{
			return TimeAbsolute(TimeAbstract::MinimumTimeVal());
		}
		
		//static 
		TimeAbsolute TimeAbsolute::CreateFromMicroseconds(const int microseconds)
		{
			return TimeAbsolute(TimeAbstract::FromMicroseconds(microseconds));
		}

		//static 
		TimeAbsolute TimeAbsolute::CreateFromMilliseconds(const int milliseconds)
		{
			return TimeAbsolute(TimeAbstract::FromMilliseconds(milliseconds));
		}
		
		//static 
		TimeAbsolute TimeAbsolute::CreateFromSeconds(const int seconds)
		{
			return TimeAbsolute(TimeAbstract::FromSeconds(seconds));
		}
		
		//static 
		TimeAbsolute TimeAbsolute::CreateFromMinutes(const int minutes)
		{
			return TimeAbsolute(TimeAbstract::FromMinutes(minutes));
		}
		
		//static 
		TimeAbsolute TimeAbsolute::CreateFromHours(const int hours)
		{
			return TimeAbsolute(TimeAbstract::FromHours(hours));
		}
		
		//static 
		TimeAbsolute TimeAbsolute::CreateFromDays(const int days)
		{
			return TimeAbsolute(TimeAbstract::FromDays(days));
		}
		
		//static 
		TimeAbsolute TimeAbsolute::CreateFromMicroseconds(const long long microseconds)
		{
			return TimeAbsolute(microseconds);
		}
		
		//static 
		TimeAbsolute TimeAbsolute::CreateFromMicroseconds(const float microseconds)
		{
			return TimeAbsolute(TimeAbstract::FromMicroseconds(microseconds));
		}

		//static 
		TimeAbsolute TimeAbsolute::CreateFromMilliseconds(const float milliseconds)
		{
			return TimeAbsolute(TimeAbstract::FromMilliseconds(milliseconds));
		}
		
		//static 
		TimeAbsolute TimeAbsolute::CreateFromSeconds(const float seconds)
		{
			return TimeAbsolute(TimeAbstract::FromSeconds(seconds));
		}
		
		//static 
		TimeAbsolute TimeAbsolute::CreateFromMinutes(const float minutes)
		{
			return TimeAbsolute(TimeAbstract::FromMinutes(minutes));
		}
		
		//static 
		TimeAbsolute TimeAbsolute::CreateFromHours(const float hours)
		{
			return TimeAbsolute(TimeAbstract::FromHours(hours));
		}
		
		//static 
		TimeAbsolute TimeAbsolute::CreateFromDays(const float days)
		{
			return TimeAbsolute(TimeAbstract::FromDays(days));
		}
		
		TimeAbsolute TimeAbsolute::AsPositiveTime() const
		{
			return TimeAbsolute(TimeAbstract::AsPositiveTime());
		}
			
		TimeAbsolute TimeAbsolute::operator +(const TimeRelative& rhs) const		
		{
			return TimeAbsolute(m_Time + rhs.m_Time);
		}
			
		TimeAbsolute TimeAbsolute::operator -(const TimeRelative& rhs) const		
		{
			return TimeAbsolute(m_Time - rhs.m_Time);
		}
			
		TimeRelative TimeAbsolute::operator -(const TimeAbsolute& rhs) const		
		{
			return TimeRelative(m_Time - rhs.m_Time);
		}
			
		TimeAbsolute& TimeAbsolute::operator +=(const TimeRelative& rhs)
		{
			m_Time += rhs.m_Time;
			return *this;
		}
			
		TimeAbsolute& TimeAbsolute::operator -=(const TimeRelative& rhs)
		{
			m_Time -= rhs.m_Time;
			return *this;
		}

		TimeAbsolute TimeAbsolute::GetSystemTime()
		{
			return sSystemClock.CurrentTime();
		}
	}
} 