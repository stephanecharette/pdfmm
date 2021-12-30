/**
 * Copyright (C) 2008 by Dominik Seichter <domseichter@web.de>
 * Copyright (C) 2021 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Library General Public 2.0 or later.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#include <PdfTest.h>

using namespace std;
using namespace mm;

static void TestObjectsDirty(
    const PdfObject& objBool,
    const PdfObject& objNum,
    const PdfObject& objReal,
    const PdfObject& objStr,
    const PdfObject& objRef,
    const PdfObject& objArray,
    const PdfObject& objDict,
    const PdfObject& objStream,
    const PdfObject& objVariant,
    bool testValue);

TEST_CASE("testEmptyObject")
{
    PdfMemDocument doc;
    auto device = std::make_shared<PdfMemoryInputDevice>("10 0 obj\nendobj\n"sv);
    PdfParserObject parserObj(doc, *device);
    parserObj.SetLoadOnDemand(false);
    parserObj.ParseFile(nullptr);
    REQUIRE(parserObj.IsNull());
}

TEST_CASE("testEmptyStream")
{
    PdfMemDocument doc;
    auto device = std::make_shared<PdfMemoryInputDevice>("10 0 obj<</Length 0>>stream\nendstream\nendobj\n"sv);
    PdfParserObject parserObj(doc, *device);
    parserObj.SetLoadOnDemand(false);
    parserObj.ParseFile(nullptr);
    REQUIRE(parserObj.IsDictionary());
    REQUIRE(parserObj.HasStream());
    REQUIRE(parserObj.GetStream()->GetLength() == 0);
}

TEST_CASE("testNameObject")
{
    PdfMemDocument doc;
    auto device = std::make_shared<PdfMemoryInputDevice>("10 0 obj / endobj\n"sv);
    PdfParserObject parserObj(doc, *device);
    parserObj.SetLoadOnDemand(false);
    parserObj.ParseFile(nullptr);
    REQUIRE(parserObj.IsName());
    REQUIRE(parserObj.GetName().GetString() == "");
}

TEST_CASE("testIsDirtyTrue")
{
    PdfMemDocument doc;

    auto objBool = doc.GetObjects().CreateObject(true);
    auto objNum = doc.GetObjects().CreateObject(static_cast<int64_t>(1));
    auto objReal = doc.GetObjects().CreateObject(1.0);
    auto objStr = doc.GetObjects().CreateObject(PdfString("Any"));
    auto objName = doc.GetObjects().CreateObject(PdfName("Name"));
    auto objRef = doc.GetObjects().CreateObject(PdfReference(0, 0));
    auto objArray = doc.GetObjects().CreateArrayObject();
    auto objDict = doc.GetObjects().CreateDictionaryObject();
    auto objStream = doc.GetObjects().CreateDictionaryObject();
    objStream->GetOrCreateStream().Set("Test"sv);
    auto objVariant = doc.GetObjects().CreateObject(PdfVariant(false));

    // IsDirty should be false after construction
    TestObjectsDirty(*objBool, *objNum, *objReal, *objStr, *objRef, *objArray, *objDict, *objStream, *objVariant, false);

    (void)objBool->GetBool();
    (void)objNum->GetNumber();
    (void)objReal->GetReal();
    (void)objStr->GetString();
    (void)objName->GetName();
    (void)objRef->GetReference();
    (void)objArray->GetArray();
    (void)objDict->GetDictionary();
    (void)objVariant->GetBool();

    // IsDirty should be false after calling getter
    TestObjectsDirty(*objBool, *objNum, *objReal, *objStr, *objRef, *objArray, *objDict, *objStream, *objVariant, false);

    objBool->SetBool(false);
    objNum->SetNumber(static_cast<int64_t>(2));
    objReal->SetReal(2.0);
    objStr->SetString("Other");
    objName->SetName("Name2");
    objRef->SetReference(PdfReference(2, 0));
    objArray->GetArray().Add(*objBool);
    objDict->GetDictionary().AddKey(objName->GetName(), *objStr);
    objStream->MustGetStream().Set("Test1"sv);
    *objVariant = *objNum;

    // IsDirty should be false after calling setter
    TestObjectsDirty(*objBool, *objNum, *objReal, *objStr, *objRef, *objArray, *objDict, *objStream, *objVariant, true);

    auto device = std::make_shared<PdfMemoryInputDevice>("Test"sv);
    PdfParserObject parserObj(doc, *device);
    parserObj.SetLoadOnDemand(false);
    parserObj.ParseFile(nullptr);
    auto stream = parserObj.GetStream();

    // IsDirty should be false after reading an object stream
    REQUIRE(stream->GetLength() == 4);
    INFO("STREAM    IsDirty() == true"); REQUIRE(!parserObj.IsDirty());

    stream->Set("Test1"sv);

    // IsDirty should be false after writing an object stream
    REQUIRE(stream->GetLength() == 5);
    INFO("STREAM    IsDirty() == true"); REQUIRE(parserObj.IsDirty());
}

TEST_CASE("testIsDirtyFalse")
{
    PdfObject objBool(true);
    PdfObject objNum(static_cast<int64_t>(1));
    PdfObject objReal(1.0);
    PdfObject objStr(PdfString("Any"));
    PdfObject objName(PdfName("Name"));
    PdfObject objRef(PdfReference(0, 0));
    PdfObject objArray = PdfObject(PdfArray());
    PdfObject objDict = PdfObject(PdfDictionary());
    PdfObject objStream = PdfObject(PdfDictionary());
    objStream.GetOrCreateStream().Set("Test"sv);
    PdfObject objVariant(objBool);
    PdfObject objEmpty;
    PdfObject objData(PdfData("/Name"));

    // IsDirty should be false after construction
    TestObjectsDirty(objBool, objNum, objReal, objStr, objRef, objArray, objDict, objStream, objVariant, false);
    INFO("EMPTY     IsDirty() == false"); REQUIRE(!objEmpty.IsDirty());
    INFO("DATA      IsDirty() == false"); REQUIRE(!objData.IsDirty());

    (void)objBool.GetBool();
    (void)objNum.GetNumber();
    (void)objReal.GetReal();
    (void)objStr.GetString();
    (void)objName.GetName();
    (void)objRef.GetReference();
    (void)objArray.GetArray();
    (void)objDict.GetDictionary();
    (void)objVariant.GetBool();

    // IsDirty should be false after calling getter
    TestObjectsDirty(objBool, objNum, objReal, objStr, objRef, objArray, objDict, objStream, objVariant, false);

    objBool.SetBool(false);
    objNum.SetNumber(static_cast<int64_t>(2));
    objReal.SetReal(2.0);
    objStr.SetString("Other");
    objName.SetName("Name2");
    objRef.SetReference(PdfReference(2, 0));
    objArray.GetArray().Add(objBool);
    objDict.GetDictionary().AddKey(objName.GetName(), objStr);
    objStream.MustGetStream().Set("Test1"sv);
    objVariant = objNum;

    // IsDirty should be false after calling setter
    TestObjectsDirty(objBool, objNum, objReal, objStr, objRef, objArray, objDict, objStream, objVariant, false);
}

void TestObjectsDirty(
    const PdfObject& objBool,
    const PdfObject& objNum,
    const PdfObject& objReal,
    const PdfObject& objStr,
    const PdfObject& objRef,
    const PdfObject& objArray,
    const PdfObject& objDict,
    const PdfObject& objStream,
    const PdfObject& objVariant,
    bool testValue)
{
    INFO(cmn::Format("BOOL      IsDirty() == {}", testValue)); REQUIRE(objBool.IsDirty() == testValue);
    INFO(cmn::Format("LONG      IsDirty() == {}", testValue)); REQUIRE(objNum.IsDirty() == testValue);
    INFO(cmn::Format("DOUBLE    IsDirty() == {}", testValue)); REQUIRE(objReal.IsDirty() == testValue);
    INFO(cmn::Format("STRING    IsDirty() == {}", testValue)); REQUIRE(objStr.IsDirty() == testValue);
    INFO(cmn::Format("REFERENCE IsDirty() == {}", testValue)); REQUIRE(objRef.IsDirty() == testValue);
    INFO(cmn::Format("ARRAY     IsDirty() == {}", testValue)); REQUIRE(objArray.IsDirty() == testValue);
    INFO(cmn::Format("DICT      IsDirty() == {}", testValue)); REQUIRE(objDict.IsDirty() == testValue);
    INFO(cmn::Format("STREAM    IsDirty() == {}", testValue)); REQUIRE(objStream.IsDirty() == testValue);
    INFO(cmn::Format("VARIANT   IsDirty() == {}", testValue)); REQUIRE(objVariant.IsDirty() == testValue);
}
