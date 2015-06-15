#ifndef DIA_ICOMPONENT
#define DIA_ICOMPONENT

#include "DiaCore/Architecture/Components/Interface/ComponentID.h"

namespace Dia
{
	namespace Core
	{
		class IComponentFactory;
	}
}

//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
namespace Dia
{
	namespace Core
	{
		// Define used by every inherited component class to make syre we have type information
		#define COMPONENT_DECLARATION(classId) \
                    static const Dia::Core::ComponentClassID ID = classId;\
					virtual bool IsType(const Dia::Core::ComponentClassID& id)const override{ return id == classId; };\
					virtual Dia::Core::ComponentClassID GetType()const{return classId;};\
		
		/******************************************************************************/
		/*!
			\class  IComponent

			\brief  Interface for a component
		*/
		/******************************************************************************/
		class IComponent
		{
		public:
            struct CreateStruct
            {
                template <class T>
                T* Cast();
            };

            struct UpdateStruct
            {
                template <class T>
				T* Cast();
            };

			IComponent();
			virtual ~IComponent(){mAssociatedFactory = nullptr;};

			virtual void Create(CreateStruct* input){};
			virtual void Destroy(){};
			
			virtual bool IsType(const ComponentClassID& id)const=0;
			virtual Dia::Core::ComponentClassID GetType()const=0;

			void SetAssociatedFactory(IComponentFactory* factory);
			IComponentFactory* GetAssociatedFactory();
			const IComponentFactory* GetAssociatedFactory()const;

		private:
			IComponentFactory* mAssociatedFactory;
		};	
	}
}

#endif