#pragma once

#include "DiaCore/Time/TimeAbstract.h"

namespace Dia
{
		
	namespace Core
	{
	
		class TimeRelative;	
					
		//--------------------------------------------------------------------------
		// TimeAbsolute class
		//--------------------------------------------------------------------------
		class TimeAbsolute : public TimeAbstract
		{
		public:
			friend class TimeRelative;

			TimeAbsolute(const TimeAbsolute&);
				
			// Operators
			TimeAbsolute				operator+(const TimeRelative& rhs)	const;
			TimeAbsolute				operator-(const TimeRelative& rhs)	const;
			TimeRelative				operator-(const TimeAbsolute& rhs)	const;
				
			TimeAbsolute&				operator+=(const TimeRelative& rhs);
			TimeAbsolute&				operator-=(const TimeRelative& rhs);
				
			bool						operator<(const TimeAbsolute& time) const;
			bool						operator<=(const TimeAbsolute& time) const;
			bool						operator==(const TimeAbsolute& time) const;
			bool						operator!=(const TimeAbsolute& time) const;
			bool						operator>=(const TimeAbsolute& time) const;
			bool						operator>(const TimeAbsolute& time) const;
				
			// Static factory methods
			static const TimeAbsolute	Zero();
			static const TimeAbsolute	MaximumTime();
			static const TimeAbsolute	MinimumTime();
	
			static TimeAbsolute			CreateFromMicroseconds(const int microseconds);
			static TimeAbsolute			CreateFromMilliseconds(const int milliseconds);
			static TimeAbsolute			CreateFromSeconds(const int seconds);
			static TimeAbsolute			CreateFromMinutes(const int minutes);
			static TimeAbsolute			CreateFromHours(const int hours);
			static TimeAbsolute			CreateFromDays(const int days);
				
			static TimeAbsolute			CreateFromMicroseconds(const long long microseconds);
		
			static TimeAbsolute			CreateFromMicroseconds(const float microseconds);
			static TimeAbsolute			CreateFromMilliseconds(const float milliseconds);
			static TimeAbsolute			CreateFromSeconds(const float seconds);
			static TimeAbsolute			CreateFromMinutes(const float minutes);
			static TimeAbsolute			CreateFromHours(const float hours);
			static TimeAbsolute			CreateFromDays(const float days);
		
			static TimeAbsolute			GetSystemTime();
		
			TimeAbsolute				AsPositiveTime() const;
				
		private:	
			explicit TimeAbsolute();
			explicit TimeAbsolute(long long);	
		};	
	}
}

