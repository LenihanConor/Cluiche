#ifndef DIA_ICOMPONENT_FACTORY
#define DIA_ICOMPONENT_FACTORY

#include "DiaCore/Architecture/Components/Interface/ComponentID.h"
#include "DiaCore/Architecture/Components/Interface/IComponent.h"

namespace Dia
{
	namespace Core
	{
		class IComponentObject;
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
		#define COMPONENT_FACTORY_DECLARATION(className) \
                    static const Dia::Core::ComponentClassID ID = className::ID;\
					virtual bool IsType(const Dia::Core::ComponentClassID& id)const override{ return id == ID; };\
					virtual Dia::Core::ComponentClassID GetType()const{return ID;};\

		/******************************************************************************/
		/*!
			\class  IComponentFactory

			\brief  Interface for a component factory
		*/
		/******************************************************************************/
		class IComponentFactory
		{
		public:
			virtual ~IComponentFactory();
			
			IComponent* CreateComponent(IComponent::CreateStruct* input); 
			void DestroyComponent(IComponent* component);

			virtual bool IsType(const ComponentClassID& id)const=0;
			virtual Dia::Core::ComponentClassID GetType()const=0;

			virtual void RegisterObject(IComponentObject* object)=0;
			virtual void DeregisterObject(IComponentObject* object)=0;

			virtual unsigned int GetNumberOfActiveComponents()const =0;
			virtual unsigned int GetNumberOfReservedComponents()const =0;
		protected:
			virtual IComponent* DoCreateComponent(IComponent::CreateStruct* input) = 0;
			virtual void DoDestroyComponent(IComponent* component) = 0;	
		};
	}
}


#endif