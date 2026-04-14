////////////////////////////////////////////////////////////////////////////////
// Filename: CommandRegistry.h
// Description: Command registration and discovery system for DiaCLI
// Feature spec: docs/specs/features/dia/diacli/command-registry.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <functional>
#include <unordered_map>

namespace Dia
{
	namespace CLI
	{
		// Forward declarations
		struct CommandArgs;
		struct CommandInfo;
		class CommandRegistry;

		////////////////////////////////////////////////////////////////////////////////
		// Command callback signature
		// Parameters: CommandArgs with positional/named/flags
		// Returns: Exit code (0 = success, non-zero = error)
		////////////////////////////////////////////////////////////////////////////////
		using CommandCallback = std::function<int(const CommandArgs& args)>;

		////////////////////////////////////////////////////////////////////////////////
		// Command arguments structure (parsed by cli-parser feature)
		// NOTE: Using std::unordered_map instead of DiaCore::HashTable due to critical
		// bugs in HashTable copy/move constructors that cause hangs
		////////////////////////////////////////////////////////////////////////////////
		struct CommandArgs
		{
			Dia::Core::Containers::DynamicArrayC<const char*, 32> positionalArgs;
			std::unordered_map<unsigned int, const char*> namedArgs;  // --key=value (key is StringCRC.Value())
			std::unordered_map<unsigned int, bool> flags;              // --flag (key is StringCRC.Value())
		};

		////////////////////////////////////////////////////////////////////////////////
		// Command metadata
		////////////////////////////////////////////////////////////////////////////////
		struct CommandInfo
		{
			Dia::Core::StringCRC name;              // Command name (e.g., "compile-asset")
			const char* description;                 // Human-readable description
			Dia::Core::StringCRC category;          // Category for grouping (e.g., "build", "asset")
			const char* owner;                       // System that registered it (e.g., "DiaAssets")
			const char* version;                     // Version string (e.g., "1.0.0")
			const char* example;                     // Usage example (e.g., "compile-asset model.fbx --format=gltf")
			CommandCallback callback;                // Function to execute
		};

		////////////////////////////////////////////////////////////////////////////////
		// Command registry class (opaque - no direct access)
		////////////////////////////////////////////////////////////////////////////////
		class CommandRegistry
		{
			CommandRegistry(const CommandRegistry&) = delete;
			CommandRegistry& operator=(const CommandRegistry&) = delete;
		private:
			CommandRegistry() = default;
		};

		////////////////////////////////////////////////////////////////////////////////
		// Lifecycle Management
		////////////////////////////////////////////////////////////////////////////////

		// Initialize the command registry
		void Initialize();

		// Shutdown the command registry
		void Shutdown();

		// Check if registry is initialized
		bool IsInitialized();

		////////////////////////////////////////////////////////////////////////////////
		// Command Registration
		////////////////////////////////////////////////////////////////////////////////

		// Register a new command
		// Returns true on success, false if command name is invalid or duplicate
		// Validates:
		//   - Command name format (lowercase + hyphens only)
		//   - Description, category, owner are non-null
		//   - Name is unique (no duplicates)
		bool RegisterCommand(const CommandInfo& info);

		////////////////////////////////////////////////////////////////////////////////
		// Command Query
		////////////////////////////////////////////////////////////////////////////////

		// Get command info by name
		// Returns nullptr if command not found
		const CommandInfo* GetCommand(const Dia::Core::StringCRC& name);

		// List all registered commands
		Dia::Core::Containers::DynamicArrayC<const CommandInfo*, 64> ListCommands();

		// Get commands by category
		// Returns empty array if category not found
		Dia::Core::Containers::DynamicArrayC<const CommandInfo*, 64> GetCommandsByCategory(const Dia::Core::StringCRC& category);
	}
}
