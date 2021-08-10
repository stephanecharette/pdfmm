/**
 * Copyright (C) 2009 by Dominik Seichter <domseichter@web.de>
 *
 * Licensed under GNU Library General Public License 2.0 or later.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#ifndef PDF_MEMORY_MANAGEMENT_H
#define PDF_MEMORY_MANAGEMENT_H

// PdfMemoryManagement.h should not include PdfDefines.h, since it is included by it.
// It should avoid depending on anything defined in PdfDefines.h .

#include "pdfmmdefs.h"

namespace mm {

/**
* Wrapper around calloc of the c-library used by pdfmm.
*
* Is used to allocate buffers inside of pdfmm, guarding against count*size size_t overflow.
*/
PDFMM_API void* pdfmm_calloc(size_t count, size_t size);

/**
 * Wrapper around realloc of the c-library used by pdfmm.
 */
PDFMM_API void* pdfmm_realloc(void* buffer, size_t size);

/**
 * Wrapper around free of the c-library used by pdfmm.
 *
 * Use this to free memory allocated inside of pdfmm
 * with pdfmm_malloc.
 */
PDFMM_API void pdfmm_free(void* buffer);


};

#endif // PDF_MEMORY_MANAGEMENT_H