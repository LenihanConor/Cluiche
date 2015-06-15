
#include "DiaCore/Architecture/Components/Interface/IComponentFactory.h"

namespace Dia
{
	namespace Core 
	{
		IComponentFactory::~IComponentFactory()
		{}

		/******************************************************************************/
		/*!
			\brief  Called by the manager to create a component

			\param  input  Class that is used to connect the calling code to 
							the virtual DoUpdate on the derived concrete class 
			\param  associatedManager   Ptr to the manager this component came from
		*/

		/******************************************************************************/
        IComponent* IComponentFactory::CreateComponent(IComponent::CreateStruct* input)
        {
            IComponent* newComponent = DoCreateComponent(input);

			newComponent->SetAssociatedFactory(this);

			newComponent->Create(input);

			return newComponent;
        }

		/******************************************************************************/
		/*!
			\brief  Called when we are "destroying" a component at the moment of 
						deassignment to the component object
		*/
		/******************************************************************************/
        void IComponentFactory::DestroyComponent(IComponent* component)
        {
			component->SetAssociatedFactory(nullptr); 
            DoDestroyComponent(component);
        } 
	}	
} // namespace Axiom

