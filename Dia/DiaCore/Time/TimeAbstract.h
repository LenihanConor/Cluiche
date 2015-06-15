#pragma once

namespace Dia
{

		namespace Core
		{
		//--------------------------------------------------------------------------
		// TimeAbstract class
		//--------------------------------------------------------------------------
		class TimeAbstract
		{
		public:
			// Integer forms
			int					AsIntInMicroseconds() const;
			int					AsIntInMilliseconds() const;
			int					AsIntInSeconds() const;
			int					AsIntInMinutes() const;
			int					AsIntInHours() const;
			int					AsIntInDays() const;
			
			long long			AsLongLongInMicroseconds() const;

			// Float forms
			float				AsFloatInMicroseconds() const;
			float				AsFloatInMilliseconds() const;
			float				AsFloatInSeconds() const;
			float				AsFloatInMinutes() const;
			float				AsFloatInHours() const;
			float				AsFloatInDays() const;
			
		protected:			
			explicit TimeAbstract(long long time);
			 
			TimeAbstract				(const TimeAbstract&);
			TimeAbstract&				operator=(const TimeAbstract&);
				
			static long long			ZeroVal();
			static long long			MaximumTimeVal();
			static long long			MinimumTimeVal();
		
			static long long			FromMicroseconds(const int microseconds);
			static long long			FromMilliseconds(const int milliseconds);
			static long long			FromSeconds(const int seconds);
			static long long			FromMinutes(const int minutes);
			static long long			FromHours(const int hours);
			static long long			FromDays(const int days);
		
			static long long			FromMicroseconds(const float microseconds);
			static long long			FromMilliseconds(const float milliseconds);
			static long long			FromSeconds(const float seconds);
			static long long			FromMinutes(const float minutes);
			static long long			FromHours(const float hours);
			static long long			FromDays(const float days);
				
			long long					AsPositiveTime() const;
				
			// Data members
			long long					m_Time;

			static const int intFromMicroToMilliseconds;
			static const int intFromMicroToSeconds;
			static const int intFromMicroToMinutes;
			static const long long intFromMicroToHours;
			static const long long  intFromMicroToDays;

			static const float floatFromMicroToMilliseconds	;
			static const float floatFromMicroToSeconds;
			static const float floatFromMicroToMinutes;
			static const float floatFromMicroToHours;
			static const float floatFromMicroToDays;
		};
	}
}
