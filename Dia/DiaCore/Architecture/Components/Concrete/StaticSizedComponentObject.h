#ifndef DIA_STATIC_SIZED_COMPONENT_OBJECT
#define DIA_STATIC_SIZED_COMPONENT_OBJECT

#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

#include "DiaCore/Architecture/Components/Interface/IComponentObject.h"

namespace Dia
{
	namespace Core
	{
		class ComponentManager;
	}
}

//------------------------------------------------------------------------------------
//			
//		
//------------------------------------------------------------------------------------
namespace Dia
{
	namespace Core
	{
		template<unsigned int capacity>
		class StaticSizedComponentObject: public IComponentObject
		{
		public:
			virtual ~StaticSizedComponentObject();

			virtual bool HasComponent(const ComponentClassID& id)const override;
			virtual IComponent* GetComponentPtr(const ComponentClassID& id)override;
			virtual const IComponent* GetComponentPtr(const ComponentClassID& id)const override;

		private:
			virtual void AssignComponent(IComponent* component) override;
			virtual void DeassignComponent(IComponent* component) override;

			Dia::Core::Containers::DynamicArrayC<IComponent*, capacity> mComponentPtrs;
		};
	}
}

#include "DiaCore/Architecture/Components/Concrete/StaticSizedComponentObject.inl"

#endif