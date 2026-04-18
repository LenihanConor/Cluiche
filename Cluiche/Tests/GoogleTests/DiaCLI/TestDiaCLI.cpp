// DiaCLI C++ integration tests
// Tests that DiaCLI commands execute correctly via system calls

#include <gtest/gtest.h>
#include <cstdio>
#include <string>
#include <memory>
#include <array>
#include <Windows.h>

namespace DiaCLI
{
	// Helper function to execute a command and capture output using cmd.exe
	std::string ExecuteCommand(const char* cmd, int& exitCode)
	{
		std::array<char, 256> buffer;
		std::string result;

		// Build full command with cmd /c for Windows compatibility
		std::string fullCmd = "cmd /c \"cd /d C:\\GitHub\\Cluiche 2>&1 && ";
		fullCmd += cmd;
		fullCmd += "\"";

		// Execute command and capture output
		FILE* pipe = _popen(fullCmd.c_str(), "r");
		if (!pipe)
		{
			exitCode = -1;
			return "";
		}

		while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
		{
			result += buffer.data();
		}

		exitCode = _pclose(pipe);
		return result;
	}

	// Test: Plugin Discovery - dia --help lists commands
	TEST(DiaCLI, PluginDiscoveryListsCommands)
	{
		int exitCode = 0;
		std::string output = ExecuteCommand(
			"C:\\Users\\clenihan\\AppData\\Roaming\\Python\\Python311-32\\Scripts\\poetry.exe run -C Dia\\DiaCLI dia --help",
			exitCode
		);

		// Should succeed
		EXPECT_EQ(0, exitCode) << "Output: " << output;

		// Should list core commands
		EXPECT_NE(output.find("command"), std::string::npos) << "Expected 'command' in help output";
		EXPECT_NE(output.find("setup"), std::string::npos) << "Expected 'setup' in help output";
		EXPECT_NE(output.find("show"), std::string::npos) << "Expected 'show' in help output";
		EXPECT_NE(output.find("api"), std::string::npos) << "Expected 'api' in help output";
		EXPECT_NE(output.find("test"), std::string::npos) << "Expected 'test' in help output";
	}

	// Test: Command Execution - dia test runs successfully
	TEST(DiaCLI, TestCommandExecutes)
	{
		int exitCode = 0;
		std::string output = ExecuteCommand(
			"C:\\Users\\clenihan\\AppData\\Roaming\\Python\\Python311-32\\Scripts\\poetry.exe run -C Dia\\DiaCLI dia test",
			exitCode
		);

		// Should succeed
		EXPECT_EQ(0, exitCode) << "Output: " << output;

		// Should output expected message
		EXPECT_NE(output.find("Plugin discovery is working"), std::string::npos);
	}

	// Test: Prefix Stripping - cli_ prefix is stripped
	TEST(DiaCLI, PrefixStrippingWorks)
	{
		int exitCode = 0;
		std::string output = ExecuteCommand(
			"C:\\Users\\clenihan\\AppData\\Roaming\\Python\\Python311-32\\Scripts\\poetry.exe run -C Dia\\DiaCLI dia prefixtest",
			exitCode
		);

		// Should succeed
		EXPECT_EQ(0, exitCode) << "Output: " << output;

		// Should output expected message
		EXPECT_NE(output.find("cli_ prefix was stripped correctly"), std::string::npos);
	}

	// Test: DiaAPI Bridge - api command exists and handles unavailable gracefully
	TEST(DiaCLI, DiaAPIBridgeCommandExists)
	{
		int exitCode = 0;
		std::string output = ExecuteCommand(
			"C:\\Users\\clenihan\\AppData\\Roaming\\Python\\Python311-32\\Scripts\\poetry.exe run -C Dia\\DiaCLI dia api list 2>&1",
			exitCode
		);

		// Command should execute (even if DiaAPI not available)
		// Exit code might be 0 or 256 (Windows returns 1 as 256)
		EXPECT_TRUE(exitCode == 0 || exitCode == 256 || exitCode == 1) << "Exit code: " << exitCode;

		// Should mention DiaAPI in some form (check stdout + stderr)
		bool hasDiaAPIReference =
			output.find("DiaAPI") != std::string::npos ||
			output.find("dia_api") != std::string::npos ||
			output.find("not available") != std::string::npos ||
			output.find("Error") != std::string::npos;

		EXPECT_TRUE(hasDiaAPIReference) << "Expected DiaAPI-related message in output: " << output;
	}

	// Test: Custom Command Appears - mycommand is discovered
	TEST(DiaCLI, CustomCommandAppears)
	{
		int exitCode = 0;
		std::string output = ExecuteCommand(
			"C:\\Users\\clenihan\\AppData\\Roaming\\Python\\Python311-32\\Scripts\\poetry.exe run -C Dia\\DiaCLI dia --help",
			exitCode
		);

		// Should succeed
		EXPECT_EQ(0, exitCode);

		// Should list mycommand (created during setup)
		EXPECT_NE(output.find("mycommand"), std::string::npos) << "Expected 'mycommand' in help output";
	}

	// Test: Exit Codes - Invalid command returns non-zero
	TEST(DiaCLI, InvalidCommandReturnsNonZero)
	{
		int exitCode = 0;
		std::string output = ExecuteCommand(
			"C:\\Users\\clenihan\\AppData\\Roaming\\Python\\Python311-32\\Scripts\\poetry.exe run -C Dia\\DiaCLI dia nonexistent-command",
			exitCode
		);

		// Should fail (Windows returns error codes as 256 * code)
		EXPECT_NE(0, exitCode) << "Expected non-zero exit code for invalid command";
	}
}
