////////////////////////////////////////////////////////////////////////////////
// Filename: DataDrivenApplicationExample.cpp
//
// Example demonstrating the Data-Driven Application System
// Shows how to:
//   1. Register types using DIA_REGISTER_* macros
//   2. Load applications from .diaapp manifest files
//   3. Use introspection API to query runtime topology
//   4. Hot reload manifests at runtime
//
////////////////////////////////////////////////////////////////////////////////

#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationModule.h>
#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
#include <DiaApplication/Loader/ApplicationLoader.h>
#include <DiaApplication/Introspection/ApplicationIntrospector.h>
#include <DiaCore/Core/Log.h>

using namespace Dia::Application;
using namespace Dia::Core;

////////////////////////////////////////////////////////////////////////////////
// Example Types
////////////////////////////////////////////////////////////////////////////////

// Example ProcessingUnit
class ExampleProcessingUnit : public ProcessingUnit
{
public:
	static const StringCRC kTypeId;

	ExampleProcessingUnit(const StringCRC& id, float hz)
		: ProcessingUnit(id, hz)
	{
		DIA_LOG("ExampleProcessingUnit created: %s", id.AsChar());
	}
};

const StringCRC ExampleProcessingUnit::kTypeId = StringCRC("ExampleProcessingUnit");

// Example Phase
class ExamplePhase : public Phase
{
public:
	static const StringCRC kTypeId;

	ExamplePhase(ProcessingUnit* pu, const StringCRC& id)
		: Phase(pu, id)
	{
		DIA_LOG("ExamplePhase created: %s", id.AsChar());
	}

	virtual bool FlaggedToStopUpdating() const override
	{
		return false; // Run forever
	}
};

const StringCRC ExamplePhase::kTypeId = StringCRC("ExamplePhase");

// Example Module (with configuration)
class ExampleModule : public Module
{
public:
	static const StringCRC kTypeId;

	ExampleModule(ProcessingUnit* pu, const StringCRC& id, float updateRate)
		: Module(pu, id, Module::RunningEnum::kUpdate)
		, mUpdateRate(updateRate)
	{
		DIA_LOG("ExampleModule created: %s (rate: %.2f)", id.AsChar(), updateRate);
	}

	virtual void DeserializeConfig(const Json::Value& config) override
	{
		if (config.isMember("updateRate"))
		{
			mUpdateRate = config["updateRate"].asFloat();
			DIA_LOG("ExampleModule config updated: rate=%.2f", mUpdateRate);
		}
	}

	virtual void SerializeConfig(Json::Value& config) const override
	{
		config["updateRate"] = mUpdateRate;
	}

private:
	float mUpdateRate;
};

const StringCRC ExampleModule::kTypeId = StringCRC("ExampleModule");

////////////////////////////////////////////////////////////////////////////////
// Type Registration (using static initialization macros)
////////////////////////////////////////////////////////////////////////////////

DIA_REGISTER_PROCESSING_UNIT(ExampleProcessingUnit)
{
	float hz = config.get("frequency_hz", 60.0f).asFloat();
	return new ExampleProcessingUnit(instanceId, hz);
}

DIA_REGISTER_PHASE(ExamplePhase)
{
	return new ExamplePhase(pu, instanceId);
}

DIA_REGISTER_MODULE(ExampleModule)
{
	float updateRate = config.get("updateRate", 30.0f).asFloat();
	return new ExampleModule(pu, instanceId, updateRate);
}

////////////////////////////////////////////////////////////////////////////////
// Example Usage Functions
////////////////////////////////////////////////////////////////////////////////

void Example1_LoadFromManifest(ApplicationTypeRegistry& registry)
{
	DIA_LOG("=== Example 1: Load Application from Manifest ===");

	ProcessingUnit* app = ApplicationLoader::LoadApplication(
		registry, "Cluiche/CluicheTest/ApplicationFlow/example_app.diaapp"
	);

	if (app)
	{
		DIA_LOG("Application loaded successfully!");

		// Start the application
		app->Start(nullptr);

		// Run a few updates
		for (int i = 0; i < 5; ++i)
		{
			app->Update();
		}

		// Stop and cleanup
		app->Stop();
		delete app;
	}
	else
	{
		DIA_LOG("Failed to load application (see errors above)");
	}
}

void Example2_LoadWithFallback(ApplicationTypeRegistry& registry)
{
	DIA_LOG("=== Example 2: Load with Fallback ===");

	auto fallbackFactory = []() -> ProcessingUnit* {
		DIA_LOG("Using fallback factory (manifest load failed)");
		auto* pu = new ExampleProcessingUnit(StringCRC("FallbackPU"), 60.0f);
		auto* phase = new ExamplePhase(pu, StringCRC("DefaultPhase"));
		pu->AddPhaseWithOwnership(Dia::Core::UniquePtr<Phase>(phase));
		pu->SetInitialPhase(phase);
		return pu;
	};

	ProcessingUnit* app = ApplicationLoader::LoadApplicationWithFallback(
		registry, "non_existent.diaapp", fallbackFactory
	);

	DIA_LOG("Application created: %s", app->GetUniqueId().AsChar());

	delete app;
}

