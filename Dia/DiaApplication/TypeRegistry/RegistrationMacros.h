#pragma once

#include "ApplicationTypeRegistry.h"
#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationModule.h>

// Registration macros for ProcessingUnit/Phase/Module types
// These use static initialization to automatically register types before main()
//
// The function body provided by the user becomes the definition of a static
// CreateInstance function inside an anonymous namespace. The factory, registrar,
// and CreateInstance all live in the same anonymous namespace.
//
// Usage pattern:
//   DIA_REGISTER_PROCESSING_UNIT(ClassName) {
//       return new ClassName(instanceId, config.get("hz", 60.0f).asFloat());
//   }

#define DIA_REGISTER_PROCESSING_UNIT(ClassName) \
	static Dia::Application::ProcessingUnit* ClassName##_CreateInstance(const Dia::Core::StringCRC& instanceId, const Json::Value& config); \
	namespace { \
		class ClassName##_Factory : public Dia::Application::ITypeFactory<Dia::Application::ProcessingUnit> { \
		public: \
			virtual Dia::Application::ProcessingUnit* Create(const Dia::Core::StringCRC& instanceId, const Json::Value& config) override { \
				return ClassName##_CreateInstance(instanceId, config); \
			} \
		}; \
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
	static Dia::Application::ProcessingUnit* ClassName##_CreateInstance(const Dia::Core::StringCRC& instanceId, const Json::Value& config)

#define DIA_REGISTER_PHASE(ClassName) \
	static Dia::Application::Phase* ClassName##_CreateInstance(Dia::Application::ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config); \
	namespace { \
		class ClassName##_Factory : public Dia::Application::ITypeFactory<Dia::Application::Phase> { \
		public: \
			virtual Dia::Application::Phase* Create(Dia::Application::ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config) override { \
				return ClassName##_CreateInstance(pu, instanceId, config); \
			} \
		}; \
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
	static Dia::Application::Phase* ClassName##_CreateInstance(Dia::Application::ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config)

#define DIA_REGISTER_MODULE(ClassName) \
	static Dia::Application::Module* ClassName##_CreateInstance(Dia::Application::ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config); \
	namespace { \
		class ClassName##_Factory : public Dia::Application::ITypeFactory<Dia::Application::Module> { \
		public: \
			virtual Dia::Application::Module* Create(Dia::Application::ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config) override { \
				return ClassName##_CreateInstance(pu, instanceId, config); \
			} \
		}; \
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
	static Dia::Application::Module* ClassName##_CreateInstance(Dia::Application::ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config)
