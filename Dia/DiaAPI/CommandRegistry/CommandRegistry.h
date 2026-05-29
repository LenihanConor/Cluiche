////////////////////////////////////////////////////////////////////////////////
// Filename: CommandRegistry.h
// Description: Command registration and discovery system for DiaAPI
// Feature spec: docs/specs/features/dia/diaapi/command-registry.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>
#include <functional>

namespace Dia
{
	namespace API
	{
		// Forward declarations
		struct CommandArgs;
		struct CommandInfo;

		////////////////////////////////////////////////////////////////////////////////
		// Command callback signature
		// std::function allows lambdas with captures (used by debug commands)
		////////////////////////////////////////////////////////////////////////////////
		using CommandCallback = std::function<int(const CommandArgs& args)>;

		////////////////////////////////////////////////////////////////////////////////
		// Fixed-size flat map entry (key = StringCRC value, value = const char*)
		////////////////////////////////////////////////////////////////////////////////
		struct NamedArgEntry
		{
			unsigned int key;
			const char* value;
		};

		struct FlagEntry
		{
			unsigned int key;
			bool value;
		};

		////////////////////////////////////////////////////////////////////////////////
		// Command arguments structure (parsed by cli-parser feature)
		// Uses Dia containers only (PD-004 compliant)
		////////////////////////////////////////////////////////////////////////////////
		struct CommandArgs
		{
			Dia::Core::Containers::DynamicArrayC<const char*, 32> positionalArgs;
			Dia::Core::Containers::DynamicArrayC<NamedArgEntry, 32> namedArgs;
			Dia::Core::Containers::DynamicArrayC<FlagEntry, 32> flags;

			const char* GetNamedArg(unsigned int crcValue) const;
			bool HasFlag(unsigned int crcValue) const;
			void SetNamedArg(unsigned int crcValue, const char* value);
			void SetFlag(unsigned int crcValue, bool value);
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
		bool RegisterCommand(const CommandInfo& info);

		////////////////////////////////////////////////////////////////////////////////
		// Command Execution
		////////////////////////////////////////////////////////////////////////////////

		// Execute a registered command by name
		// Returns: exit code (0=success, 3=command not found)
		int ExecuteCommand(const Dia::Core::StringCRC& commandName, const CommandArgs& args);

		// Main entry point for CLI usage (parses argv, dispatches command)
		// Returns: exit code
		int RunCLI(int argc, char* argv[]);

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

		////////////////////////////////////////////////////////////////////////////////
		// JSON command callback path (AC2-AC3)
		//
		// Parallel to the CLI callback path. Commands registered here return a
		// structured Json::Value response rather than an int exit code.
		// All responses are wrapped in {"success": true/false, "data"/"error": ...}.
		// Guard fn must be idempotent and side-effect-free.
		////////////////////////////////////////////////////////////////////////////////

		// JSON-oriented callback — returns the response data (wrapped by framework).
		// On success: return your data object; framework wraps as {"success":true,"data":...}
		// On error: throw or return Json::Value(Json::nullValue) to signal failure.
		using CommandCallbackJson = std::function<Json::Value(const Json::Value& params)>;

		struct CommandInfoJson
		{
			Dia::Core::StringCRC name;
			const char* description = nullptr;
			Dia::Core::StringCRC category;
			const char* owner = nullptr;
			CommandCallbackJson callback;
		};

		// Register a JSON-callback command.
		// Logs a warning if a CLI command with the same name is also registered.
		// Returns false if name is invalid or duplicate JSON command.
		bool RegisterCommandJson(const CommandInfoJson& info);

		// Execute a JSON-callback command by name.
		// Returns {"success":true,"data":{...}} on success.
		// Returns {"success":false,"error":"command not found"} for unknown commands.
		// Returns {"success":false,"error":"<message>"} on handler exception.
		Json::Value ExecuteCommandJson(const Dia::Core::StringCRC& name, const Json::Value& params);

	} // namespace API
} // namespace Dia
