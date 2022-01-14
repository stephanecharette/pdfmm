/**
 * Copyright (C) 2021 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Lesser General Public License 2.1.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#ifndef PDF_ENCODING_H
#define PDF_ENCODING_H

#include "PdfEncodingMap.h"
#include "PdfString.h"
#include "PdfObject.h"

namespace mm
{
    class PdfFont;
    class PdfEncoding;

    enum class PdfEncodingExportFlags
    {
        None = 0,
        SkipToUnicode = 1,  ///< Skip exporting a /ToUnicode entry
    };

    /**
     * A PdfEncoding is in PdfFont to transform a text string
     * into a representation so that it can be displayed in a
     * PDF file.
     *
     * PdfEncoding can also be used to convert strings from a
     * PDF file back into a PdfString.
     */
    class PDFMM_API PdfEncoding
    {
        friend class PdfEncodingFactory;
        friend class PdfEncodingShim;

    public:
        /** Null encoding
         */
        PdfEncoding();
        PdfEncoding(const PdfEncodingMapConstPtr& encoding, const PdfEncodingMapConstPtr& toUnicode = nullptr);
        virtual ~PdfEncoding();

    private:
        PdfEncoding(size_t id, const PdfEncodingMapConstPtr& encoding, const PdfEncodingMapConstPtr& toUnicode = nullptr);
        PdfEncoding(const PdfObject& fontObj, const PdfEncodingMapConstPtr& encoding, const PdfEncodingMapConstPtr& toUnicode);

    public:
        /**
         * \remarks Doesn't throw if conversion failed, totally or partially
         */
        std::string ConvertToUtf8(const PdfString& encodedStr) const;

        /**
         * \remarks Produces a partial result also in case of failure
         */
        bool TryConvertToUtf8(const PdfString& encodedStr, std::string& str) const;

        /**
         * \remarks It throws if conversion failed, totally or partially
         */
        charbuff ConvertToEncoded(const std::string_view& str) const;

        bool TryConvertToEncoded(const std::string_view& str, charbuff& encoded) const;

        /**
         * Get a cid codes from a utf8 string
         *
         * \param str utf8 encoded string
         * \remarks Doesn't throw if conversion failed, totally or partially
         */
        std::vector<PdfCID> ConvertToCIDs(const std::string_view& str) const;

        /**
         * Try to get a cid codes from a utf8 string
         *
         * \param str utf8 encoded string
         * \remarks Produces a partial result also in case of failure
         */
        bool TryConvertToCIDs(const std::string_view& str, std::vector<PdfCID>& cids) const;

        /**
         * \remarks Doesn't throw if conversion failed, totally or partially
         */
        std::vector<PdfCID> ConvertToCIDs(const PdfString& encodedStr) const;

        /**
         * \remarks Produces a partial result also in case of failure
         */
        bool TryConvertToCIDs(const PdfString& encodedStr, std::vector<PdfCID>& cids) const;

        /**
         * \remarks Doesn't throw if conversion failed, totally or partially
         */
        PdfCID GetCID(char32_t codePoint) const;

        bool TryGetCID(char32_t codePoint, PdfCID& cid) const;

        /** Get code point from char code unit
         *
         * \returns the found code point or U'\0' if missing or
         *      multiple matched codepoints
         */
        char32_t GetCodePoint(const PdfCharCode& codeUnit) const;

        /** Get code point from char code
         *
         * \returns the found code point or U'\0' if missing or
         *      multiple matched codepoints
         * \remarks it will iterate available code sizes
         */
        char32_t GetCodePoint(unsigned charCode) const;

        void ExportToFont(PdfFont& font, PdfEncodingExportFlags flags = { }) const;

    public:
        /** This return the first char code used in the encoding
         * \remarks Mostly useful for non cid-keyed fonts to export /FirstChar
         */
        const PdfCharCode& GetFirstChar() const;

        /** This return the last char code used in the encoding
         * \remarks Mostly useful for non cid-keyed fonts to export /LastChar
         */
        const PdfCharCode& GetLastChar() const;

        /** Return true if the encoding is a dummy null encoding
         */
        bool IsNull() const;

        /** Return true if the encoding does CID mapping
         */
        bool HasCIDMapping() const;

        /** Returns true if /FirstChar and /LastChar were parsed from object
         */
        bool HasParsedLimits() const;

        /**
         * Return an Id to be used in hashed containers.
         * Id 0 has a special meaning for PdfDynamicEncoding
         *  \see PdfDynamicEncoding
         */
        size_t GetId() const { return m_Id; }

        /** Get actual limits of the encoding
         *
         * May be the limits inferred from /Encoding or the limits inferred by /FirstChar, /LastChar
         */
        const PdfEncodingLimits& GetLimits() const;

        bool HasValidToUnicodeMap() const;

        /** Get the ToUnicode map, throws if missing
         */
        const PdfEncodingMap& GetToUnicodeMap() const;

        /** Get the ToUnicode map, fallback to the normal encoding if missing
         *
         * \param toUnicode the retrieved map
         * \return true if the retrieved map is valid, false otherwise
         */
        bool GetToUnicodeMapSafe(const PdfEncodingMap*& toUnicode) const;

        /** Get the ToUnicode map, fallback to the normal encoding if missing
         *
         * \return the retrieved map
         * \remark As a general rule, we always use this method when converting encoded -> Unicode
         */
        const PdfEncodingMap& GetToUnicodeMapSafe() const;

        inline const PdfEncodingMap& GetEncodingMap() const { return *m_Encoding; }

        inline const PdfEncodingMapConstPtr GetEncodingMapPtr() const { return m_Encoding; }

        const PdfEncodingMapConstPtr GetToUnicodeMapPtr() const;

    public:
        PdfEncoding& operator=(const PdfEncoding&) = default;
        PdfEncoding(const PdfEncoding&) = default;

    protected:
        virtual PdfFont & GetFont() const;

    private:
        bool tryExportObjectTo(PdfDictionary& dictionary, bool wantCidMapping) const;
        bool tryConvertEncodedToUtf8(const std::string_view& encoded, std::string& str) const;
        bool tryConvertEncodedToCIDs(const std::string_view& encoded, std::vector<PdfCID>& cids) const;
        void writeCIDMapping(PdfObject& cmapObj, const PdfFont& font, const std::string_view& baseFont) const;
        void writeToUnicodeCMap(PdfObject& cmapObj) const;
        PdfCharCode getCharCode(PdfFont& font, unsigned gid, const unicodeview& codePoints) const;

    private:
        size_t m_Id;
        PdfEncodingMapConstPtr m_Encoding;
        PdfEncodingMapConstPtr m_ToUnicode;
        PdfEncodingLimits m_Limits;
    };
}

ENABLE_BITMASK_OPERATORS(mm::PdfEncodingExportFlags);

#endif // PDF_ENCODING_H
