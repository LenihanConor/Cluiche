#pragma once

#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		namespace Collections
		{
			//------------------------------------------------------------------------------------
			//	DynamicLinkListNode
			//------------------------------------------------------------------------------------
			template <class T>
			class DynamicLinkListNode
			{
			public:
				DynamicLinkListNode();
				DynamicLinkListNode(const T& payload);
				~DynamicLinkListNode();
				
				void				SetPayload(const T& payload);

				DynamicLinkListNode<T>*			GetNext();
				const DynamicLinkListNode<T>*	GetNext()const;

				T&					GetPayload();
				const T&			GetPayload()const;

			private:
				T mPayload;
				DynamicLinkListNode<T>* mNext;
			};

			//------------------------------------------------------------------------------------
			//	DynamicLinkList
			//------------------------------------------------------------------------------------
			template <class T>
			class DynamicLinkList
			{
			public:

				DynamicLinkList();
				~DynamicLinkList();

				bool			IsEmpty				() const;
				unsigned int	Count				() const;
				unsigned int	HighWatermark		() const;

				void			Add					(T& payloadToAdd);
				void			AddDefault			();
				void			Insert				(const unsigned int index, const T& nodeToAdd);
				void			InsertDefault		(const unsigned int index);
				void			Remove				(const unsigned int index);
				
				DynamicLinkListNode<T>&			Item				(const unsigned int index);
				const DynamicLinkListNode<T>&	Item				(const unsigned int index) const;
				const LinkListNode<T>&			ConstItem			(const unsigned int index) const;
				DynamicLinkListNode<T>&			LastItem			();
				const DynamicLinkListNode<T>&	LastItem			() const;

				DynamicLinkListNode<T>&			FirstItem			();
				const DynamicLinkListNode<T>&	FirstItem			() const;

				void							RemoveHead();

				// ---------- changing data ---------------
				DynamicLinkListNode<T>&				operator []			(const unsigned int index);
				const DynamicLinkListNode<T>&		operator []			(const unsigned int index) const;

			private:	
				DynamicLinkListNode<T>*	mRoot;
				DynamicLinkListNode<T>*	mLastNode;

				unsigned int			mHighWatermark;
				unsigned int			mNumberNodes;
			};
		}
	}
}

#include "DiaCore/LinkLists/DynamicLinkList.h"