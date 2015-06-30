namespace Dia
{
	namespace Core
	{
		template <class T> inline
		FrameStream<T>::FrameStream()
		{}

		template <class T> inline
		void FrameStream<T>::InsertCopyOfDataToStream(const T& data, const Dia::Core::TimeAbsolute& timeStamp)
		{
			std::lock_guard<std::mutex> lock(mMutex);

			InternalData tempStamp;
			mFrameList.push_back(tempStamp);				// Create a default value then insert the data so that it is faster
			mFrameList.back().Create(data, timeStamp);
		}

		template <class T> inline
		const T* FrameStream<T>::FetchDataClosestToTime(const Dia::Core::TimeAbsolute& timeStamp, Dia::Core::TimeAbsolute& timeStampOfOutput)const
		{
			std::lock_guard<std::mutex> lock(mMutex);

			if (mFrameList.size() == 0)
			{
				return nullptr;
			}

			if (mFrameList.size() == 1)
			{
				timeStampOfOutput = mFrameList.front().mTimeStamp;
				return &(mFrameList.front().mFrameData);
			}

			const InternalData* returnValue = &mFrameList.front();

			for (const InternalData& i : mFrameList)
			{
				if (i.mTimeStamp > timeStamp)
				{
					break;
				}
				else
				{
					returnValue = &i;
				}
			}

			timeStampOfOutput = returnValue->mTimeStamp;
			return &(returnValue->mFrameData);
		}

		template <class T> inline
		void FrameStream<T>::FetchAllDataUpToTime(const Dia::Core::TimeAbsolute& timeStamp, Dia::Core::Containers::DynamicArrayC<const T*, 32>& ptrBuffer)const
		{
			std::lock_guard<std::mutex> lock(mMutex);

			if (mFrameList.size() == 0)
			{
				return;
			}

			for (const InternalData& i : mFrameList)
			{
				if (i.mTimeStamp > timeStamp)
				{
					break;
				}

				ptrBuffer.Add(&(i.mFrameData));
			}
		}

		template <class T> inline
		void FrameStream<T>::GarbageCollectAllFramesOlderThan(const Dia::Core::TimeAbsolute& timeStamp)
		{
			std::lock_guard<std::mutex> lock(mMutex);

			if (mFrameList.size() == 0)
			{
				return;
			}

			int position = 0;
			for (const InternalData& i : mFrameList)
			{
				if (i.mTimeStamp > timeStamp)
				{
					break;
				}

				position++;
			}

			mFrameList.erase(mFrameList.begin(), mFrameList.begin() + Dia::Maths::Clamp(position, 0, static_cast<int>(mFrameList.size())));
		}





		
		template <class T> inline
		FrameStream<T>::InternalData::InternalData()
			: mTimeStamp(Dia::Core::TimeAbsolute::Zero())
		{}


		template <class T> inline
		void FrameStream<T>::InternalData::Create(const T& data, const Dia::Core::TimeAbsolute& timeStamp)
		{
			static int sGlobalFrameIndex = 0;

			mFrameIndex = sGlobalFrameIndex++;
			mTimeStamp = timeStamp;
			mFrameData = data;
		}

		template <class T> inline
		typename FrameStream<T>::InternalData& FrameStream<T>::InternalData::operator=(const InternalData &rhs)
		{
			mTimeStamp = rhs.mTimeStamp;
			mFrameData = rhs.mFrameData;

			return *this;
		}

		template <class T> inline
		int FrameStream<T>::InternalData::operator==(const InternalData &rhs) const
		{
			return static_cast<int>(mTimeStamp == rhs.mTimeStamp);
		}

		template <class T> inline
		int FrameStream<T>::InternalData::operator<(const InternalData &rhs) const
		{
			return static_cast<int>(mTimeStamp < rhs.mTimeStamp);
		}
	}
}