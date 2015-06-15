#ifndef DIA_COMPONENT_FACTORY_REGISTRY
#define DIA_COMPONENT_FACTORY_REGISTRY
		
//------------------------------------------------------------------------------------
namespace Dia
{
	namespace Core
	{
		/******************************************************************************/
		/*!
			\class  IComponentObject

			\brief  Interface for a component object
		*/
		/******************************************************************************/
		class ComponentFactoryRegistry
		{
		public:
			ComponentFactoryRegistry(){};

			void RegisterFactory(IComponentFactory* factory);
			void DeregisterFactory(IComponentFactory* factory);

			IComponentFactory* GetFactory(ComponentClassID id);
			const IComponentFactory* GetFactory(ComponentClassID id)const;

			void PopulateObjectWithAllComponents(IComponentObject* object);
			void DepopulateObjectWithAllComponents(IComponentObject* object);

		private:
			// Map of factory to id
		};
	}
}

#endif