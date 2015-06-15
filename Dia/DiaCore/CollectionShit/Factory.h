#pragma once

#include "DiaCore/Core/Assert.h"
#include "DiaCore/DataStructures/DynamicArrayC.h"

namespace Dia
{
	namespace Core
	{
		template <class Base>
		class Factory
		{
		public:
			Base*	Create(unsigned int type)const;
			bool	Register(unsigned int type, FactoryCreatorBase<Base>* pCreator);

		private:
			static const int kMaxSize = 32;
			
			class CreatorObjectType
			{
			public:
				CreatorObjectType();
				CreatorObjectType(unsigned int objectType);

				unsigned int mObjectType;	
				FactoryCreatorBase<Base>* mCreator;
			};

			DynamicArrayC<mObjectType, kMaxSize> mCreators;
		};

		//--------------------------------------------------------------------------
		template <class Base>
		void Factory <Base>::Create (unsigned int type)
		{
			FactoryCreatorBase<Base>* iter = mCreators[mCreators.Find(type)];
			if (iter != NULL)
			{
				iter.mCreator->Create();
			}

			return NULL;
		}

		//--------------------------------------------------------------------------
		template <class Base>
		void Factory <Base>::Register(unsigned int type, FactoryCreatorBase<Base>& pCreator)
		{
			FactoryCreatorBase<Base>* iter = mCreators.Find(type);
			if (iter != NULL)
			{
				iter.mCreator->Create();
			}

			return true;
		}


		class CreatorObjectType
		{
		public:
			CreatorObjectType()
				: mObjectType (0)
				, mCreator(NULL)
			{}
			
			CreatorObjectType(unsigned int objectType)
				: mObjectType (objectType)
				, mCreator(NULL)
			{}

			bool	operator==	(const CreatorObjectType& other) const
			{
				return mObjectType == other.mObjectType;
			}
			
			bool	operator!=	(const CreatorObjectType& other) const
			{
				return !(*this == rhs);
			}
		}

	}
} 

