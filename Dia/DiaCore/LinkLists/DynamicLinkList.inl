namespace Dia
{
	namespace Core
	{
		namespace Collections
		{
			//------------------------------------------------------------------------------------
			//	DynamicLinkListNode - Implementation
			//------------------------------------------------------------------------------------
			template <class T> inline
			DynamicLinkListNode<T>::DynamicLinkListNode()
				: mNext(NULL)
			{}
		
			//-----------------------------------------------------------------------------
			template <class T> inline
			DynamicLinkListNode<T>::DynamicLinkListNode(const T& payload)
				: mPayload(payload)
				, mNext(NULL)
			{}

			//-----------------------------------------------------------------------------
			template <class T> inline
			DynamicLinkListNode<T>::~DynamicLinkListNode()
			{
				mNext = NULL;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			void DynamicLinkListNode<T>::SetPayload(const T& payload)
			{
				mPayload = payload;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			DynamicLinkListNode<T>* DynamicLinkListNode<T>::GetNext()
			{
				return mNext;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			const DynamicLinkListNode<T>* DynamicLinkListNode<T>::GetNext()const
			{
				return mNext;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			T& DynamicLinkListNode<T>::GetPayload()
			{
				return mPayload;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			const T& DynamicLinkListNode<T>::GetPayload()const
			{
				return mPayload;
			}

			//------------------------------------------------------------------------------------
			//	DynamicLinkList - Implementation
			//------------------------------------------------------------------------------------
			template <class T> inline
			DynamicLinkList::DynamicLinkList()
				: mRoot(NULL)
				, mLastNode(NULL)
				, mHighWatermark(0)
				, mNumberNodes(0)
			{}

			//------------------------------------------------------------------------------------
			template <class T> inline
			DynamicLinkList::~DynamicLinkList()
			{
				NOT DONE
				mHighWatermark = 0;
				mNumberNodes = 0;
			}

			//------------------------------------------------------------------------------------
			template <class T> inline
			bool DynamicLinkList::IsEmpty() const
			{
				return (mNumberNodes == 0);
			}
			
			//------------------------------------------------------------------------------------
			template <class T> inline
			unsigned int DynamicLinkList::Count	() const
			{
				return mNumberNodes
			}

			//------------------------------------------------------------------------------------
			template <class T> inline
			unsigned int DynamicLinkList::HighWatermark() const
			{
				return mHighWatermark;
			}
		}
	}
}