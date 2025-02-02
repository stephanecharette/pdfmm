/**
 * Copyright (C) 2005 by Dominik Seichter <domseichter@web.de>
 * Copyright (C) 2020 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Library General Public License 2.0 or later.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#ifndef PDF_OBJECT_STREAM_H
#define PDF_OBJECT_STREAM_H

#include "PdfDeclarations.h"

#include "PdfFilter.h"
#include "PdfEncrypt.h"
#include "PdfOutputStream.h"
#include "PdfInputStream.h"

namespace mm {

class InputStream;
class PdfName;
class PdfObject;
class PdfObjectStream;

class PdfObjectInputStream : public InputStream
{
    friend class PdfObjectStream;
public:
    PdfObjectInputStream();
    ~PdfObjectInputStream();
    PdfObjectInputStream(PdfObjectInputStream&& rhs) noexcept;
private:
    PdfObjectInputStream(PdfObjectStream& stream, bool raw);
public:
    const PdfFilterList& GetMediaFilters() const { return m_MediaFilters; }
protected:
    size_t readBuffer(char* buffer, size_t size, bool& eof) override;
    bool readChar(char& ch) override;
public:
    PdfObjectInputStream& operator=(PdfObjectInputStream&& rhs) noexcept;
private:
    PdfObjectStream* m_stream;
    std::unique_ptr<InputStream> m_input;
    PdfFilterList m_MediaFilters;
};

class PdfObjectOutputStream : public OutputStream
{
    friend class PdfObjectStream;
public:
    PdfObjectOutputStream();
    ~PdfObjectOutputStream();
    PdfObjectOutputStream(PdfObjectOutputStream&& rhs) noexcept;
private:
    PdfObjectOutputStream(PdfObjectStream& stream, PdfFilterList&& filters,
        bool append);
    PdfObjectOutputStream(PdfObjectStream& stream);
private:
    PdfObjectOutputStream(PdfObjectStream& stream, PdfFilterList&& filters,
        bool append, bool preserveFilter);
protected:
    void writeBuffer(const char* buffer, size_t size) override;
    void flush() override;
public:
    PdfObjectOutputStream& operator=(PdfObjectOutputStream&& rhs) noexcept;
private:
    PdfObjectStream* m_stream;
    PdfFilterList m_filters;
    std::unique_ptr<OutputStream> m_output;
};

/** A PDF stream can be appended to any PdfObject
 *  and can contain arbitrary data.
 *
 *  Most of the time it will contain either drawing commands
 *  to draw onto a page or binary data like a font or an image.
 *
 *  You have to use a concrete implementation of a stream,
 *  which can be retrieved from a StreamFactory.
 *  \see PdfIndirectObjectList
 *  \see PdfMemoryObjectStream
 *  \see PdfFileObjectStream
 */
class PDFMM_API PdfObjectStream
{
    friend class PdfObject;
    friend class PdfParserObject;
    friend class PdfObjectInputStream;
    friend class PdfObjectOutputStream;

protected:
    /** Create a new PdfObjectStream object which has a parent PdfObject.
     *  The stream will be deleted along with the parent.
     *  This constructor will be called by PdfObject::Stream() for you.
     *  \param parent parent object
     */
    PdfObjectStream(PdfObject& parent);

public:
    virtual ~PdfObjectStream();

    PdfObjectOutputStream GetOutputStreamRaw(bool append = false);

    PdfObjectOutputStream GetOutputStream(bool append = false);

    PdfObjectOutputStream GetOutputStream(const PdfFilterList& filters, bool append = false);

    PdfObjectInputStream GetInputStream(bool raw = false) const;

    /** Set the data contents copying from a buffer
     *  All data will be Flate-encoded.
     *
     *  \param buffer buffer containing the stream data
     */
    void SetData(const bufferview& buffer, bool raw = false);

    /** Set the data contents copying from a buffer
     *
     * Use PdfFilterFactory::CreateFilterList() if you want to use the contents
     * of the stream dictionary's existing filter key.
     *
     *  \param buffer buffer containing the stream data
     *  \param filters a list of filters to use when appending data
     */
    void SetData(const bufferview& buffer, const PdfFilterList& filters);

    /** Set the data contents reading from an InputStream
     *  All data will be Flate-encoded.
     *
     *  \param stream read stream contents from this InputStream
     */
    void SetData(InputStream& stream, bool raw = false);

    /** Set the data contents reading from an InputStream
     *
     * Use PdfFilterFactory::CreateFilterList() if you want to use the contents
     * of the stream dictionary's existing filter key.
     *
     *  \param stream read stream contents from this InputStream
     *  \param filters a list of filters to use when appending data
     */
    void SetData(InputStream& stream, const PdfFilterList& filters);

    /** Get an unwrapped copy of the stream, unpacking non media filters
     * \remarks throws if the stream contains media filters, like DCTDecode
     */
    charbuff GetCopy(bool raw = false) const;

    /** Get an unwrapped copy of the stream, unpacking non media filters
     */
    charbuff GetCopySafe() const;

    /** Unwrap the stream to the given buffer, unpacking non media filters
     * \remarks throws if the stream contains media filters, like DCTDecode
     */
    void CopyTo(charbuff& buffer, bool raw = false) const;

    /** Unwrap the stream to the given buffer, unpacking non media filters
     */
    void CopyToSafe(charbuff& buffer) const;

    /** Unwrap the stream and write it to the given stream, unpacking non media filters
     * \remarks throws if the stream contains media filters, like DCTDecode
     */
    void CopyTo(OutputStream& stream, bool raw = false) const;

    /** Unwrap the stream and write it to the given stream, unpacking non media filters
     */
    void CopyToSafe(OutputStream& stream) const;

    void MoveTo(PdfObject& obj);

    /** Get the stream's length with all filters applied (e.g. if the stream is
     * Flate-compressed, the length of the compressed data stream).
     *
     *  \returns the length of the internal buffer
     */
    virtual size_t GetLength() const = 0;

    /** Create a copy of a PdfObjectStream object
     *  \param rhs the object to clone
     *  \returns a reference to this object
     */
    PdfObjectStream& operator=(const PdfObjectStream& rhs);

protected:
    /** Write the stream to an output device
     *  \param device write to this outputdevice.
     *  \param encrypt encrypt stream data using this object
     */
    virtual void Write(OutputStream& stream, const PdfStatefulEncrypt& encrypt) = 0;

    virtual void CopyFrom(const PdfObjectStream& rhs);

    virtual std::unique_ptr<InputStream> getInputStream() = 0;

    virtual std::unique_ptr<OutputStream> getOutputStream() = 0;

    void EnsureClosed() const;

    PdfObject& GetParent() { return *m_Parent; }

private:
    void InitData(InputStream& stream, size_t len);

private:
    std::unique_ptr<InputStream> getInputStream(bool raw, PdfFilterList& mediaFilters);

    void setData(InputStream& stream, PdfFilterList filters, ssize_t size, bool markObjectDirty);

private:
    PdfObjectStream(const PdfObjectStream& rhs) = delete;

private:
    PdfObject* m_Parent;
    bool m_locked;
};

};

#endif // PDF_OBJECT_STREAM_H
