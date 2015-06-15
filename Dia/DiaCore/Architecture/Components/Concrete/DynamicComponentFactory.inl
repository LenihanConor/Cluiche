#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core 
	{
		template <class T>
		IComponent* DynamicComponentFactory<T>::DoCreateComponent(IComponent::CreateStruct* input) 
        {
            IComponent* newComponent = new T(); 

			if(component == nullptr)
			{
				DIA_ASSERT(0, "Failed to create component");
				return NULL;
			}

            newComponent->DoCreate(input); 

			return newComponent;
        }
		template <class T>
		void DynamicComponentFactory<T>::DoDestroyComponent(IComponent* component) 
        {
			if(component == nullptr)
			{
				DIA_ASSERT(0, "Cannot delete a null component");
				return;
			}
            delete component;              
        }     
	}	
} // namespace Axiom

