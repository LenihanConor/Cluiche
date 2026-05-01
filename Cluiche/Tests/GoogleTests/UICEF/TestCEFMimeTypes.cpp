////////////////////////////////////////////////////////////////////////////////
// Tests for CEFUtils MIME type detection
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaUICEF/CEFUtils.h>

using namespace Dia::UICEF::Utils;

TEST(CEFMimeTypes, Html_ReturnsTextHtml)
{
	EXPECT_STREQ("text/html", GetMimeType("index.html"));
	EXPECT_STREQ("text/html", GetMimeType("page.htm"));
}

TEST(CEFMimeTypes, Css_ReturnsTextCss)
{
	EXPECT_STREQ("text/css", GetMimeType("styles/main.css"));
}

TEST(CEFMimeTypes, Js_ReturnsApplicationJavascript)
{
	EXPECT_STREQ("application/javascript", GetMimeType("app.js"));
}

TEST(CEFMimeTypes, Json_ReturnsApplicationJson)
{
	EXPECT_STREQ("application/json", GetMimeType("data.json"));
}

TEST(CEFMimeTypes, Png_ReturnsImagePng)
{
	EXPECT_STREQ("image/png", GetMimeType("logo.png"));
}

TEST(CEFMimeTypes, Jpg_ReturnsImageJpeg)
{
	EXPECT_STREQ("image/jpeg", GetMimeType("photo.jpg"));
	EXPECT_STREQ("image/jpeg", GetMimeType("photo.jpeg"));
}

TEST(CEFMimeTypes, Svg_ReturnsImageSvgXml)
{
	EXPECT_STREQ("image/svg+xml", GetMimeType("icon.svg"));
}

TEST(CEFMimeTypes, Woff2_ReturnsFontWoff2)
{
	EXPECT_STREQ("font/woff2", GetMimeType("font.woff2"));
}

TEST(CEFMimeTypes, UnknownExtension_ReturnsOctetStream)
{
	EXPECT_STREQ("application/octet-stream", GetMimeType("file.xyz"));
}

TEST(CEFMimeTypes, NoExtension_ReturnsOctetStream)
{
	EXPECT_STREQ("application/octet-stream", GetMimeType("README"));
}
