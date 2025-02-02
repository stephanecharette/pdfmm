/**
 * Copyright (C) 2006 by Dominik Seichter <domseichter@web.de>
 * Copyright (C) 2020 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Library General Public License 2.0 or later.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#ifndef PDF_DEVICE_BASE_H
#define PDF_DEVICE_BASE_H

#include "PdfDeclarations.h"

namespace mm {

enum class DeviceAccess
{
    Read = 1,
    Write = 2,
    ReadWrite = Read | Write
};

enum class SeekDirection
{
    Begin = 0,
    Current,
    End
};

class PDFMM_API StreamDeviceBase
{
protected:
    StreamDeviceBase();

public:
    /** Seek the device to the position offset from the beginning
     *  \param offset from the beginning of the file
     */
    void Seek(size_t offset);

    /** Seek the device to the position offset from the beginning
     *  \param offset from the beginning of the file
     *  \param direction where to start (Begin, Current, End)
     *
     *  A non-seekable input device will throw an InvalidDeviceOperation.
     */
    void Seek(ssize_t offset, SeekDirection direction);

    void Close();

public:
    DeviceAccess GetAccess() const { return m_Access; }

    /**
     * \return True if the stream is at EOF
     */
    virtual bool Eof() const = 0;

    /** The number of bytes written to this object.
     *  \returns the number of bytes written to this object.
     *
     *  \see Init
     */
    virtual size_t GetLength() const = 0;

    /** Get the current offset from the beginning of the file.
     *  \return the offset form the beginning of the file.
     */
    virtual size_t GetPosition() const = 0;

    virtual bool CanSeek() const;

protected:
    void EnsureAccess(DeviceAccess access) const;
    void SetAccess(DeviceAccess access) { m_Access = access; }

    virtual void seek(ssize_t offset, SeekDirection direction);
    virtual void close();

private:
    DeviceAccess m_Access;
};

}

ENABLE_BITMASK_OPERATORS(mm::DeviceAccess);

#endif // PDF_DEVICE_BASE_H
