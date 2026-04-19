#pragma once

#include "ApplicationTypeRegistry.h"
#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationModule.h>

// Registration macros for ProcessingUnit/Phase/Module types
// These use static initialization to automatically register types before main()

// ProcessingUnit Registration
// Usage in .cpp file:
//   DIA_REGISTER_PROCESSING_UNIT(MyProcessingUnit) {
//       return new MyProcessingUnit(instanceId, config.get("hz", 60.0f).asFloat());
//   }
#define DIA_REGISTER_PROCESSING_UNIT(ClassName) \
	namespace { \
		class ClassName##_Factory : public Dia::Application::ITypeFactory<Dia::Application::ProcessingUnit> { \
		public: \
			virtual Dia::Application::ProcessingUnit* Create(const Dia::Core::StringCRC& instanceId, const Json::Value& config) override; \
		}; \
		\
		Dia::Application::ProcessingUnit* ClassName##_Factory::Create(const Dia::Core::StringCRC& instanceId, const Json::Value& config) \
		{ \
			return CreateInstance(instanceId, config); \
		} \
		\
		static Dia::Application::ProcessingUnit* CreateInstance(const Dia::Core::StringCRC& instanceId, const Json::Value& config); \
		\
		struct ClassName##_Registrar { \
			ClassName##_Registrar() { \
				static ClassName##_Factory factory; \
				Dia::Application::ApplicationTypeRegistry::Instance().RegisterProcessingUnitType( \
					ClassName::kTypeId, &factory); \
			} \
		}; \
		static ClassName##_Registrar g_##ClassName##_registrar; \
	} \
	static Dia::Application::ProcessingUnit* CreateInstance(const Dia::Core::StringCRC& instanceId, const Json::Value& config)

// Phase Registration
// Usage in .cpp file:
//   DIA_REGISTER_PHASE(MyPhase) {
//       return new MyPhase(pu, instanceId);
//   }
#define DIA_REGISTER_PHASE(ClassName) \
	namespace { \
		class ClassName##_Factory : public Dia::Application::ITypeFactory<Dia::Application::Phase> { \
		public: \
			virtual Dia::Application::Phase* Create(Dia::Application::ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config) override; \
		}; \
		\
		Dia::Application::Phase* ClassName##_Factory::Create(Dia::Application::ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config) \
		{ \
			return CreateInstance(pu, instanceId, config); \
		} \
		\
		static Dia::Application::Phase* CreateInstance(Dia::Application::ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config); \
		\
		struct ClassName##_Registrar { \
			ClassName##_Registrar() { \
				static ClassName##_Factory factory; \
				Dia::Application::ApplicationTypeRegistry::Instance().RegisterPhaseType( \
					ClassName::kTypeId, &factory); \
			} \
		}; \
		static ClassName##_Registrar g_##ClassName##_registrar; \
	} \
	static Dia::Application::Phase* CreateInstance(Dia::Application::ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config)

// Module Registration
// Usage in .cpp file:
//   DIA_REGISTER_MODULE(MyModule) {
//       return new MyModule(pu, instanceId, Module::RunningEnum::kUpdate);
//   }
#define DIA_REGISTER_MODULE(ClassName) \
	namespace { \
		class ClassName##_Factory : public Dia::Application::ITypeFactory<Dia::Application::Module> { \
		public: \
			virtual Dia::Application::Module* Create(Dia::Application::ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config) override; \
		}; \
		\
		Dia::Application::Module* ClassName##_Factory::Create(Dia::Application::ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config) \
		{ \
			return CreateInstance(pu, instanceId, config); \
		} \
		\
		static Dia::Application::Module* CreateInstance(Dia::Application::ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config); \
		\
		struct ClassName##_Registrar { \
			ClassName##_Registrar() { \
				static ClassName##_Factory factory; \
				Dia::Application::ApplicationTypeRegistry::Instance().RegisterModuleType( \
					ClassName::kTypeId, &factory); \
			} \
		}; \
		static ClassName##_Registrar g_##ClassName##_registrar; \
	} \
	static Dia::Application::Module* CreateInstance(Dia::Application::ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config)
