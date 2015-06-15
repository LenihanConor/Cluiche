#include "DiaCore/Time/TimeRelative.h"

#include "DiaCore/Time/TimeAbsolute.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		//-----------------------------------------------------------------------
		TimeRelative::TimeRelative(long long time)
			: TimeAbstract(time)
		{}
		
		//-----------------------------------------------------------------------
		TimeRelative::TimeRelative()
			: TimeAbstract(0)
		{}
		
		//-----------------------------------------------------------------------
		TimeRelative::TimeRelative(const TimeRelative& time)			        
			: TimeAbstract(time.m_Time)
		{}
		
		//-----------------------------------------------------------------------
		bool TimeRelative::operator<(const TimeRelative& time) const
		{
			return m_Time < time.m_Time;
		}
		
		//-----------------------------------------------------------------------
		bool TimeRelative::operator<=(const TimeRelative& time) const
		{
			return m_Time <= time.m_Time;
		}
		
		//-----------------------------------------------------------------------
		bool TimeRelative::operator==(const TimeRelative& time) const
		{
			return m_Time == time.m_Time;
		}
		
		//-----------------------------------------------------------------------
		bool TimeRelative::operator!=(const TimeRelative& time) const
		{
			return m_Time != time.m_Time;
		}
		
		//-----------------------------------------------------------------------
		bool TimeRelative::operator>=(const TimeRelative& time) const
		{
			return m_Time >= time.m_Time;
		}
		
		//-----------------------------------------------------------------------
		bool TimeRelative::operator>(const TimeRelative& time) const
		{
			return m_Time > time.m_Time;
		}
				
		//-----------------------------------------------------------------------
		const TimeRelative TimeRelative::Zero()
		{
			return TimeRelative(static_cast<long long>(TimeAbstract::ZeroVal()));
		}
		
		//-----------------------------------------------------------------------
		const TimeRelative TimeRelative::MaximumTime()
		{
			return TimeRelative(static_cast<long long>(TimeAbstract::MaximumTimeVal()));
		}
		
		//-----------------------------------------------------------------------
		const TimeRelative TimeRelative::MinimumTime()
		{
			return TimeRelative(static_cast<long long>(TimeAbstract::MinimumTimeVal()));
		}
		
		//-----------------------------------------------------------------------
		TimeRelative TimeRelative::CreateFromMilliseconds(const int milliseconds)
		{
			return TimeRelative(static_cast<long long>(TimeAbstract::FromMilliseconds(milliseconds)));
		}
		
		//----------------------------------------------------------------------- 
		TimeRelative TimeRelative::CreateFromSeconds(const int seconds)
		{
			return TimeRelative(static_cast<long long>(TimeAbstract::FromSeconds(seconds)));
		}
		
		//-----------------------------------------------------------------------
		TimeRelative TimeRelative::CreateFromMinutes(const int minutes)
		{
			return TimeRelative(static_cast<long long>(TimeAbstract::FromMinutes(minutes)));
		}
		
		//-----------------------------------------------------------------------
		TimeRelative TimeRelative::CreateFromHours(const int hours)
		{
			return TimeRelative(static_cast<long long>(TimeAbstract::FromHours(hours)));
		}
		
		//-----------------------------------------------------------------------
		TimeRelative TimeRelative::CreateFromDays(const int days)
		{
			return TimeRelative(static_cast<long long>(TimeAbstract::FromDays(days)));
		}
				
		//-----------------------------------------------------------------------
		TimeRelative TimeRelative::CreateFromMicroseconds(const float microseconds)
		{
			return TimeRelative(TimeAbstract::FromMicroseconds(microseconds));
		}
		
		//----------------------------------------------------------------------- 
		TimeRelative TimeRelative::CreateFromMilliseconds(const float milliseconds)
		{
			return TimeRelative(TimeAbstract::FromMilliseconds(milliseconds));
		}
		
		//-----------------------------------------------------------------------
		TimeRelative TimeRelative::CreateFromSeconds(const float seconds)
		{
			return TimeRelative(TimeAbstract::FromSeconds(seconds));
		}
		
		//-----------------------------------------------------------------------
		TimeRelative TimeRelative::CreateFromMinutes(const float minutes)
		{
			return TimeRelative(TimeAbstract::FromMinutes(minutes));
		}
		
		//----------------------------------------------------------------------- 
		TimeRelative TimeRelative::CreateFromHours(const float hours)
		{
			return TimeRelative(TimeAbstract::FromHours(hours));
		}
		
		//----------------------------------------------------------------------- 
		TimeRelative TimeRelative::CreateFromDays(const float days)
		{
			return TimeRelative(TimeAbstract::FromDays(days));
		}
		
		//-----------------------------------------------------------------------
		TimeRelative TimeRelative::AsPositiveTime() const
		{
			return TimeRelative(TimeAbstract::AsPositiveTime());
		}
								
		//-----------------------------------------------------------------------
		TimeRelative TimeRelative::operator +(const TimeRelative& rhs) const		
		{
			return TimeRelative(m_Time + rhs.m_Time);
		}
		
		//-----------------------------------------------------------------------	
		TimeAbsolute TimeRelative::operator +(const TimeAbsolute& rhs) const		
		{
			return TimeAbsolute(m_Time + rhs.m_Time);
		}
			
		//-----------------------------------------------------------------------
		TimeRelative TimeRelative::operator -(const TimeRelative& rhs) const		
		{
			return TimeRelative(m_Time - rhs.m_Time);
		}
			
		//-----------------------------------------------------------------------	
		TimeAbsolute TimeRelative::operator -(const TimeAbsolute& rhs) const		
		{
			return TimeAbsolute(m_Time - rhs.m_Time);
		}

		//-----------------------------------------------------------------------
		TimeRelative& TimeRelative::operator +=(const TimeRelative& rhs)
		{
			m_Time += rhs.m_Time;
			return *this;
		}
			
		//-----------------------------------------------------------------------
		TimeRelative& TimeRelative::operator -=(const TimeRelative& rhs)
		{
			m_Time -= rhs.m_Time;
			return *this;
		}
			
		//-----------------------------------------------------------------------
		TimeRelative TimeRelative::operator*(const int rhs) const		
		{
			return TimeRelative(m_Time * rhs);
		}	
		
			//-----------------------------------------------------------------------
		TimeRelative TimeRelative::operator*(const float rhs) const		
		{
			return TimeRelative(static_cast<int>(static_cast<float>(m_Time) * rhs));
		}
			
		//-----------------------------------------------------------------------
		TimeRelative TimeRelative::operator/(const int rhs) const
		{
			DIA_ASSERT( rhs!=0, "Can not divide by Zero");
			return TimeRelative(m_Time / rhs);
		}	
		
		//-----------------------------------------------------------------------
		TimeRelative TimeRelative::operator/(const float rhs) const	
		{
			DIA_ASSERT( rhs!=0, "Can not divide by Zero");
			return TimeRelative(static_cast<int>(static_cast<float>(m_Time) / rhs));
		}
		
		//-----------------------------------------------------------------------	
		TimeRelative& TimeRelative::operator*=(const int rhs)
		{
			m_Time *= rhs;
			return *this;
		}
		
		//-----------------------------------------------------------------------	
		TimeRelative& TimeRelative::operator*=(const float rhs)
		{
			m_Time = static_cast<int>(static_cast<float>(m_Time) * rhs);
			return *this;
		}
		
		#if WIN32
		#pragma warning(push)
		#pragma warning(disable:4723)
		#endif
		//-----------------------------------------------------------------------
		TimeRelative& TimeRelative::operator/=(const int rhs)
		{ 
			DIA_ASSERT( rhs!=0, "Can not divide by Zero");
			m_Time /= rhs;
			return *this;
		}
		
		//-----------------------------------------------------------------------	
		TimeRelative& TimeRelative::operator/=(const float rhs)
		{
			DIA_ASSERT( rhs!=0.0f, "Can not divide by Zero");
			m_Time = static_cast<int>(static_cast<float>(m_Time) / rhs);
			return *this;
		}
		#if CORE_WIN32
		#pragma warning(pop)
		#endif
	}
} 

