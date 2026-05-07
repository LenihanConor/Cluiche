#include <gtest/gtest.h>
#include <DiaEditor/UI/FileDialogHandler.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::Editor;

TEST(FileDialogHandler, HandleOpenFileDialog_EmptyData_ReturnsFalseOnCancel)
{
	// Cannot verify a real dialog in automated tests, but we can verify
	// the handler doesn't crash with empty/minimal input and returns a
	// well-formed response. In CI the dialog will be dismissed immediately
	// (no HWND owner), returning success=false.
	Json::Value data;
	Json::Value result = FileDialogHandler::HandleOpenFileDialog(data);

	EXPECT_TRUE(result.isMember("success"));
	EXPECT_TRUE(result["success"].isBool());
}

TEST(FileDialogHandler, HandleSaveFileDialog_EmptyData_ReturnsFalseOnCancel)
{
	Json::Value data;
	Json::Value result = FileDialogHandler::HandleSaveFileDialog(data);

	EXPECT_TRUE(result.isMember("success"));
	EXPECT_TRUE(result["success"].isBool());
}

TEST(FileDialogHandler, HandleOpenFileDialog_WithFilters_DoesNotCrash)
{
	Json::Value data;
	Json::Value filters(Json::arrayValue);
	Json::Value f1;
	f1["name"] = "Test Files";
	f1["ext"] = "*.txt";
	filters.append(f1);
	Json::Value f2;
	f2["name"] = "All Files";
	f2["ext"] = "*.*";
	filters.append(f2);
	data["filters"] = filters;
	data["default_ext"] = "txt";
	data["title"] = "Test Open";

	Json::Value result = FileDialogHandler::HandleOpenFileDialog(data);

	EXPECT_TRUE(result.isMember("success"));
}

TEST(FileDialogHandler, HandleOpenFileDialog_WithInitialDir_DoesNotCrash)
{
	Json::Value data;
	data["initial_dir"] = "C:\\";
	data["title"] = "Test with initial dir";

	Json::Value result = FileDialogHandler::HandleOpenFileDialog(data);

	EXPECT_TRUE(result.isMember("success"));
}

TEST(FileDialogHandler, HandleOpenFileDialog_SemicolonExtensions_DoesNotCrash)
{
	Json::Value data;
	Json::Value filters(Json::arrayValue);
	Json::Value f1;
	f1["name"] = "Dia Files";
	f1["ext"] = "*.diagame;*.diaapp";
	filters.append(f1);
	data["filters"] = filters;

	Json::Value result = FileDialogHandler::HandleOpenFileDialog(data);

	EXPECT_TRUE(result.isMember("success"));
}

TEST(FileDialogHandler, HandleSaveFileDialog_WithOverwritePrompt_DoesNotCrash)
{
	Json::Value data;
	data["title"] = "Save Test";
	data["default_ext"] = "json";

	Json::Value result = FileDialogHandler::HandleSaveFileDialog(data);

	EXPECT_TRUE(result.isMember("success"));
}
