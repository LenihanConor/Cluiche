#pragma once

#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <deque>
#include <mutex>

namespace Dia
{
	namespace Core
	{
		template <class T>
		class FrameStream
		{
		public:
			FrameStream();

			void InsertCopyOfDataToStream(const T& data, const Dia::Core::TimeAbsolute& timeStamp);

			const T* FetchLatestData(Dia::Core::TimeAbsolute& timeStampOfOutput)const;
			const T* FetchDataClosestToTime(const Dia::Core::TimeAbsolute& timeStamp, Dia::Core::TimeAbsolute& timeStampOfOutput)const;
			void FetchAllDataUpToTime(const Dia::Core::TimeAbsolute& timeStamp, Dia::Core::Containers::DynamicArrayC<const T*, 32>& ptrBuffer)const;

			bool IsTimeStampGreaterThanCurrentData(const Dia::Core::TimeAbsolute& timeStamp)const;

			void GarbageCollectAllFramesOlderThan(const Dia::Core::TimeAbsolute& timeStamp);

		private:
			class InternalData
			{
			public:
				InternalData();

				void Create(const T& data, const Dia::Core::TimeAbsolute& timeStamp);

				InternalData& operator=(const InternalData &rhs);
				int operator==(const InternalData &rhs) const;
				int operator<(const InternalData &rhs) const;

				int mFrameIndex;
				Dia::Core::TimeAbsolute mTimeStamp;
				T mFrameData;
			};

			mutable std::mutex mMutex;
			std::deque<InternalData> mFrameList;
		}; 
	}
}

#include "DiaCore/Frame/FrameStream.inl"