void Example3_Introspection(ApplicationTypeRegistry& registry)
{
	DIA_LOG("=== Example 3: Introspection API ===");

	ProcessingUnit* app = ApplicationLoader::LoadApplication(
		registry, "Cluiche/CluicheTest/ApplicationFlow/example_app.diaapp"
	);

	if (app)
	{
		// Create introspector
		ApplicationIntrospector inspector(app);

		// Query phases
		const auto& phases = inspector.GetPhases();
		DIA_LOG("Phases (%d):", phases.Size());
		for (unsigned int i = 0; i < phases.Size(); ++i)
		{
			Phase* phase = phases[i];
			DIA_LOG("  - %s (state: %d)", phase->GetUniqueId().AsChar(), (int)phase->GetState());
		}

		// Query modules
		const auto& modules = inspector.GetModules();
		DIA_LOG("Modules (%d):", modules.Size());
		for (unsigned int i = 0; i < modules.Size(); ++i)
		{
			Module* module = modules[i];
			DIA_LOG("  - %s (state: %d)", module->GetUniqueId().AsChar(), (int)module->GetState());
		}

		// Query transitions
		const auto& transitions = inspector.GetTransitions();
		DIA_LOG("Phase Transitions (%d):", transitions.Size());
		for (unsigned int i = 0; i < transitions.Size(); ++i)
		{
			const auto& transition = transitions[i];
			DIA_LOG("  - %s -> %s",
				transition.fromPhase.AsChar(),
				transition.toPhase.AsChar());
		}

		delete app;
	}
}

void Example4_HotReload(ApplicationTypeRegistry& registry)
{
	DIA_LOG("=== Example 4: Hot Reload ===");

	ProcessingUnit* app = ApplicationLoader::LoadApplication(
		registry, "Cluiche/CluicheTest/ApplicationFlow/example_app.diaapp"
	);

	if (app)
	{
		app->Start(nullptr);

		DIA_LOG("Initial state:");
		ApplicationIntrospector inspector(app);
		DIA_LOG("  Modules: %d", inspector.GetModules().Size());

		DIA_LOG("Reloading manifest...");

		ApplicationManifestLoader loader(registry);
		ManifestValidationResult result = loader.ReloadManifest(
			"Cluiche/CluicheTest/ApplicationFlow/example_app.diaapp",
			app
		);

		if (result == ManifestValidationResult::kSuccess)
		{
			DIA_LOG("Hot reload successful!");

			DIA_LOG("New state:");
			DIA_LOG("  Modules: %d", inspector.GetModules().Size());
		}
		else
		{
			DIA_LOG("Hot reload failed: %s",
				ManifestValidationError::GetResultString(result));
		}

		app->Stop();
		delete app;
	}
}

void Example5_QueryRegisteredTypes(ApplicationTypeRegistry& registry)
{
	DIA_LOG("=== Example 5: Query Registered Types ===");

	// Query registered ProcessingUnit types
	const auto& puTypes = registry.GetRegisteredProcessingUnitTypes();
	DIA_LOG("Registered ProcessingUnit Types (%d):", puTypes.Size());
	for (unsigned int i = 0; i < puTypes.Size(); ++i)
	{
		DIA_LOG("  - %s", puTypes[i].AsChar());
	}

	// Query registered Phase types
	const auto& phaseTypes = registry.GetRegisteredPhaseTypes();
	DIA_LOG("Registered Phase Types (%d):", phaseTypes.Size());
	for (unsigned int i = 0; i < phaseTypes.Size(); ++i)
	{
		DIA_LOG("  - %s", phaseTypes[i].AsChar());
	}

	// Query registered Module types
	const auto& moduleTypes = registry.GetRegisteredModuleTypes();
	DIA_LOG("Registered Module Types (%d):", moduleTypes.Size());
	for (unsigned int i = 0; i < moduleTypes.Size(); ++i)
	{
		DIA_LOG("  - %s", moduleTypes[i].AsChar());
	}

	// Check if specific type is registered
	if (registry.IsModuleTypeRegistered(StringCRC("ExampleModule")))
	{
		DIA_LOG("ExampleModule is registered!");
	}
}

////////////////////////////////////////////////////////////////////////////////
// Main Entry Point (for standalone example)
////////////////////////////////////////////////////////////////////////////////

int main()
{
	DIA_LOG("Data-Driven Application System Examples");
	DIA_LOG("========================================");

	ApplicationTypeRegistry registry;
	registry.DrainPendingRegistrations();

	Example1_LoadFromManifest(registry);
	Example2_LoadWithFallback(registry);
	Example3_Introspection(registry);
	Example4_HotReload(registry);
	Example5_QueryRegisteredTypes(registry);

	DIA_LOG("All examples completed!");
	return 0;
}
