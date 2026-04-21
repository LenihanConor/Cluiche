#pragma once

#include "ApplicationTypeRegistry.h"
#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationModule.h>

// Registration macros for ProcessingUnit/Phase/Module types
//
// These run at static-init time (before main()). Rather than writing directly
// into a singleton registry, they enqueue into a POD pending-registration list.
// Call ApplicationTypeRegistry::DrainPendingRegistrations() on your registry
// instance at startup to transfer all enqueued entries.
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
				Dia::Application::PendingRegistrationQueue& q = Dia::Application::GetPendingRegistrationQueue(); \
				if (q.count < Dia::Application::kMaxPendingRegistrations) { \
					q.entries[q.count++] = { Dia::Application::PendingRegistrationKind::ProcessingUnit, ClassName::kTypeId, &factory }; \
				} \
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
				Dia::Application::PendingRegistrationQueue& q = Dia::Application::GetPendingRegistrationQueue(); \
				if (q.count < Dia::Application::kMaxPendingRegistrations) { \
					q.entries[q.count++] = { Dia::Application::PendingRegistrationKind::Phase, ClassName::kTypeId, &factory }; \
				} \
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
				Dia::Application::PendingRegistrationQueue& q = Dia::Application::GetPendingRegistrationQueue(); \
				if (q.count < Dia::Application::kMaxPendingRegistrations) { \
					q.entries[q.count++] = { Dia::Application::PendingRegistrationKind::Module, ClassName::kTypeId, &factory }; \
				} \
			} \
		}; \
		static ClassName##_Registrar g_##ClassName##_registrar; \
	} \
	static Dia::Application::Module* ClassName##_CreateInstance(Dia::Application::ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config)
