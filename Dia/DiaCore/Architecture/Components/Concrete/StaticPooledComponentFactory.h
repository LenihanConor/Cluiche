#ifndef DIA_STATIC_POOLED_COMPONENT_FACTORY
#define DIA_STATIC_POOLED_COMPONENT_FACTORY

#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

#include "DiaCore/Architecture/Components/Interface/IComponentFactory.h"

//------------------------------------------------------------------------------------
//			
//
//------------------------------------------------------------------------------------
namespace Dia
{
	namespace Core
	{
        template<class T, unsigned int capacity>
        class StaticPooledComponentFactory: public IComponentFactory
        {
        public:
			static const int kInvalidIndex = -1;

            StaticPooledComponentFactory(ComponentClassID associatedId);
            virtual ~StaticPooledComponentFactory();

			virtual void RegisterObject(IComponentObject* object)override;
			virtual void DeregisterObject(IComponentObject* object)override;

			virtual unsigned int GetNumberOfActiveComponents()const override;
			virtual unsigned int GetNumberOfReservedComponents()const override;

        protected:
            struct ComponentMetaData 
            {
                ComponentMetaData() : mIsFree(true){}
                bool mIsFree;
            };

            virtual IComponent* DoCreateComponent(IComponent::CreateStruct* input) override;
            virtual void DoDestroyComponent(IComponent* component) override;

            int FindEarliestFreeComponentIndex()const;
            int ConvertComponentAddressToIndex(IComponent* component)const;

			ComponentClassID mAssociatedId;
            int mNextFreeComponentIndex; 
            Dia::Core::Containers::DynamicArrayC<ComponentMetaData, capacity> mComponentMetaData; 
            Dia::Core::Containers::DynamicArrayC<T, capacity> mComponents; 
			Dia::Core::Containers::DynamicArrayC<IComponentObject*, 32> mAssociatedObjects; // This should be a link/list or something
        };

	#define CREATE_STATIC_SIZED_COMPONENT_FACTORY(factoryName, associatedClassName, size)\
		class factoryName: public Dia::Core::StaticPooledComponentFactory<associatedClassName, size>\
		{\
		public:\
			COMPONENT_FACTORY_DECLARATION(associatedClassName);\
			factoryName(): Dia::Core::StaticPooledComponentFactory<associatedClassName, size>(ID){}\
		};
	}
}

#include "DiaCore/Architecture/Components/Concrete/StaticPooledComponentFactory.inl"

#endif