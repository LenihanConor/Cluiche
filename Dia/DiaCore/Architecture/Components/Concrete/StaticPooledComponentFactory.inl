#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core 
	{
		template<class T, unsigned int capacity>
        StaticPooledComponentFactory<T, capacity>::StaticPooledComponentFactory(ComponentClassID associatedId)
			: IComponentFactory()
        {
            mComponentMetaData.FillDefault();
            mComponents.FillDefault();
                
            mNextFreeComponentIndex = 0;

			mAssociatedId = associatedId;
        }

		template<class T, unsigned int capacity>
		StaticPooledComponentFactory<T, capacity>::~StaticPooledComponentFactory()
		{
			// Go through each associated object and remove this type of component
			for(int i = mAssociatedObjects.Size() - 1; i >= 0; i--)
			{
				mAssociatedObjects[i]->RemoveComponentFromID(mAssociatedId);
			}
		}
					

		template<class T, unsigned int capacity>
        IComponent* StaticPooledComponentFactory<T, capacity>::DoCreateComponent(IComponent::CreateStruct* input)
        {
            if (mNextFreeComponentIndex == kInvalidIndex)
            {
                DIA_ASSERT(0, "No more free components");
                return nullptr;
            }
			unsigned int currentFreeComponentIndex = mNextFreeComponentIndex;

            mComponentMetaData[currentFreeComponentIndex].mIsFree = false;

			mNextFreeComponentIndex = FindEarliestFreeComponentIndex();

            IComponent* component = &mComponents[currentFreeComponentIndex];

			if (component == nullptr)
			{
				DIA_ASSERT(0, "Failed to create a component");
				return nullptr;
			}

			return component;
        }

		template<class T, unsigned int capacity>
        void StaticPooledComponentFactory<T, capacity>::DoDestroyComponent(IComponent* component)
        {
            if (component == nullptr)
            {
                DIA_ASSERT(0, "Cannot destroy a component null ptr");
                return;
            }

            int index = ConvertComponentAddressToIndex(component);

            mComponents[index].Destroy();
            mComponentMetaData[index].mIsFree = true;
                
            mNextFreeComponentIndex = FindEarliestFreeComponentIndex();
        }

		template<class T, unsigned int capacity>
        int StaticPooledComponentFactory<T, capacity>::FindEarliestFreeComponentIndex()const
        {
            for(unsigned int i = 0; i < capacity; i++)
            {
                if (mComponentMetaData[i].mIsFree)
                {
                    return i;
                }
            }
            return kInvalidIndex;
        }

		template<class T, unsigned int capacity>
        int StaticPooledComponentFactory<T, capacity>::ConvertComponentAddressToIndex(IComponent* component)const
        {
            if (component == nullptr)
            {
                DIA_ASSERT(0, "Cannot handle a null ptr");
                return kInvalidIndex;
            }

            for(unsigned int i = 0; i < capacity; i++)
            {
                if(&mComponents[i] == component)
                {
                    return i;
                }
            }
            return kInvalidIndex;
        } 

		template<class T, unsigned int capacity>
		void StaticPooledComponentFactory<T, capacity>::RegisterObject(IComponentObject* object) 
		{
			// Associate the object to this manager
			mAssociatedObjects.Add(object);
		}

		template<class T, unsigned int capacity>
		void StaticPooledComponentFactory<T, capacity>::DeregisterObject(IComponentObject* object) 
		{
			// Remove the object as an associated object
			mAssociatedObjects.RemoveFirst(object);
		}

		template<class T, unsigned int capacity>
		unsigned int StaticPooledComponentFactory<T, capacity>::GetNumberOfActiveComponents()const 
		{
			unsigned int numberActiveComponents = capacity;
			for(unsigned int i = 0; i < mComponentMetaData.Size(); ++i)
			{
				if (mComponentMetaData[i].mIsFree)
				{
					numberActiveComponents--;
				}
			}
			return numberActiveComponents;
		}

		template<class T, unsigned int capacity>
		unsigned int StaticPooledComponentFactory<T, capacity>::GetNumberOfReservedComponents()const 
		{
			return capacity;
		}
	}	
} // namespace Axiom

