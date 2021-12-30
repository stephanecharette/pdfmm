/**
 * Copyright (C) 2007 by Dominik Seichter <domseichter@web.de>
 * Copyright (C) 2021 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Library General Public 2.0 or later.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#include <PdfTest.h>

using namespace std;
using namespace mm;

static void Test(const string_view& buffer, PdfDataType dataType, string_view expected = { });
static void TestStream(const string_view& buffer, const char* tokens[]);
static void TestStreamIsNextToken(const string_view& buffer, const char* tokens[]);

TEST_CASE("testArrays")
{
    Test("[]", PdfDataType::Array, "[ ]");
    Test("[ ]", PdfDataType::Array);
    Test("[ / ]", PdfDataType::Array, "[ / ]"); // empty names are legal, too!
    Test("[ / [ ] ]", PdfDataType::Array, "[ / [ ] ]"); // empty names are legal, too!
    Test("[/[]]", PdfDataType::Array, "[ / [ ] ]"); // empty names are legal, too!
    Test("[ 1 2 3 4 ]", PdfDataType::Array);
    Test("[1 2 3 4]", PdfDataType::Array, "[ 1 2 3 4 ]");
    Test("[ 2 (Hallo Welt!) 3.500000 /FMC ]", PdfDataType::Array);
    Test("[ [ 1 2 ] (Hallo Welt!) 3.500000 /FMC ]", PdfDataType::Array);
    Test("[/ImageA/ImageB/ImageC]", PdfDataType::Array, "[ /ImageA /ImageB /ImageC ]");
    Test("[<530464995927cef8aaf46eb953b93373><530464995927cef8aaf46eb953b93373>]", PdfDataType::Array, "[ <530464995927CEF8AAF46EB953B93373> <530464995927CEF8AAF46EB953B93373> ]");
    Test("[ 2 0 R (Test Data) 4 << /Key /Data >> 5 0 R ]", PdfDataType::Array, "[ 2 0 R (Test Data) 4 <<\n/Key /Data\n>> 5 0 R ]");
    Test("[<</key/name>>2 0 R]", PdfDataType::Array, "[ <<\n/key /name\n>> 2 0 R ]");
    Test("[<<//name>>2 0 R]", PdfDataType::Array, "[ <<\n/ /name\n>> 2 0 R ]");
    Test("[ 27.673200 27.673200 566.256000 651.295000 ]", PdfDataType::Array);
}

TEST_CASE("testBool")
{
    Test("false", PdfDataType::Bool);
    Test("true", PdfDataType::Bool);
}

TEST_CASE("testHexString")
{
    Test("<FFEB0400A0CC>", PdfDataType::String);
    Test("<FFEB0400A0C>", PdfDataType::String, "<FFEB0400A0C0>");
    Test("<>", PdfDataType::String);
}

TEST_CASE("testName")
{
    Test("/Type", PdfDataType::Name);
    Test("/Length", PdfDataType::Name);
    Test("/Adobe#20Green", PdfDataType::Name);
    Test("/$$", PdfDataType::Name);
    Test("/1.2", PdfDataType::Name);
    Test("/.notdef", PdfDataType::Name);
    Test("/@pattern", PdfDataType::Name);
    Test("/A;Name_With-Various***Characters?", PdfDataType::Name);
    Test("/", PdfDataType::Name); // empty names are legal, too!
}

TEST_CASE("testName2")
{
    // Some additional tests, which cause errors for Sebastian Loch
    string_view buffer = "/CheckBox#C3#9Cbersetzungshinweis";
    PdfMemoryInputDevice device(buffer);

    PdfTokenizer tokenizer;
    PdfVariant variant;
    REQUIRE(tokenizer.TryReadNextVariant(device, variant));

    PdfName name(variant.GetName());
    auto nameStr = name.GetString();

    REQUIRE(variant.GetName() == name);
    REQUIRE(name == nameStr);

    INFO(cmn::Format("!!! Name=[{}]\n", variant.GetName().GetString()));
    INFO(cmn::Format("!!! Name2=[{}]\n", name.GetString()));
}

TEST_CASE("testNull")
{
    Test("null", PdfDataType::Null);
}

TEST_CASE("testNumbers")
{
    Test("145", PdfDataType::Number);
    Test("-12", PdfDataType::Number);
    Test("3.141230", PdfDataType::Real);
    Test("-2.970000", PdfDataType::Real);
    Test("0", PdfDataType::Number);
    Test("4.", PdfDataType::Real, "4.000000");
}

TEST_CASE("testReference")
{
    Test("2 0 R", PdfDataType::Reference);
    Test("3 0 R", PdfDataType::Reference);
    Test("4 1 R", PdfDataType::Reference);
}

TEST_CASE("testString")
{
    // testing strings
    Test("(Hallo Welt!)", PdfDataType::String);
    Test("(Hallo \\(schöne\\) Welt!)", PdfDataType::String);
    Test("(Balanced () brackets are (ok ()) in PDF Strings)", PdfDataType::String,
        "(Balanced \\(\\) brackets are \\(ok \\(\\)\\) in PDF Strings)");
    Test("()", PdfDataType::String);

    // Test octal strings
    Test("(Test: \\064)", PdfDataType::String, "(Test: \064)");
    Test("(Test: \\064\\064)", PdfDataType::String, "(Test: \064\064)");
    Test("(Test: \\0645)", PdfDataType::String, "(Test: 45)");
    Test("(Test: \\478)", PdfDataType::String, "(Test: '8)");

    // Test line breaks 
    Test("(Hallo\nWelt!)", PdfDataType::String, "(Hallo\\nWelt!)");
    Test("(These \\\ntwo strings \\\nare the same.)", PdfDataType::String,
        "(These two strings are the same.)");

    // Test escape sequences
    Test("(Hallo\\nWelt!)", PdfDataType::String, "(Hallo\\nWelt!)");
    Test("(Hallo\\rWelt!)", PdfDataType::String, "(Hallo\\rWelt!)");
    Test("(Hallo\\tWelt!)", PdfDataType::String, "(Hallo\\tWelt!)");
    Test("(Hallo\\bWelt!)", PdfDataType::String, "(Hallo\\bWelt!)");
    Test("(Hallo\\fWelt!)", PdfDataType::String, "(Hallo\\fWelt!)");
}

TEST_CASE("testDictionary")
{
    const char* pszDictIn =
        "<< /CheckBox#C3#9Cbersetzungshinweis(False)/Checkbox#C3#9Cbersetzungstabelle(False) >>";
    const char* pszDictOut =
        "<<\n/CheckBox#C3#9Cbersetzungshinweis (False)\n/Checkbox#C3#9Cbersetzungstabelle (False)\n>>";

    Test(pszDictIn, PdfDataType::Dictionary, pszDictOut);
}

TEST_CASE("testTokens")
{
    const char* pszBuffer = "613 0 obj"
        "<< /Length 141 /Filter [ /ASCII85Decode /FlateDecode ] >>"
        "endobj";

    const char* pszTokens[] = {
        "613", "0", "obj", "<<", "/", "Length", "141", "/", "Filter", "[", "/",
        "ASCII85Decode", "/", "FlateDecode", "]", ">>", "endobj", NULL
    };

    TestStream(pszBuffer, pszTokens);
    TestStreamIsNextToken(pszBuffer, pszTokens);
}

TEST_CASE("testComments")
{
    const char* pszBuffer = "613 0 obj\n"
        "% A comment that should be ignored\n"
        "<< /Length 141 /Filter\n% A comment in a dictionary\n[ /ASCII85Decode /FlateDecode ] >>"
        "endobj";

    const char* pszTokens[] = {
        "613", "0", "obj", "<<", "/", "Length", "141", "/", "Filter", "[", "/",
        "ASCII85Decode", "/", "FlateDecode", "]", ">>", "endobj", NULL
    };

    TestStream(pszBuffer, pszTokens);
    TestStreamIsNextToken(pszBuffer, pszTokens);
}

TEST_CASE("testLocale")
{
    // Test with a locale thate uses "," instead of "." for doubles 
    char* old = setlocale(LC_ALL, "de_DE");

    const char* pszNumber = "3.140000";
    Test(pszNumber, PdfDataType::Real, pszNumber);

    setlocale(LC_ALL, old);
}

void Test(const string_view& buffer, PdfDataType dataType, string_view expected)
{
    expected = expected.empty() ? buffer : expected;

    INFO(cmn::Format("Testing with value: {}", buffer));

    PdfMemoryInputDevice device(buffer);
    PdfTokenizer tokenizer;
    PdfVariant variant;
    REQUIRE(tokenizer.TryReadNextVariant(device, variant));

    INFO(cmn::Format("   -> Expected Datatype: {}", dataType));
    INFO(cmn::Format("   -> Got      Datatype: {}", variant.GetDataType()));
    REQUIRE(variant.GetDataType() == dataType);

    string variantStr;
    variant.ToString(variantStr);
    INFO(cmn::Format("   -> Convert To String: {}", variantStr));

    REQUIRE(variantStr == expected);
}

void TestStream(const string_view& buffer, const char* tokens[])
{
    PdfMemoryInputDevice device(buffer);
    PdfTokenizer tokenizer;
    string_view token;
    unsigned i = 0;
    while (tokens[i] != nullptr)
    {
        REQUIRE(tokenizer.TryReadNextToken(device, token));
        REQUIRE(token == tokens[i]);

        i++;
    }

    // We are at the end, so GetNextToken has to return false!
    REQUIRE(!tokenizer.TryReadNextToken(device, token));
}

void TestStreamIsNextToken(const string_view& buffer, const char* tokens[])
{
    PdfMemoryInputDevice device(buffer);
    PdfTokenizer tokenizer;

    unsigned i = 0;
    while (tokens[i] != nullptr)
        REQUIRE(tokenizer.IsNextToken(device, tokens[i++]));
}
