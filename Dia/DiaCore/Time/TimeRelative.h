#pragma once

#include "DiaCore/Time/TimeAbstract.h"

namespace Dia
{
	namespace Core
	{	
		class TimeAbsolute;
		//--------------------------------------------------------------------------
		// Time class
		//--------------------------------------------------------------------------
		class TimeRelative : public TimeAbstract
		{
		public:
			friend class TimeAbsolute;

			TimeRelative(const TimeRelative&);
				
			// Operators
			TimeRelative				operator+(const TimeRelative& rhs) const;
			TimeAbsolute				operator+(const TimeAbsolute& rhs) const;
			TimeRelative				operator-(const TimeRelative& rhs) const;
			TimeAbsolute				operator-(const TimeAbsolute& rhs) const;

			TimeRelative&				operator+=(const TimeRelative& rhs);
			TimeRelative&				operator-=(const TimeRelative& rhs);
				
			TimeRelative				operator*(const int rhs) const;
			TimeRelative				operator*(const float rhs) const;
			TimeRelative				operator/(const int rhs) const;
			TimeRelative				operator/(const float rhs) const;
				
			TimeRelative&				operator*=(const int rhs);
			TimeRelative&				operator*=(const float rhs);
			TimeRelative&				operator/=(const int rhs);
			TimeRelative&				operator/=(const float rhs);
				
			bool						operator<(const TimeRelative& time) const;
			bool						operator<=(const TimeRelative& time) const;
			bool						operator==(const TimeRelative& time) const;
			bool						operator!=(const TimeRelative& time) const;
			bool						operator>=(const TimeRelative& time) const;
			bool						operator>(const TimeRelative& time) const;
				
			static const TimeRelative		Zero();
			static const TimeRelative		MaximumTime();
			static const TimeRelative		MinimumTime();
		
			static TimeRelative				CreateFromMilliseconds(const int milliseconds);
			static TimeRelative				CreateFromSeconds(const int seconds);
			static TimeRelative				CreateFromMinutes(const int minutes);
			static TimeRelative				CreateFromHours(const int hours);
			static TimeRelative				CreateFromDays(const int days);
				
			static TimeRelative				CreateFromMicroseconds(const float microseconds);
			static TimeRelative				CreateFromMilliseconds(const float milliseconds);
			static TimeRelative				CreateFromSeconds(const float seconds);
			static TimeRelative				CreateFromMinutes(const float minutes);
			static TimeRelative				CreateFromHours(const float hours);
			static TimeRelative				CreateFromDays(const float days);
				
			TimeRelative					AsPositiveTime() const;
							
		private:
			explicit TimeRelative();	
			explicit TimeRelative(long long microseconds);		
		};
	}
}

