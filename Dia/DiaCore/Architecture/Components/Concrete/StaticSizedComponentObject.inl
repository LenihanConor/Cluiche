
#include "DiaCore/Core/Assert.h"


//------------------------------------------------------------------------------------
//			
//		
//------------------------------------------------------------------------------------
namespace Dia
{
	namespace Core
	{
		template<unsigned int capacity>
		StaticSizedComponentObject<capacity>::~StaticSizedComponentObject()
		{
			for (int i = mComponentPtrs.Size() - 1; i >= 0; --i)
			{
				IComponent* component = mComponentPtrs[i];
				RemoveComponentFromFactory(component->GetAssociatedFactory());
			}
		}

		template<unsigned int capacity>
		bool StaticSizedComponentObject<capacity>::HasComponent(const ComponentClassID& id)const 
		{
			for (unsigned int i = 0; i < mComponentPtrs.Size(); ++i)
			{
				if(mComponentPtrs[i]->IsType(id))
				{
					return true;
				}
			}

			return false;
		}

		template<unsigned int capacity>
		IComponent* StaticSizedComponentObject<capacity>::GetComponentPtr(const ComponentClassID& id)
		{
			for (unsigned int i = 0; i < mComponentPtrs.Size(); ++i)
			{
				if(mComponentPtrs[i]->IsType(id))
				{
					return mComponentPtrs[i];
				}
			}

			return nullptr;
		}

		template<unsigned int capacity>
		const IComponent* StaticSizedComponentObject<capacity>::GetComponentPtr(const ComponentClassID& id)const
		{
			for (unsigned int i = 0; i < mComponentPtrs.Size(); ++i)
			{
				if(mComponentPtrs[i]->IsType(id))
				{
					return mComponentPtrs[i];
				}
			}

			return nullptr;
		}

		template<unsigned int capacity>
		void StaticSizedComponentObject<capacity>::AssignComponent(IComponent* component) 
		{
			if (component == nullptr)
			{
				DIA_ASSERT(0, "Cannot assign a null ptr component");
				return;
			}

			mComponentPtrs.Add(component);
		}

		template<unsigned int capacity>
		void StaticSizedComponentObject<capacity>::DeassignComponent(IComponent* component) 
		{
			if (component == nullptr)
			{
				DIA_ASSERT(0, "Cannot deassign a null ptr component");
				return;
			}

			mComponentPtrs.RemoveFirst(component);
		}
	}
}
