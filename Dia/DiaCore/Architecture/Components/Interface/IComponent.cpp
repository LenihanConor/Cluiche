
#include "DiaCore/Architecture/Components/Interface/IComponent.h"

#include "DiaCore/Architecture/Components/Interface/IComponentFactory.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core 
	{	
		IComponent::IComponent()
			: mAssociatedFactory(nullptr)
		{}

		void IComponent::SetAssociatedFactory(IComponentFactory* factory)
		{
			mAssociatedFactory = factory;
		}

		IComponentFactory* IComponent::GetAssociatedFactory()
		{
			DIA_ASSERT(mAssociatedFactory != nullptr, "Associated Factory cannot be equal to NULL");

			return mAssociatedFactory;
		}

		const IComponentFactory* IComponent::GetAssociatedFactory()const
		{
			DIA_ASSERT(mAssociatedFactory != nullptr, "Associated Factory cannot be equal to NULL");

			return mAssociatedFactory;
		}
	}	
} 

