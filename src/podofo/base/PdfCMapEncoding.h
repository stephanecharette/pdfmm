/**
 * Copyright (C) 2007 by Dominik Seichter <domseichter@web.de>
 * Copyright (C) 2020 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Library General Public License 2.0 or later.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#include "PdfEncodingMap.h"

namespace PoDoFo
{
    class PdfObject;
    class PdfStream;

    class PdfCMapEncoding : public PdfEncodingMapBase
    {
    public:
        PdfCMapEncoding(const PdfObject& cmapObj);

    public:
        bool HasCIDMapping() const override;
        bool HasLigaturesSupport() const override;

    private:
        struct MapIdentity
        {
            PdfCharCodeMap Map;
            PdfEncodingLimits Limits;
        };
        PdfCMapEncoding(MapIdentity map);
        static MapIdentity parseCMapObject(const PdfStream& stream);
    };
}