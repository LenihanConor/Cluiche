#include <gtest/gtest.h>
#include <DiaPipelineEditor/Internal/PipelineTargetParser.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>

namespace
{
	class TempToml
	{
	public:
		TempToml()
		{
			char tmpDir[MAX_PATH];
			GetTempPathA(MAX_PATH, tmpDir);
			GetTempFileNameA(tmpDir, "toml", 0, mPath);
			DeleteFileA(mPath);
		}

		~TempToml()
		{
			DeleteFileA(mPath);
		}

		const char* Path() const { return mPath; }

		void Write(const char* content) const
		{
			FILE* f = nullptr;
			fopen_s(&f, mPath, "wb");
			if (f)
			{
				fwrite(content, 1, strlen(content), f);
				fclose(f);
			}
		}

	private:
		char mPath[MAX_PATH];
	};
}

TEST(PipelineTargetParser, ParsesBasicTargets)
{
	TempToml tmp;
	tmp.Write(
		"[global]\n"
		"default_target = \"googletest\"\n"
		"\n"
		"[targets.googletest]\n"
		"project = \"Tests.vcxproj\"\n"
		"\n"
		"[targets.cluicheeditor]\n"
		"project = \"Editor.vcxproj\"\n"
	);

	Json::Value result = Dia::PipelineEditor::Internal::ParsePipelineTargets(tmp.Path());
	ASSERT_EQ(result.size(), 2u);
	EXPECT_STREQ(result[0].asCString(), "googletest");
	EXPECT_STREQ(result[1].asCString(), "cluicheeditor");
}

TEST(PipelineTargetParser, HiddenTargetsExcluded)
{
	TempToml tmp;
	tmp.Write(
		"[targets.googletest]\n"
		"project = \"Tests.vcxproj\"\n"
		"\n"
		"[targets.diasfml]\n"
		"project = \"DiaSFML.vcxproj\"\n"
		"hidden = true\n"
		"\n"
		"[targets.cluicheeditor]\n"
		"project = \"Editor.vcxproj\"\n"
	);

	Json::Value result = Dia::PipelineEditor::Internal::ParsePipelineTargets(tmp.Path());
	ASSERT_EQ(result.size(), 2u);
	EXPECT_STREQ(result[0].asCString(), "googletest");
	EXPECT_STREQ(result[1].asCString(), "cluicheeditor");
}

TEST(PipelineTargetParser, HiddenFalseNotExcluded)
{
	TempToml tmp;
	tmp.Write(
		"[targets.googletest]\n"
		"project = \"Tests.vcxproj\"\n"
		"hidden = false\n"
	);

	Json::Value result = Dia::PipelineEditor::Internal::ParsePipelineTargets(tmp.Path());
	ASSERT_EQ(result.size(), 1u);
	EXPECT_STREQ(result[0].asCString(), "googletest");
}

TEST(PipelineTargetParser, SkipsSubtables)
{
	TempToml tmp;
	tmp.Write(
		"[targets.googletest]\n"
		"project = \"Tests.vcxproj\"\n"
		"\n"
		"[targets.googletest.deploy]\n"
		"files = []\n"
		"\n"
		"[targets.googletest.build_deps]\n"
		"protobuf = true\n"
	);

	Json::Value result = Dia::PipelineEditor::Internal::ParsePipelineTargets(tmp.Path());
	ASSERT_EQ(result.size(), 1u);
	EXPECT_STREQ(result[0].asCString(), "googletest");
}

TEST(PipelineTargetParser, AllHiddenReturnsEmpty)
{
	TempToml tmp;
	tmp.Write(
		"[targets.diasfml]\n"
		"hidden = true\n"
		"\n"
		"[targets.diauiultralight]\n"
		"hidden = true\n"
	);

	Json::Value result = Dia::PipelineEditor::Internal::ParsePipelineTargets(tmp.Path());
	EXPECT_EQ(result.size(), 0u);
}

TEST(PipelineTargetParser, MissingFileReturnsEmpty)
{
	Json::Value result = Dia::PipelineEditor::Internal::ParsePipelineTargets("C:\\nonexistent\\pipeline.toml");
	EXPECT_EQ(result.size(), 0u);
}

TEST(PipelineTargetParser, HiddenOnLastTargetInFile)
{
	TempToml tmp;
	tmp.Write(
		"[targets.googletest]\n"
		"project = \"Tests.vcxproj\"\n"
		"\n"
		"[targets.diasfml]\n"
		"project = \"DiaSFML.vcxproj\"\n"
		"hidden = true\n"
	);

	Json::Value result = Dia::PipelineEditor::Internal::ParsePipelineTargets(tmp.Path());
	ASSERT_EQ(result.size(), 1u);
	EXPECT_STREQ(result[0].asCString(), "googletest");
}
