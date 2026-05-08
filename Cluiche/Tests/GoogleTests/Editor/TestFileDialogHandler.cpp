#include <gtest/gtest.h>
#include <DiaEditor/UI/FileDialogHandler.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstdlib>

using namespace Dia::Editor;

namespace
{
	struct HeadlessGuard
	{
		HeadlessGuard() { _putenv_s("DIA_HEADLESS", "1"); }
		~HeadlessGuard() { _putenv_s("DIA_HEADLESS", ""); }
	};
}

TEST(FileDialogHandler, HandleOpenFileDialog_EmptyData_ReturnsFalseOnCancel)
{
	HeadlessGuard guard;
	Json::Value data;
	Json::Value result = FileDialogHandler::HandleOpenFileDialog(data);

	EXPECT_TRUE(result.isMember("success"));
	EXPECT_TRUE(result["success"].isBool());
	EXPECT_FALSE(result["success"].asBool());
}

TEST(FileDialogHandler, HandleSaveFileDialog_EmptyData_ReturnsFalseOnCancel)
{
	HeadlessGuard guard;
	Json::Value data;
	Json::Value result = FileDialogHandler::HandleSaveFileDialog(data);

	EXPECT_TRUE(result.isMember("success"));
	EXPECT_TRUE(result["success"].isBool());
	EXPECT_FALSE(result["success"].asBool());
}

TEST(FileDialogHandler, HandleOpenFileDialog_WithFilters_DoesNotCrash)
{
	HeadlessGuard guard;
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
	HeadlessGuard guard;
	Json::Value data;
	data["initial_dir"] = "C:\\";
	data["title"] = "Test with initial dir";

	Json::Value result = FileDialogHandler::HandleOpenFileDialog(data);

	EXPECT_TRUE(result.isMember("success"));
}

TEST(FileDialogHandler, HandleOpenFileDialog_SemicolonExtensions_DoesNotCrash)
{
	HeadlessGuard guard;
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
	HeadlessGuard guard;
	Json::Value data;
	data["title"] = "Save Test";
	data["default_ext"] = "json";

	Json::Value result = FileDialogHandler::HandleSaveFileDialog(data);

	EXPECT_TRUE(result.isMember("success"));
}
