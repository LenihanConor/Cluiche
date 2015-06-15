#ifndef DIA_DYNAMIC_COMPONENT_FACTORY
#define DIA_DYNAMIC_COMPONENT_FACTORY

#include "DiaCore/Architecture/Components/Interface/IComponentFactory.h"

//------------------------------------------------------------------------------------
//			
//
//------------------------------------------------------------------------------------
namespace Dia
{
	namespace Core
	{
		template<class T>
		class DynamicComponentFactory: public IComponentFactory
		{
		public:
			virtual ~DynamicComponentFactory(){};

			virtual void RegisterObject(IComponentObject* object)override{}
			virtual void DeregisterObject(IComponentObject* object)override{}
			virtual void CleanupRegisteredObjects()override{}
		protected:
			virtual IComponent* DoCreateComponent(IComponent::CreateStruct* input) override;
			virtual void DoDestroyComponent(IComponent* component) override;
		};
	}
}


#include "DiaCore/Architecture/Components/Concrete/DynamicComponentFactory.inl"

#endif