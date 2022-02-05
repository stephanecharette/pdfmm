/**
 * Copyright (C) 2007 by Dominik Seichter <domseichter@web.de>
 * Copyright (C) 2020 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Library General Public License 2.0 or later.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#include <pdfmm/private/PdfDeclarationsPrivate.h>
#include "PdfEncodingMap.h"

#include <utfcpp/utf8.h>

#include "PdfDictionary.h"
#include "PdfCMapEncoding.h"
#include "PdfFont.h"

using namespace std;
using namespace mm;

PdfEncodingMap::PdfEncodingMap() { }

PdfEncodingMap::~PdfEncodingMap() { }

bool PdfEncodingMap::TryGetExportObject(PdfIndirectObjectList& objects, PdfName& name, PdfObject*& obj) const
{
    name = { };
    obj = nullptr;
    getExportObject(objects, name, obj);
    return !(obj == nullptr && name.IsNull());
}

void PdfEncodingMap::getExportObject(PdfIndirectObjectList& objects, PdfName& name, PdfObject*& obj) const
{
    (void)objects;
    (void)name;
    (void)obj;
}

bool PdfEncodingMap::TryGetNextCharCode(string_view::iterator& it, const string_view::iterator& end, PdfCharCode& code) const
{
    if (it == end)
    {
        code = { };
        return false;
    }

    if (HasLigaturesSupport())
    {
        auto temp = it;
        if (!tryGetNextCharCode(temp, end, code))
        {
            code = { };
            return false;
        }

        it = temp;
        return true;
    }
    else
    {
        char32_t cp = (char32_t)utf8::next(it, end);
        return tryGetCharCode(cp, code);
    }
}

bool PdfEncodingMap::tryGetNextCharCode(string_view::iterator& it, const string_view::iterator& end, PdfCharCode& codeUnit) const
{
    (void)it;
    (void)end;
    (void)codeUnit;
    PDFMM_RAISE_ERROR(PdfErrorCode::NotImplemented);
}

bool PdfEncodingMap::tryGetCharCodeSpan(const unicodeview& ligature, PdfCharCode& codeUnit) const
{
    (void)ligature;
    (void)codeUnit;
    PDFMM_RAISE_ERROR(PdfErrorCode::NotImplemented);
}

bool PdfEncodingMap::TryGetCharCode(char32_t codePoint, PdfCharCode& codeUnit) const
{
    return tryGetCharCode(codePoint, codeUnit);
}

bool PdfEncodingMap::TryGetCharCode(const unicodeview& codePoints, PdfCharCode& codeUnit) const
{
    if (codePoints.size() == 1)
    {
        return tryGetCharCode(codePoints[0], codeUnit);
    }
    else if (codePoints.size() == 0 || !HasLigaturesSupport())
    {
        codeUnit = { };
        return false;
    }

    // Try to lookup the ligature
    PDFMM_INVARIANT(codePoints.size() > 1);
    return tryGetCharCodeSpan(codePoints, codeUnit);
}

bool PdfEncodingMap::TryGetCharCode(unsigned cid, PdfCharCode& codeUnit) const
{
    // NOTE: getting the char code from a cid on this map is the same operation
    // as getting it from an unicode code point
    return tryGetCharCode((char32_t)cid, codeUnit);
}

bool PdfEncodingMap::TryGetNextCID(string_view::iterator& it,
    const string_view::iterator& end, PdfCID& cid) const
{
    if (GetType() == PdfEncodingMapType::CMap)
    {
        vector<char32_t> codePoints;
        bool success = tryGetNextCodePoints(it, end, cid.Unit, codePoints);
        if (!success || codePoints.size() != 1)
        {
            // Return false on missing lookup or malformed multiple code points found
            cid = { };
            return false;
        }

        // char32_t is also unsigned
        cid.Id = (unsigned)codePoints[0];
        return true;
    }
    else
    {
        // If there's no CID mapping, we just iterate character codes

        // Save current iterator in the case the search is unsuccessful
        string_view::iterator curr = it;

        unsigned code = 0;
        unsigned char i = 1;
        auto& limits = GetLimits();
        PDFMM_ASSERT(i <= limits.MaxCodeSize);
        while (curr != end)
        {
            // Iterate the string and accumulate a
            // code of the appropriate code size
            code <<= 8;
            code |= (unsigned char)*curr;
            curr++;
            if (i == limits.MaxCodeSize)
            {
                cid.Unit = { code, limits.MaxCodeSize };
                cid.Id = code; // We assume identity with CharCode
                it = curr;
                return true;
            }
            i++;
        }

        cid = { };
        return false;
    }
}

bool PdfEncodingMap::TryGetNextCodePoints(string_view::iterator& it,
    const string_view::iterator& end, vector<char32_t>& codePoints) const
{
    codePoints.clear();
    PdfCharCode code;
    return tryGetNextCodePoints(it, end, code, codePoints);
}

bool PdfEncodingMap::TryGetCIDId(const PdfCharCode& codeUnit, unsigned& cid) const
{
    // NOTE: Here we assume the map will actually
    // contains cids, and not unicode codepoints
    vector<char32_t> cids;
    bool success = tryGetCodePoints(codeUnit, cids);
    if (!success || cids.size() != 1)
    {
        // Return false on missing lookup or malformed multiple code points found
        return false;
    }

    cid = (unsigned)cids[0];
    return true;
}

bool PdfEncodingMap::TryGetCodePoints(const PdfCharCode& codeUnit, vector<char32_t>& codePoints) const
{
    codePoints.clear();
    return tryGetCodePoints(codeUnit, codePoints);
}

bool PdfEncodingMap::HasLigaturesSupport() const
{
    return false;
}

// NOTE: Don't clear the result on failure. It is done externally
bool PdfEncodingMap::tryGetNextCodePoints(string_view::iterator& it, const string_view::iterator& end,
    PdfCharCode& codeUnit, vector<char32_t>& codePoints) const
{
    // Save current iterator in the case the search is unsuccessful
    string_view::iterator curr = it;
    unsigned code = 0;
    unsigned char i = 1;
    auto& limits = GetLimits();
    while (curr != end)
    {
        if (i > limits.MaxCodeSize)
            return false;

        // CMap Mapping, PDF Reference 1.7, pg. 453
        // A sequence of one or more bytes is extracted from the string and matched against
        // the codespace ranges in the CMap. That is, the first byte is matched against 1-byte
        // codespace ranges; if no match is found, a second byte is extracted, and the 2-byte
        // srcCode is matched against 2-byte codespace ranges. This process continues for successively
        // longer codes until a match is found or all codespace ranges have been
        // tested. There will be at most one match because codespace ranges do not overlap.

        code <<= 8;
        code |= (uint8_t)*curr;
        curr++;
        codeUnit = { code, i };
        if (i < limits.MinCodeSize || !tryGetCodePoints(codeUnit, codePoints))
        {
            i++;
            continue;
        }

        it = curr;
        return true;
    }

    return false;
}

PdfEncodingMapBase::PdfEncodingMapBase(PdfCharCodeMap&& map)
    : m_charMap(std::make_shared<PdfCharCodeMap>(std::move(map)))
{
}

void PdfEncodingMapBase::AppendCIDMappingEntries(PdfObjectStream& stream, const PdfFont& font) const
{
    (void)font;
    stream.Append(std::to_string(m_charMap->GetSize())).Append(" begincidchar\n");
    string code;
    for (auto& pair : *m_charMap)
    {
        auto& unit = pair.first;
        unsigned cid = pair.second[0]; // We assume the cid to be in the single element
        unit.WriteHexTo(code);
        stream.Append(code).Append(" ").Append(std::to_string(cid)).Append("\n");;
    }
    stream.Append("endcidchar\n");
}

void PdfEncodingMapBase::AppendCodeSpaceRange(PdfObjectStream& stream) const
{
    // CHECK-ME: The limit inferr may be not needed anymore
    struct Limit
    {
        PdfCharCode FirstCode;
        PdfCharCode LastCode;
    };

    unordered_map<unsigned char, Limit> ranges;
    for (auto& pair : *m_charMap)
    {
        auto& codeUnit = pair.first;
        auto found = ranges.find(codeUnit.CodeSpaceSize);
        if (found == ranges.end())
        {
            ranges[codeUnit.CodeSpaceSize] = Limit{ codeUnit, codeUnit };
        }
        else
        {
            auto& limit = found->second;
            if (codeUnit.Code < limit.FirstCode.Code)
                limit.FirstCode = codeUnit;

            if (codeUnit.Code > limit.LastCode.Code)
                limit.LastCode = codeUnit;
        }
    }

    bool first = true;
    for (auto& pair : ranges)
    {
        if (first)
            first = false;
        else
            stream.Append("\n");

        auto& range = pair.second;
        string temp;
        range.FirstCode.WriteHexTo(temp);
        stream.Append(temp);
        range.LastCode.WriteHexTo(temp);
        stream.Append(temp);
    }
}

PdfEncodingMapType PdfEncodingMapBase::GetType() const
{
    return PdfEncodingMapType::CMap;
}

const PdfEncodingLimits& PdfEncodingMapBase::GetLimits() const
{
    return m_charMap->GetLimits();
}

PdfEncodingMapBase::PdfEncodingMapBase(const shared_ptr<PdfCharCodeMap>& map)
    : m_charMap(map)
{
    if (map == nullptr)
        PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InvalidHandle, "Map must be not null");
}

bool PdfEncodingMapBase::tryGetNextCharCode(string_view::iterator& it, const string_view::iterator& end,
    PdfCharCode& codeUnit) const
{
    return m_charMap->TryGetNextCharCode(it, end, codeUnit);
}

bool PdfEncodingMapBase::tryGetCharCodeSpan(const unicodeview& codePoints, PdfCharCode& codeUnit) const
{
    return m_charMap->TryGetCharCode(codePoints, codeUnit);
}

bool PdfEncodingMapBase::tryGetCharCode(char32_t codePoint, PdfCharCode& codeUnit) const
{
    return m_charMap->TryGetCharCode(codePoint, codeUnit);
}

bool PdfEncodingMapBase::tryGetCodePoints(const PdfCharCode& code, vector<char32_t>& codePoints) const
{
    return m_charMap->TryGetCodePoints(code, codePoints);
}

void PdfEncodingMapBase::AppendToUnicodeEntries(PdfObjectStream& stream) const
{
    // Very easy, just do a list of bfchar
    // Use PdfEncodingMap::AppendUTF16CodeTo
    u16string u16temp;
    string temp = std::to_string(m_charMap->GetSize());
    stream.Append(temp).Append(" beginbfchar\n");
    for (auto& pair : *m_charMap)
    {
        pair.first.WriteHexTo(temp);
        stream.Append(temp);
        stream.Append(" ");
        PdfEncodingMap::AppendUTF16CodeTo(stream, pair.second, u16temp);
        stream.Append("\n");
    }
    stream.Append("endbfchar");
}

void PdfEncodingMap::AppendUTF16CodeTo(PdfObjectStream& stream, const unicodeview& codePoints, u16string& u16tmp)
{
    char hexbuf[2];
    stream.Append("<");
    bool first = true;
    for (unsigned i = 0; i < codePoints.size(); i++)
    {
        if (first)
            first = false;
        else
            stream.Append(" "); // Separate each character in the ligatures

        char32_t cp = codePoints[i];
        utls::WriteToUtf16BE(u16tmp, cp);

        auto data = (const char*)u16tmp.data();
        size_t size = u16tmp.size() * sizeof(char16_t);
        for (unsigned l = 0; l < size; l++)
        {
            // Append hex codes of the converted utf16 string
            utls::WriteCharHexTo(hexbuf, data[l]);
            stream.Append(hexbuf, std::size(hexbuf));
        }
    }
    stream.Append(">");
}

void PdfEncodingMap::AppendCodeSpaceRange(PdfObjectStream& stream) const
{
    string temp;
    auto& limits = GetLimits();
    limits.FirstChar.WriteHexTo(temp);
    stream.Append(temp);
    limits.LastChar.WriteHexTo(temp);
    stream.Append(temp);
}

PdfEncodingMapOneByte::PdfEncodingMapOneByte(const PdfEncodingLimits& limits)
    : m_Limits(limits) { }

void PdfEncodingMapOneByte::AppendToUnicodeEntries(PdfObjectStream& stream) const
{
    auto& limits = GetLimits();
    PDFMM_ASSERT(limits.MaxCodeSize == 1);
    vector<char32_t> codePoints;
    unsigned code = limits.FirstChar.Code;
    unsigned lastCode = limits.LastChar.Code;
    string codeStr;
    stream.Append("1 beginbfrange\n");
    limits.FirstChar.WriteHexTo(codeStr);
    stream.Append(codeStr).Append(" ");
    limits.LastChar.WriteHexTo(codeStr);
    stream.Append(codeStr).Append(" [\n");
    u16string u16tmp;
    for (; code < lastCode; code++)
    {
        if (!TryGetCodePoints(PdfCharCode(code), codePoints))
            PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InvalidFontFile, "Unable to find character code");

        AppendUTF16CodeTo(stream, codePoints, u16tmp);
        stream.Append("\n");
    }
    stream.Append("]\n");
    stream.Append("endbfrange");
}

void PdfEncodingMapOneByte::AppendCIDMappingEntries(PdfObjectStream& stream, const PdfFont& font) const
{
    auto& limits = GetLimits();
    PDFMM_ASSERT(limits.MaxCodeSize == 1);
    unsigned code = limits.FirstChar.Code;
    unsigned lastCode = limits.LastChar.Code;
    vector<char32_t> codePoints;
    unsigned gid;
    struct Mapping
    {
        PdfCharCode Code;
        unsigned CID;
    };

    vector<Mapping> mappings;
    for (; code < lastCode; code++)
    {
        PdfCharCode charCode(code);
        if (!TryGetCodePoints(charCode, codePoints))
            PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InvalidFontFile, "Unable to find character code");

        if (!font.TryGetGID(codePoints[0], gid))
            continue;

        // NOTE: We will map the char code directly to the gid, so
        // we assume cid == gid identity
        mappings.push_back({ charCode, gid });
    }

    stream.Append(std::to_string(mappings.size())).Append(" begincidchar\n");
    string temp;
    for (auto& mapping : mappings)
    {
        mapping.Code.WriteHexTo(temp);
        stream.Append(temp).Append(" ").Append(std::to_string(mapping.CID)).Append("\n");;
    }
    stream.Append("endcidchar\n");
}

const PdfEncodingLimits& PdfEncodingMapOneByte::GetLimits() const
{
    return m_Limits;
}

PdfEncodingMapType PdfEncodingMapOneByte::GetType() const
{
    return PdfEncodingMapType::Simple;
}

PdfNullEncodingMap::PdfNullEncodingMap() { }

PdfEncodingMapType PdfNullEncodingMap::GetType() const
{
    // NOTE: We assume PdfNullEncodingMap will used in the
    // null PdfEncoding that replaced with PdfDynamicEncoding
    // in PdfFont. See PdfFont implementation
    return PdfEncodingMapType::CMap;
}

const PdfEncodingLimits& PdfNullEncodingMap::GetLimits() const
{
    PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InternalLogic, "The null encoding must be bound to a PdfFont");
}

bool PdfNullEncodingMap::tryGetCharCode(char32_t codePoint, PdfCharCode& codeUnit) const
{
    (void)codePoint;
    (void)codeUnit;
    PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InternalLogic, "The null encoding must be bound to a PdfFont");
}

bool PdfNullEncodingMap::tryGetCodePoints(const PdfCharCode& codeUnit, vector<char32_t>& codePoints) const
{
    (void)codeUnit;
    (void)codePoints;
    PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InternalLogic, "The null encoding must be bound to a PdfFont");
}

void PdfNullEncodingMap::AppendToUnicodeEntries(PdfObjectStream& stream) const
{
    (void)stream;
    PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InternalLogic, "The null encoding must be bound to a PdfFont");
}

void PdfNullEncodingMap::AppendCIDMappingEntries(PdfObjectStream& stream, const PdfFont& font) const
{
    (void)stream;
    (void)font;
    PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InternalLogic, "The null encoding must be bound to a PdfFont");
}

PdfBuiltInEncoding::PdfBuiltInEncoding(const PdfName& name)
    : PdfEncodingMapOneByte({ 1, 1, PdfCharCode(0), PdfCharCode(0xFF) }), m_Name(name)
{
}

void PdfBuiltInEncoding::InitEncodingTable()
{
    if (!m_EncodingTable.empty())
        return;

    const char32_t* cpUnicodeTable = this->GetToUnicodeTable();
    for (size_t i = 0; i < 256; i++)
    {
        // fill the table with data
        m_EncodingTable[cpUnicodeTable[i]] =
            static_cast<unsigned char>(i);
    }
}

bool PdfBuiltInEncoding::tryGetCharCode(char32_t codePoint, PdfCharCode& codeUnit) const
{
    const_cast<PdfBuiltInEncoding*>(this)->InitEncodingTable();
    auto found = m_EncodingTable.find(codePoint);
    if (found == m_EncodingTable.end())
    {
        codeUnit = { };
        return false;
    }

    codeUnit = { (unsigned char)found->second, 1 };
    return true;
}

bool PdfBuiltInEncoding::tryGetCodePoints(const PdfCharCode& codeUnit, vector<char32_t>& codePoints) const
{
    if (codeUnit.Code >= 256)
        return false;

    const char32_t* cpUnicodeTable = this->GetToUnicodeTable();
    codePoints.push_back(cpUnicodeTable[codeUnit.Code]);
    return true;
}
