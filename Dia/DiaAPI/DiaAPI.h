////////////////////////////////////////////////////////////////////////////////
// Filename: DiaAPI.h
// Description: Main public header for DiaAPI - Command registration and execution API
// System spec: docs/specs/systems/dia/diaapi.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

// DiaAPI is a plugin-based command registration framework that provides command registration,
// argument parsing, help generation, and automatic Python exposure.
// Other Dia modules register commands via this API.

// Feature: command-registry
#include "CommandRegistry/CommandRegistry.h"

// Feature: cli-parser
#include "Parser/ArgumentParser.h"

// Feature: help-system
#include "Help/HelpSystem.h"

// Feature: event-system
#include "Events/EventSystem.h"

// Feature: python-bindings
#include "PythonBindings/PythonBindings.h"
