
#include "DiaCore/Architecture/Components/Interface/IComponentObject.h"

#include "DiaCore/Architecture/Components/Interface/IComponentFactory.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core 
	{
		void IComponentObject::AddComponentFromFactory(IComponentFactory* factory, IComponent::CreateStruct* input)
		{
			// Need a factory
			if(factory == nullptr)
			{
				DIA_ASSERT(0, "Requires factory");
				return;
			}
				
			// Creat the component from the factory
			IComponent* newComponent = factory->CreateComponent(input);

			// If we fail to create a component handle gracefully
			if (newComponent == nullptr)
			{
				DIA_ASSERT(0, "Failed to create component");
				return;
			}

			factory->RegisterObject(this);

			AssignComponent(newComponent);
		}

		void IComponentObject::RemoveComponentFromFactory(IComponentFactory* factory)
		{
			// Need to have a factory
			if(factory == nullptr)
			{
				DIA_ASSERT(0, "Factory needs to be initialized first");
				return;
			}

			// Get the associated componet id
			ComponentClassID componentID = factory->GetType();

			// Get the component that is associated to this object
			IComponent* component = GetComponentPtr(componentID);

			// If this doesnt exist something bad has happened in the associated object list
			if (component == nullptr)
			{
				DIA_ASSERT(0, "Cannot destroy nullptr component");
				return;
			}

			// Remove the component from the object
			DeassignComponent(component);

			factory->DeregisterObject(this);

			// Destroy the component
			factory->DestroyComponent(component);
		}


		// This will leak the factory, but in the scenario where the factory is dstructing this is not an issue
		void IComponentObject::RemoveComponentFromID(ComponentClassID componentID)
		{
			// Get the component that is associated to this object
			IComponent* component = GetComponentPtr(componentID);

			// If this doesnt exist something bad has happened in the associated object list
			if (component == nullptr)
			{
				DIA_ASSERT(0, "Cannot destroy nullptr component");
				return;
			}

			// Remove the component from the object
			DeassignComponent(component);
		}
	}	
} 

