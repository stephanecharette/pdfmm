/**
 * Copyright (C) 2005 by Dominik Seichter <domseichter@web.de>
 * Copyright (C) 2020 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Library General Public License 2.0 or later.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#include <podofo/private/PdfDefinesPrivate.h>
#include "PdfPainter.h"

#include <iostream>
#include <iomanip>

#include <utfcpp/utf8.h>

#include "PdfColor.h"
#include "PdfDictionary.h"
#include "PdfFilter.h"
#include "PdfName.h"
#include "PdfRect.h"
#include "PdfStream.h"
#include "PdfString.h"
#include "PdfLocale.h"
#include "PdfContents.h"
#include "PdfExtGState.h"
#include "PdfFont.h"
#include "PdfFontMetrics.h"
#include "PdfImage.h"
#include "PdfMemDocument.h"
#include "PdfXObject.h"

using namespace std;
using namespace PoDoFo;

constexpr unsigned BEZIER_POINTS = 13;

// 4/3 * (1-cos 45<-,A0<-(B)/sin 45<-,A0<-(B = 4/3 * sqrt(2) - 1
constexpr double ARC_MAGIC = 0.552284749;

constexpr unsigned clPainterHighPrecision = 15;
constexpr unsigned clPainterDefaultPrecision = 3;

string expandTabs(const string_view& str, unsigned tabWidth, unsigned nTabCnt);

inline bool IsNewLineChar(char32_t ch)
{
    return ch == U'\n' || ch == U'\r';
}

inline bool IsSpaceChar(char32_t ch)
{
    return isspace(ch) != 0;
}

PdfPainter::PdfPainter(EPdfPainterFlags flags) :
    m_flags(flags),
    m_stream(nullptr),
    m_canvas(nullptr),
    m_Font(nullptr),
    m_TabWidth(4),
    m_curColor(PdfColor(0.0, 0.0, 0.0)),
    m_isTextOpen(false),
    m_isCurColorICCDepend(false),
    m_currentTextRenderingMode(PdfTextRenderingMode::Fill),
    m_lpx(0),
    m_lpy(0),
    m_lpx2(0),
    m_lpy2(0),
    m_lpx3(0),
    m_lpy3(0),
    m_lcx(0),
    m_lcy(0),
    m_lrx(0),
    m_lry(0)
{
    m_tmpStream.flags(ios_base::fixed);
    m_tmpStream.precision(clPainterDefaultPrecision);
    PdfLocaleImbue(m_tmpStream);

    m_curPath.flags(ios_base::fixed);
    m_curPath.precision(clPainterDefaultPrecision);
    PdfLocaleImbue(m_curPath);
}

PdfPainter::~PdfPainter() noexcept(false)
{
    // Throwing exceptions in C++ destructors is not allowed.
    // Just log the error.
    // PODOFO_RAISE_LOGIC_IF( m_Canvas, "FinishPage() has to be called after a page is completed!" );
    // Note that we can't do this for the user, since FinishPage() might
    // throw and we can't safely have that in a dtor. That also means
    // we can't throw here, but must abort.
    if (m_stream != nullptr)
    {
        PdfError::LogMessage(LogSeverity::Error,
            "PdfPainter::~PdfPainter(): FinishPage() has to be called after a page is completed!");
    }

    PODOFO_ASSERT(m_stream == nullptr);
}

void PdfPainter::SetCanvas(PdfCanvas* canvas)
{
    // Ignore setting the same canvas twice
    if (m_canvas == canvas)
        return;

    finishDrawing();

    m_canvas = canvas;
    m_stream = nullptr;
    m_currentTextRenderingMode = PdfTextRenderingMode::Fill;
}

void PdfPainter::FinishDrawing()
{
    try
    {
        finishDrawing();
    }
    catch (PdfError& e)
    {
        // clean up, even in case of error
        m_stream = nullptr;
        m_canvas = nullptr;
        throw e;
    }

    m_stream = nullptr;
    m_canvas = nullptr;
    m_currentTextRenderingMode = PdfTextRenderingMode::Fill;
}

void PdfPainter::finishDrawing()
{
    if (m_stream != nullptr)
    {
        if ((m_flags & EPdfPainterFlags::NoSaveRestorePrior) == EPdfPainterFlags::NoSaveRestorePrior)
        {
            // GetLength() must be called before BeginAppend()
            if (m_stream->GetLength() == 0)
            {
                m_stream->BeginAppend(false);
            }
            else
            {
                m_stream->BeginAppend(false);
                // there is already content here - so let's assume we are appending
                // as such, we MUST put in a "space" to separate whatever we do.
                m_stream->Append("\n");
            }
        }
        else
        {
            PdfMemoryOutputStream memstream;
            if (m_stream->GetLength() != 0)
                m_stream->GetFilteredCopy(memstream);

            size_t length = memstream.GetLength();
            if (length == 0)
            {
                m_stream->BeginAppend(false);
            }
            else
            {
                m_stream->BeginAppend(true);
                m_stream->Append("q\n");
                m_stream->Append(memstream.GetBuffer(), length);
                m_stream->Append("Q\n");
            }
        }

        if ((m_flags & EPdfPainterFlags::NoSaveRestore) == EPdfPainterFlags::NoSaveRestore)
        {
            m_stream->Append(m_tmpStream.str());
        }
        else
        {
            m_stream->Append("q\n");
            m_stream->Append(m_tmpStream.str());
            m_stream->Append("Q\n");
        }

        m_stream->EndAppend();
    }

    // Reset temporary stream
    m_tmpStream.str("");
}

void PdfPainter::SetStrokingShadingPattern(const PdfShadingPattern& rPattern)
{
    CheckStream();
    this->AddToPageResources(rPattern.GetIdentifier(), rPattern.GetObject().GetIndirectReference(), "Pattern");
    m_tmpStream << "/Pattern CS /" << rPattern.GetIdentifier().GetString() << " SCN" << std::endl;
}

void PdfPainter::SetShadingPattern(const PdfShadingPattern& rPattern)
{
    CheckStream();
    this->AddToPageResources(rPattern.GetIdentifier(), rPattern.GetObject().GetIndirectReference(), "Pattern");
    m_tmpStream << "/Pattern cs /" << rPattern.GetIdentifier().GetString() << " scn" << std::endl;
}

void PdfPainter::SetStrokingTilingPattern(const PdfTilingPattern& rPattern)
{
    CheckStream();
    this->AddToPageResources(rPattern.GetIdentifier(), rPattern.GetObject().GetIndirectReference(), "Pattern");
    m_tmpStream << "/Pattern CS /" << rPattern.GetIdentifier().GetString() << " SCN" << std::endl;
}

void PdfPainter::SetStrokingTilingPattern(const string_view& rPatternName)
{
    CheckStream();
    m_tmpStream << "/Pattern CS /" << rPatternName << " SCN" << std::endl;
}

void PdfPainter::SetTilingPattern(const PdfTilingPattern& rPattern)
{
    CheckStream();
    this->AddToPageResources(rPattern.GetIdentifier(), rPattern.GetObject().GetIndirectReference(), "Pattern");
    m_tmpStream << "/Pattern cs /" << rPattern.GetIdentifier().GetString() << " scn" << std::endl;
}

void PdfPainter::SetTilingPattern(const std::string_view& rPatternName)
{
    CheckStream();
    m_tmpStream << "/Pattern cs /" << rPatternName << " scn" << std::endl;
}

void PdfPainter::SetStrokingColor(const PdfColor& color)
{
    CheckStream();
    switch (color.GetColorSpace())
    {
        default:
        case PdfColorSpace::DeviceRGB:
        {
            m_tmpStream << color.GetRed() << " "
                << color.GetGreen() << " "
                << color.GetBlue()
                << " RG" << std::endl;
            break;
        }
        case PdfColorSpace::DeviceCMYK:
        {
            m_tmpStream << color.GetCyan() << " "
                << color.GetMagenta() << " "
                << color.GetYellow() << " "
                << color.GetBlack()
                << " K" << std::endl;
            break;
        }
        case PdfColorSpace::DeviceGray:
        {
            m_tmpStream << color.GetGrayScale() << " G" << std::endl;
            break;
        }
        case PdfColorSpace::Separation:
        {
            m_canvas->AddColorResource(color);
            m_tmpStream << "/ColorSpace" << PdfName(color.GetName()).GetEscapedName() << " CS " << color.GetDensity() << " SCN" << std::endl;
            break;
        }
        case PdfColorSpace::CieLab:
        {
            m_canvas->AddColorResource(color);
            m_tmpStream << "/ColorSpaceCieLab" << " CS "
                << color.GetCieL() << " "
                << color.GetCieA() << " "
                << color.GetCieB() <<
                " SCN" << std::endl;
            break;
        }
        case PdfColorSpace::Unknown:
        case PdfColorSpace::Indexed:
        {
            PODOFO_RAISE_ERROR(EPdfError::CannotConvertColor);
        }
    }
}

void PdfPainter::SetColor(const PdfColor& color)
{
    CheckStream();
    m_isCurColorICCDepend = false;
    m_curColor = color;
    switch (color.GetColorSpace())
    {
        default:
        case PdfColorSpace::DeviceRGB:
        {
            m_tmpStream << color.GetRed() << " "
                << color.GetGreen() << " "
                << color.GetBlue()
                << " rg" << std::endl;
            break;
        }
        case PdfColorSpace::DeviceCMYK:
        {
            m_tmpStream << color.GetCyan() << " "
                << color.GetMagenta() << " "
                << color.GetYellow() << " "
                << color.GetBlack()
                << " k" << std::endl;
            break;
        }
        case PdfColorSpace::DeviceGray:
        {
            m_tmpStream << color.GetGrayScale() << " g" << std::endl;
            break;
        }
        case PdfColorSpace::Separation:
        {
            m_canvas->AddColorResource(color);
            m_tmpStream << "/ColorSpace" << PdfName(color.GetName()).GetEscapedName() << " cs " << color.GetDensity() << " scn" << std::endl;
            break;
        }
        case PdfColorSpace::CieLab:
        {
            m_canvas->AddColorResource(color);
            m_tmpStream << "/ColorSpaceCieLab" << " cs "
                << color.GetCieL() << " "
                << color.GetCieA() << " "
                << color.GetCieB() <<
                " scn" << std::endl;
            break;
        }
        case PdfColorSpace::Unknown:
        case PdfColorSpace::Indexed:
        {
            PODOFO_RAISE_ERROR(EPdfError::CannotConvertColor);
        }
    }
}

void PdfPainter::SetStrokeWidth(double width)
{
    CheckStream();
    m_tmpStream << width << " w" << std::endl;
}

void PdfPainter::SetStrokeStyle(PdfStrokeStyle strokeStyle, const string_view& custom, bool inverted, double scale, bool subtractJoinCap)
{
    bool have = false;
    CheckStream();
    if (strokeStyle != PdfStrokeStyle::Custom)
        m_tmpStream << "[";

    if (inverted && strokeStyle != PdfStrokeStyle::Solid && strokeStyle != PdfStrokeStyle::Custom)
        m_tmpStream << "0 ";

    switch (strokeStyle)
    {
        case PdfStrokeStyle::Solid:
        {
            have = true;
            break;
        }
        case PdfStrokeStyle::Dash:
        {
            have = true;
            if (scale >= 1.0 - 1e-5 && scale <= 1.0 + 1e-5)
            {
                m_tmpStream << "6 2";
            }
            else
            {
                if (subtractJoinCap)
                {
                    m_tmpStream << scale * 2.0 << " " << scale * 2.0;
                }
                else
                {
                    m_tmpStream << scale * 3.0 << " " << scale * 1.0;
                }
            }
            break;
        }
        case PdfStrokeStyle::Dot:
        {
            have = true;
            if (scale >= 1.0 - 1e-5 && scale <= 1.0 + 1e-5)
            {
                m_tmpStream << "2 2";
            }
            else
            {
                if (subtractJoinCap)
                {
                    // zero length segments are drawn anyway here
                    m_tmpStream << 0.001 << " " << 2.0 * scale << " " << 0 << " " << 2.0 * scale;
                }
                else
                {
                    m_tmpStream << scale * 1.0 << " " << scale * 1.0;
                }
            }
            break;
        }
        case PdfStrokeStyle::DashDot:
        {
            have = true;
            if (scale >= 1.0 - 1e-5 && scale <= 1.0 + 1e-5)
            {
                m_tmpStream << "3 2 1 2";
            }
            else
            {
                if (subtractJoinCap)
                {
                    // zero length segments are drawn anyway here
                    m_tmpStream << scale * 2.0 << " " << scale * 2.0 << " " << 0 << " " << scale * 2.0;
                }
                else
                {
                    m_tmpStream << scale * 3.0 << " " << scale * 1.0 << " " << scale * 1.0 << " " << scale * 1.0;
                }
            }
            break;
        }
        case PdfStrokeStyle::DashDotDot:
        {
            have = true;
            if (scale >= 1.0 - 1e-5 && scale <= 1.0 + 1e-5)
            {
                m_tmpStream << "3 1 1 1 1 1";
            }
            else
            {
                if (subtractJoinCap)
                {
                    // zero length segments are drawn anyway here
                    m_tmpStream << scale * 2.0 << " " << scale * 2.0 << " " << 0 << " " << scale * 2.0 << " " << 0 << " " << scale * 2.0;
                }
                else {
                    m_tmpStream << scale * 3.0 << " " << scale * 1.0 << " " << scale * 1.0 << " " << scale * 1.0 << " " << scale * 1.0 << " " << scale * 1.0;
                }
            }
            break;
        }
        case PdfStrokeStyle::Custom:
        {
            have = !custom.empty();
            if (have)
                m_tmpStream << custom;
            break;
        }
        default:
        {
            PODOFO_RAISE_ERROR(EPdfError::InvalidStrokeStyle);
        }
    }

    if (!have)
        PODOFO_RAISE_ERROR(EPdfError::InvalidStrokeStyle);

    if (inverted && strokeStyle != PdfStrokeStyle::Solid && strokeStyle != PdfStrokeStyle::Custom)
        m_tmpStream << " 0";

    if (strokeStyle != PdfStrokeStyle::Custom)
        m_tmpStream << "] 0";

    m_tmpStream << " d" << std::endl;
}

void PdfPainter::SetLineCapStyle(PdfLineCapStyle eCapStyle)
{
    CheckStream();
    m_tmpStream << static_cast<int>(eCapStyle) << " J" << std::endl;
}

void PdfPainter::SetLineJoinStyle(PdfLineJoinStyle eJoinStyle)
{
    CheckStream();
    m_tmpStream << static_cast<int>(eJoinStyle) << " j" << std::endl;
}

void PdfPainter::SetFont(PdfFont* font)
{
    CheckStream();
    m_Font = font;
}

void PdfPainter::SetTextRenderingMode(PdfTextRenderingMode mode)
{
    CheckStream();
    if (mode == m_currentTextRenderingMode)
        return;

    m_currentTextRenderingMode = mode;
    if (m_isTextOpen)
        SetCurrentTextRenderingMode();
}

void PdfPainter::SetCurrentTextRenderingMode()
{
    CheckStream();
    m_tmpStream << (int)m_currentTextRenderingMode << " Tr" << std::endl;
}

void PdfPainter::SetClipRect(double x, double y, double width, double height)
{
    CheckStream();
    m_tmpStream << x << " "
        << y << " "
        << width << " "
        << height
        << " re W n" << std::endl;

    m_curPath
        << x << " "
        << y << " "
        << width << " "
        << height
        << " re W n" << std::endl;
}

void PdfPainter::SetMiterLimit(double value)
{
    CheckStream();
    m_tmpStream << value << " M" << std::endl;
}

void PdfPainter::DrawLine(double startX, double startY, double endX, double endY)
{
    CheckStream();
    m_curPath.str("");
    m_curPath
        << startX << " "
        << startY
        << " m "
        << endX << " "
        << endY
        << " l" << std::endl;

    m_tmpStream << startX << " "
        << startY
        << " m "
        << endX << " "
        << endY
        << " l S" << std::endl;
}

void PdfPainter::Rectangle(double x, double y, double width, double height,
    double roundX, double roundY)
{
    CheckStream();
    if (static_cast<int>(roundX) || static_cast<int>(roundY))
    {
        double w = width, h = height,
            rx = roundX, ry = roundY;
        double b = 0.4477f;

        MoveTo(x + rx, y);
        LineTo(x + w - rx, y);
        CubicBezierTo(x + w - rx * b, y, x + w, y + ry * b, x + w, y + ry);
        LineTo(x + w, y + h - ry);
        CubicBezierTo(x + w, y + h - ry * b, x + w - rx * b, y + h, x + w - rx, y + h);
        LineTo(x + rx, y + h);
        CubicBezierTo(x + rx * b, y + h, x, y + h - ry * b, x, y + h - ry);
        LineTo(x, y + ry);
        CubicBezierTo(x, y + ry * b, x + rx * b, y, x + rx, y);
    }
    else
    {
        m_curPath
            << x << " "
            << y << " "
            << width << " "
            << height
            << " re" << std::endl;

        m_tmpStream << x << " "
            << y << " "
            << width << " "
            << height
            << " re" << std::endl;
    }
}

void PdfPainter::Ellipse(double x, double y, double width, double height)
{
    double dPointX[BEZIER_POINTS];
    double dPointY[BEZIER_POINTS];
    int i;

    CheckStream();

    ConvertRectToBezier(x, y, width, height, dPointX, dPointY);

    m_curPath
        << dPointX[0] << " "
        << dPointY[0]
        << " m" << std::endl;

    m_tmpStream << dPointX[0] << " "
        << dPointY[0]
        << " m" << std::endl;

    for (i = 1; i < BEZIER_POINTS; i += 3)
    {
        m_curPath
            << dPointX[i] << " "
            << dPointY[i] << " "
            << dPointX[i + 1] << " "
            << dPointY[i + 1] << " "
            << dPointX[i + 2] << " "
            << dPointY[i + 2]
            << " c" << std::endl;

        m_tmpStream << dPointX[i] << " "
            << dPointY[i] << " "
            << dPointX[i + 1] << " "
            << dPointY[i + 1] << " "
            << dPointX[i + 2] << " "
            << dPointY[i + 2]
            << " c" << std::endl;
    }
}

void PdfPainter::Circle(double x, double y, double radius)
{
    CheckStream();

    // draw four Bezier curves to approximate a circle
    MoveTo(x + radius, y);
    CubicBezierTo(x + radius, y + radius * ARC_MAGIC,
        x + radius * ARC_MAGIC, y + radius,
        x, y + radius);
    CubicBezierTo(x - radius * ARC_MAGIC, y + radius,
        x - radius, y + radius * ARC_MAGIC,
        x - radius, y);
    CubicBezierTo(x - radius, y - radius * ARC_MAGIC,
        x - radius * ARC_MAGIC, y - radius,
        x, y - radius);
    CubicBezierTo(x + radius * ARC_MAGIC, y - radius,
        x + radius, y - radius * ARC_MAGIC,
        x + radius, y);
    Close();
}

void PdfPainter::DrawText(double x, double y, const string_view& str)
{
    CheckStream();
    if (m_Font == nullptr)
        PODOFO_RAISE_ERROR_INFO(EPdfError::InvalidHandle, "Font should be set prior calling the method");

    auto expStr = this->ExpandTabs(str);
    this->AddToPageResources(m_Font->GetIdentifier(), m_Font->GetObject().GetIndirectReference(), "Font");

    if (m_textState.IsUnderlined() || m_textState.IsStrikeOut())
    {
        this->Save();
        this->SetCurrentStrokingColor();

        // Draw underline
        this->SetStrokeWidth(m_Font->GetUnderlineThickness(m_textState));
        if (m_textState.IsUnderlined())
        {
            this->DrawLine(x,
                y + m_Font->GetUnderlinePosition(m_textState),
                x + m_Font->GetStringWidth(expStr, m_textState),
                y + m_Font->GetUnderlinePosition(m_textState));
        }

        // Draw strikeout
        this->SetStrokeWidth(m_Font->GetStrikeOutThickness(m_textState));
        if (m_textState.IsStrikeOut())
        {
            this->DrawLine(x,
                y + m_Font->GetStrikeOutPosition(m_textState),
                x + m_Font->GetStringWidth(expStr, m_textState),
                y + m_Font->GetStrikeOutPosition(m_textState));
        }

        this->Restore();
    }

    m_tmpStream << "BT" << std::endl << "/" << m_Font->GetIdentifier().GetString()
        << " " << m_textState.GetFontSize()
        << " Tf" << std::endl;

    if (m_currentTextRenderingMode != PdfTextRenderingMode::Fill)
        SetCurrentTextRenderingMode();

    m_tmpStream << m_textState.GetFontScale() * 100 << " Tz" << std::endl;
    m_tmpStream << m_textState.GetCharSpace() * (double)m_textState.GetFontSize() / 100.0 << " Tc" << std::endl;

    m_tmpStream << x << std::endl << y << std::endl << "Td ";
    m_Font->WriteStringToStream(m_tmpStream, expStr);
    m_tmpStream << " Tj\nET\n";
}

void PdfPainter::BeginText(double x, double y)
{
    CheckStream();
    if (m_Font == nullptr)
        PODOFO_RAISE_ERROR_INFO(EPdfError::InvalidHandle, "Font should be set prior calling the method");

    if (m_isTextOpen)
        PODOFO_RAISE_ERROR_INFO(EPdfError::InternalLogic, "Text writing is already opened");

    this->AddToPageResources(m_Font->GetIdentifier(), m_Font->GetObject().GetIndirectReference(), "Font");

    m_tmpStream << "BT" << std::endl << "/" << m_Font->GetIdentifier().GetString()
        << " " << m_textState.GetFontSize()
        << " Tf" << std::endl;

    if (m_currentTextRenderingMode != PdfTextRenderingMode::Fill) {
        SetCurrentTextRenderingMode();
    }

    m_tmpStream << m_textState.GetFontScale() * 100 << " Tz" << std::endl;
    m_tmpStream << m_textState.GetCharSpace() * (double)m_textState.GetFontSize() / 100.0 << " Tc" << std::endl;

    m_tmpStream << x << " " << y << " Td" << std::endl;

    m_isTextOpen = true;
}

void PdfPainter::MoveTextPos(double x, double y)
{
    CheckStream();
    if (m_Font == nullptr)
        PODOFO_RAISE_ERROR_INFO(EPdfError::InvalidHandle, "Font should be set prior calling the method");

    if (!m_isTextOpen)
        PODOFO_RAISE_ERROR_INFO(EPdfError::InternalLogic, "Text writing is not opened");

    m_tmpStream << x << " " << y << " Td" << std::endl;
}

void PdfPainter::AddText(const string_view& str)
{
    CheckStream();
    if (m_Font == nullptr)
        PODOFO_RAISE_ERROR_INFO(EPdfError::InvalidHandle, "Font should be set prior calling the method");

    if (!m_isTextOpen)
        PODOFO_RAISE_ERROR_INFO(EPdfError::InternalLogic, "Text writing is not opened");

    auto expStr = this->ExpandTabs(str);

    // TODO: Underline and Strikeout not yet supported
    m_Font->WriteStringToStream(m_tmpStream, expStr);

    m_tmpStream << " Tj\n";
}

void PdfPainter::EndText()
{
    CheckStream();
    if (m_Font == nullptr)
        PODOFO_RAISE_ERROR_INFO(EPdfError::InvalidHandle, "Font should be set prior calling the method");

    if (!m_isTextOpen)
        PODOFO_RAISE_ERROR_INFO(EPdfError::InternalLogic, "Text writing is not opened");

    m_tmpStream << "ET\n";
    m_isTextOpen = false;
}

void PdfPainter::DrawMultiLineText(double x, double y, double width, double height, const string_view& str,
    PdfHorizontalAlignment hAlignment, PdfVerticalAlignment vAlignment, bool clip, bool skipSpaces)
{
    CheckStream();
    if (m_Font == nullptr)
        PODOFO_RAISE_ERROR_INFO(EPdfError::InvalidHandle, "Font should be set prior calling the method");

    if (width <= 0.0 || height <= 0.0) // nonsense arguments
        return;

    this->Save();
    if (clip)
        this->SetClipRect(x, y, width, height);

    auto expanded = this->ExpandTabs(str);

    vector<string> lines = GetMultiLineTextAsLines(width, expanded, skipSpaces);
    double dLineGap = m_Font->GetLineSpacing(m_textState) - m_Font->GetAscent(m_textState) + m_Font->GetDescent(m_textState);
    // Do vertical alignment
    switch (vAlignment)
    {
        default:
        case PdfVerticalAlignment::Top:
            y += height; break;
        case PdfVerticalAlignment::Bottom:
            y += m_Font->GetLineSpacing(m_textState) * lines.size(); break;
        case PdfVerticalAlignment::Center:
            y += (height -
                ((height - (m_Font->GetLineSpacing(m_textState) * lines.size())) / 2.0));
            break;
    }

    y -= (m_Font->GetAscent(m_textState) + dLineGap / (2.0));

    vector<string>::const_iterator it = lines.begin();
    while (it != lines.end())
    {
        if (it->length() != 0)
            this->DrawTextAligned(x, y, width, *it, hAlignment);

        y -= m_Font->GetLineSpacing(m_textState);
        it++;
    }
    this->Restore();
}

// FIX-ME/CLEAN-ME: The following is a shit load of fuck that was found
// in this deprecable state while cleaning the code
// Fix/clean it to support Unicode, remove hungarian notation and simplify the code
vector<string> PdfPainter::GetMultiLineTextAsLines(double width, const string_view& text, bool skipSpaces)
{
    // FIX-ME: This method is currently broken, just rewrite it later
    vector<string> ret = { (string)text };
    return ret;

    if (width <= 0.0) // nonsense arguments
        return vector<string>();

    if (text.length() == 0) // empty string
        return vector<string>(1, (string)text);

    const char* const stringBegin = text.data();
    const char* lineBegin = stringBegin;
    const char* currentCharacter = stringBegin;
    const char* startOfCurrentWord = stringBegin;
    bool startOfWord = true;
    double curWidthOfLine = 0.0;
    vector<string> lines;

    // do simple word wrapping
    while (*currentCharacter)
    {
        if (IsNewLineChar(*currentCharacter)) // hard-break!
        {
            lines.push_back(string({ lineBegin, (size_t)(currentCharacter - lineBegin) }));

            lineBegin = currentCharacter + 1; // skip the line feed
            startOfWord = true;
            curWidthOfLine = 0.0;
        }
        else if (IsSpaceChar(*currentCharacter))
        {
            if (curWidthOfLine > width)
            {
                // The previous word does not fit in the current line.
                // -> Move it to the next one.
                if (startOfCurrentWord > lineBegin)
                {
                    lines.push_back(string({ lineBegin, (size_t)(startOfCurrentWord - lineBegin) }));
                }
                else
                {
                    lines.push_back(string({ lineBegin, (size_t)(currentCharacter - lineBegin) }));
                    if (skipSpaces)
                    {
                        // Skip all spaces at the end of the line
                        while (IsSpaceChar(*(currentCharacter + 1)))
                            currentCharacter++;

                        startOfCurrentWord = currentCharacter + 1;
                    }
                    else
                    {
                        startOfCurrentWord = currentCharacter;
                    }
                    startOfWord = true;
                }
                lineBegin = startOfCurrentWord;

                if (!startOfWord)
                {
                    curWidthOfLine = m_Font->GetStringWidth(
                        { startOfCurrentWord, (size_t)(currentCharacter - startOfCurrentWord) },
                        m_textState);
                }
                else
                {
                    curWidthOfLine = 0.0;
                }
            }
            ////else if( ( dCurWidthOfLine + m_Font->GetCharWidth( *currentCharacter, m_textState) ) > width )
            {
                lines.push_back(string({ lineBegin, (size_t)(currentCharacter - lineBegin) }));
                if (skipSpaces)
                {
                    // Skip all spaces at the end of the line
                    while (IsSpaceChar(*(currentCharacter + 1)))
                        currentCharacter++;

                    startOfCurrentWord = currentCharacter + 1;
                }
                else
                {
                    startOfCurrentWord = currentCharacter;
                }
                lineBegin = startOfCurrentWord;
                startOfWord = true;
                curWidthOfLine = 0.0;
            }
            ////else
            {
                ////    dCurWidthOfLine += m_Font->GetCharWidth( *currentCharacter, m_textState);
            }

            startOfWord = true;
        }
        else
        {
            if (startOfWord)
            {
                startOfCurrentWord = currentCharacter;
                startOfWord = false;
            }
            //else do nothing

            ////if ((dCurWidthOfLine + m_Font->GetCharWidth(*currentCharacter, m_textState)) > width)
            {
                if (lineBegin == startOfCurrentWord)
                {
                    // This word takes up the whole line.
                    // Put as much as possible on this line.
                    if (lineBegin == currentCharacter)
                    {
                        lines.push_back(string({ currentCharacter, 1 }));
                        lineBegin = currentCharacter + 1;
                        startOfCurrentWord = currentCharacter + 1;
                        curWidthOfLine = 0;
                    }
                    else
                    {
                        lines.push_back(string({ lineBegin, (size_t)(currentCharacter - lineBegin) }));
                        lineBegin = currentCharacter;
                        startOfCurrentWord = currentCharacter;
                        ////dCurWidthOfLine = m_Font->GetCharWidth(*currentCharacter, m_textState);
                    }
                }
                else
                {
                    // The current word does not fit in the current line.
                    // -> Move it to the next one.
                    lines.push_back(string({ lineBegin, (size_t)(startOfCurrentWord - lineBegin) }));
                    lineBegin = startOfCurrentWord;
                    curWidthOfLine = m_Font->GetStringWidth({ startOfCurrentWord, (size_t)((currentCharacter - startOfCurrentWord) + 1) }, m_textState);
                }
            }
            ////else
            {
                ////    dCurWidthOfLine += m_Font->GetCharWidth(*currentCharacter, m_textState);
            }
        }
        currentCharacter++;
    }

    if ((currentCharacter - lineBegin) > 0)
    {
        if (curWidthOfLine > width && startOfCurrentWord > lineBegin)
        {
            // The previous word does not fit in the current line.
            // -> Move it to the next one.
            lines.push_back(string({ lineBegin, (size_t)(startOfCurrentWord - lineBegin) }));
            lineBegin = startOfCurrentWord;
        }
        //else do nothing

        if (currentCharacter - lineBegin > 0)
        {
            lines.push_back(string({ lineBegin, (size_t)(currentCharacter - lineBegin) }));
        }
        //else do nothing
    }

    return lines;
}

void PdfPainter::DrawTextAligned(double x, double y, double width, const string_view& str, PdfHorizontalAlignment hAlignment)
{
    CheckStream();
    if (m_Font == nullptr)
        PODOFO_RAISE_ERROR_INFO(EPdfError::InvalidHandle, "Font should be set prior calling the method");

    if (width <= 0.0) // nonsense arguments
        return;

    switch (hAlignment)
    {
        default:
        case PdfHorizontalAlignment::Left:
            break;
        case PdfHorizontalAlignment::Center:
            x += (width - m_Font->GetStringWidth(str, m_textState)) / 2.0;
            break;
        case PdfHorizontalAlignment::Right:
            x += (width - m_Font->GetStringWidth(str, m_textState));
            break;
    }

    this->DrawText(x, y, str);
}

void PdfPainter::DrawImage(double x, double y, const PdfImage& obj, double scaleX, double scaleY)
{
    this->DrawXObject(x, y, obj,
        scaleX * obj.GetRect().GetWidth(),
        scaleY * obj.GetRect().GetHeight());
}

void PdfPainter::DrawXObject(double x, double y, const PdfXObject& obj, double scaleX, double scaleY)
{
    CheckStream();

    // use OriginalReference() as the XObject might have been written to disk
    // already and is not in memory anymore in this case.
    this->AddToPageResources(obj.GetIdentifier(), obj.GetObjectReference(), "XObject");

    std::streamsize oldPrecision = m_tmpStream.precision(clPainterHighPrecision);
    m_tmpStream << "q" << std::endl
        << scaleX << " 0 0 "
        << scaleY << " "
        << x << " "
        << y << " cm" << std::endl
        << "/" << obj.GetIdentifier().GetString() << " Do" << std::endl << "Q" << std::endl;
    m_tmpStream.precision(oldPrecision);
}

void PdfPainter::ClosePath()
{
    CheckStream();
    m_curPath << "h" << std::endl;
    m_tmpStream << "h\n";
}

void PdfPainter::LineTo(double x, double y)
{
    CheckStream();
    m_curPath
        << x << " "
        << y
        << " l" << std::endl;

    m_tmpStream << x << " "
        << y
        << " l" << std::endl;
}

void PdfPainter::MoveTo(double x, double y)
{
    CheckStream();
    m_curPath
        << x << " "
        << y
        << " m" << std::endl;

    m_tmpStream << x << " "
        << y
        << " m" << std::endl;
}

void PdfPainter::CubicBezierTo(double x1, double y1, double x2, double y2, double x3, double y3)
{
    CheckStream();
    m_curPath
        << x1 << " "
        << y1 << " "
        << x2 << " "
        << y2 << " "
        << x3 << " "
        << y3
        << " c" << std::endl;

    m_tmpStream << x1 << " "
        << y1 << " "
        << x2 << " "
        << y2 << " "
        << x3 << " "
        << y3
        << " c" << std::endl;
}

void PdfPainter::HorizontalLineTo(double x)
{
    LineTo(x, m_lpy3);
}

void PdfPainter::VerticalLineTo(double y)
{
    LineTo(m_lpx3, y);
}

void PdfPainter::SmoothCurveTo(double x2, double y2, double x3, double y3)
{
    double px, py, px2 = x2;
    double py2 = y2;
    double px3 = x3;
    double py3 = y3;

    // compute the reflective points (thanks Raph!)
    px = 2 * m_lcx - m_lrx;
    py = 2 * m_lcy - m_lry;

    m_lpx = px;
    m_lpy = py;
    m_lpx2 = px2;
    m_lpy2 = py2;
    m_lpx3 = px3;
    m_lpy3 = py3;
    m_lcx = px3;
    m_lcy = py3;
    m_lrx = px2;
    m_lry = py2;

    CubicBezierTo(px, py, px2, py2, px3, py3);
}

void PdfPainter::QuadCurveTo(double x1, double y1, double x3, double y3)
{
    double px = x1, py = y1,
        px2, py2,
        px3 = x3, py3 = y3;

    // raise quadratic bezier to cubic - thanks Raph!
    // http://www.icce.rug.nl/erikjan/bluefuzz/beziers/beziers/beziers.html
    px = (m_lcx + 2 * px) * (1.0 / 3.0);
    py = (m_lcy + 2 * py) * (1.0 / 3.0);
    px2 = (px3 + 2 * px) * (1.0 / 3.0);
    py2 = (py3 + 2 * py) * (1.0 / 3.0);

    m_lpx = px;
    m_lpy = py;
    m_lpx2 = px2;
    m_lpy2 = py2;
    m_lpx3 = px3;
    m_lpy3 = py3;
    m_lcx = px3;
    m_lcy = py3;
    m_lrx = px2;
    m_lry = py2;

    CubicBezierTo(px, py, px2, py2, px3, py3);
}

void PdfPainter::SmoothQuadCurveTo(double x3, double y3)
{
    double px, py, px2, py2,
        px3 = x3, py3 = y3;

    double xc, yc; // quadratic control point
    xc = 2 * m_lcx - m_lrx;
    yc = 2 * m_lcy - m_lry;

    // generate a quadratic bezier with control point = xc, yc
    px = (m_lcx + 2 * xc) * (1.0 / 3.0);
    py = (m_lcy + 2 * yc) * (1.0 / 3.0);
    px2 = (px3 + 2 * xc) * (1.0 / 3.0);
    py2 = (py3 + 2 * yc) * (1.0 / 3.0);

    m_lpx = px; m_lpy = py; m_lpx2 = px2; m_lpy2 = py2; m_lpx3 = px3; m_lpy3 = py3;
    m_lcx = px3;    m_lcy = py3;    m_lrx = xc;    m_lry = yc;    // thanks Raph!

    CubicBezierTo(px, py, px2, py2, px3, py3);
}

void PdfPainter::ArcTo(double x, double y, double radiusX, double radiusY,
    double rotation, bool large, bool sweep)
{
    double px = x, py = y;
    double rx = radiusX, ry = radiusY, rot = rotation;

    double sin_th, cos_th;
    double a00, a01, a10, a11;
    double x0, y0, x1, y1, xc, yc;
    double d, sfactor, sfactor_sq;
    double th0, th1, th_arc;
    int i, n_segs;

    sin_th = sin(rot * (M_PI / 180));
    cos_th = cos(rot * (M_PI / 180));
    a00 = cos_th / rx;
    a01 = sin_th / rx;
    a10 = -sin_th / ry;
    a11 = cos_th / ry;
    x0 = a00 * m_lcx + a01 * m_lcy;
    y0 = a10 * m_lcx + a11 * m_lcy;
    x1 = a00 * px + a01 * py;
    y1 = a10 * px + a11 * py;
    // (x0, y0) is current point in transformed coordinate space.
    // (x1, y1) is new point in transformed coordinate space.

    // The arc fits a unit-radius circle in this space.
    d = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0);
    sfactor_sq = 1.0 / d - 0.25;
    if (sfactor_sq < 0)
        sfactor_sq = 0;
    sfactor = sqrt(sfactor_sq);
    if (sweep == large)
        sfactor = -sfactor;
    xc = 0.5 * (x0 + x1) - sfactor * (y1 - y0);
    yc = 0.5 * (y0 + y1) + sfactor * (x1 - x0);
    // (xc, yc) is center of the circle

    th0 = atan2(y0 - yc, x0 - xc);
    th1 = atan2(y1 - yc, x1 - xc);

    th_arc = th1 - th0;
    if (th_arc < 0 && sweep)
        th_arc += 2 * M_PI;
    else if (th_arc > 0 && !sweep)
        th_arc -= 2 * M_PI;

    n_segs = static_cast<int>(ceil(fabs(th_arc / (M_PI * 0.5 + 0.001))));

    for (i = 0; i < n_segs; i++)
    {
        double nth0 = th0 + (double)i * th_arc / n_segs,
            nth1 = th0 + ((double)i + 1) * th_arc / n_segs;
        double nsin_th = 0.0;
        double ncos_th = 0.0;
        double na00 = 0.0;
        double na01 = 0.0;
        double na10 = 0.0;
        double na11 = 0.0;
        double nx1 = 0.0;
        double ny1 = 0.0;
        double nx2 = 0.0;
        double ny2 = 0.0;
        double nx3 = 0.0;
        double ny3 = 0.0;
        double t = 0.0;
        double th_half = 0.0;

        nsin_th = sin(rot * (M_PI / 180));
        ncos_th = cos(rot * (M_PI / 180));
        /* inverse transform compared with rsvg_path_arc */
        na00 = ncos_th * rx;
        na01 = -nsin_th * ry;
        na10 = nsin_th * rx;
        na11 = ncos_th * ry;

        th_half = 0.5 * (nth1 - nth0);
        t = (8.0 / 3.0) * sin(th_half * 0.5) * sin(th_half * 0.5) / sin(th_half);
        nx1 = xc + cos(nth0) - t * sin(nth0);
        ny1 = yc + sin(nth0) + t * cos(nth0);
        nx3 = xc + cos(nth1);
        ny3 = yc + sin(nth1);
        nx2 = nx3 + t * sin(nth1);
        ny2 = ny3 - t * cos(nth1);
        nx1 = na00 * nx1 + na01 * ny1;
        ny1 = na10 * nx1 + na11 * ny1;
        nx2 = na00 * nx2 + na01 * ny2;
        ny2 = na10 * nx2 + na11 * ny2;
        nx3 = na00 * nx3 + na01 * ny3;
        ny3 = na10 * nx3 + na11 * ny3;
        CubicBezierTo(nx1, ny1, nx2, ny2, nx3, ny3);
    }

    m_lpx = m_lpx2 = m_lpx3 = px;
    m_lpy = m_lpy2 = m_lpy3 = py;
    m_lcx = px;
    m_lcy = py;
    m_lrx = px;
    m_lry = py;
}

bool PdfPainter::Arc(double x, double y, double radius, double angle1, double angle2)
{
    bool cont_flg = false;

    bool ret = true;

    if (angle1 >= angle2 || (angle2 - angle1) >= 360.0f)
        return false;

    while (angle1 < 0.0f || angle2 < 0.0f) {
        angle1 = angle1 + 360.0f;
        angle2 = angle2 + 360.0f;
    }

    while (true)
    {
        if (angle2 - angle1 <= 90.0f)
        {
            return InternalArc(x, y, radius, angle1, angle2, cont_flg);
        }
        else
        {
            double tmp_ang = angle1 + 90.0f;

            ret = InternalArc(x, y, radius, angle1, tmp_ang, cont_flg);
            if (!ret)
                return ret;

            angle1 = tmp_ang;
        }

        if (angle1 >= angle2)
            break;

        cont_flg = true;
    }

    return true;
}

bool PdfPainter::InternalArc(double x, double y, double ray, double ang1,
    double ang2, bool contFlg)
{
    bool ret = true;

    double rx0, ry0, rx1, ry1, rx2, ry2, rx3, ry3;
    double x0, y0, x1, y1, x2, y2, x3, y3;
    double delta_angle = (90.0f - static_cast<double>(ang1 + ang2) / 2.0f) / 180 * M_PI;
    double new_angle = static_cast<double>(ang2 - ang1) / 2.0f / 180 * M_PI;

    rx0 = ray * cos(new_angle);
    ry0 = ray * sin(new_angle);
    rx2 = (ray * 4.0f - rx0) / 3.0f;
    ry2 = ((ray * 1.0f - rx0) * (rx0 - ray * 3.0f)) / (3.0 * ry0);
    rx1 = rx2;
    ry1 = -ry2;
    rx3 = rx0;
    ry3 = -ry0;

    x0 = rx0 * cos(delta_angle) - ry0 * sin(delta_angle) + x;
    y0 = rx0 * sin(delta_angle) + ry0 * cos(delta_angle) + y;
    x1 = rx1 * cos(delta_angle) - ry1 * sin(delta_angle) + x;
    y1 = rx1 * sin(delta_angle) + ry1 * cos(delta_angle) + y;
    x2 = rx2 * cos(delta_angle) - ry2 * sin(delta_angle) + x;
    y2 = rx2 * sin(delta_angle) + ry2 * cos(delta_angle) + y;
    x3 = rx3 * cos(delta_angle) - ry3 * sin(delta_angle) + x;
    y3 = rx3 * sin(delta_angle) + ry3 * cos(delta_angle) + y;

    if (!contFlg) {
        MoveTo(x0, y0);
    }

    CubicBezierTo(x1, y1, x2, y2, x3, y3);

    //attr->cur_pos.x = (HPDF_REAL)x3;
    //attr->cur_pos.y = (HPDF_REAL)y3;
    m_lcx = x3;
    m_lcy = y3;

    m_lpx = m_lpx2 = m_lpx3 = x3;
    m_lpy = m_lpy2 = m_lpy3 = y3;
    m_lcx = x3;
    m_lcy = y3;
    m_lrx = x3;
    m_lry = y3;

    return ret;
}

void PdfPainter::Close()
{
    CheckStream();
    m_curPath << "h" << std::endl;
    m_tmpStream << "h\n";
}

void PdfPainter::Stroke()
{
    CheckStream();
    m_curPath.str("");
    m_tmpStream << "S\n";
}

void PdfPainter::Fill(bool useEvenOddRule)
{
    CheckStream();
    m_curPath.str("");
    if (useEvenOddRule)
        m_tmpStream << "f*\n";
    else
        m_tmpStream << "f\n";
}

void PdfPainter::FillAndStroke(bool useEvenOddRule)
{
    CheckStream();
    m_curPath.str("");
    if (useEvenOddRule)
        m_tmpStream << "B*\n";
    else
        m_tmpStream << "B\n";
}

void PdfPainter::Clip(bool useEvenOddRule)
{
    CheckStream();
    if (useEvenOddRule)
        m_tmpStream << "W* n\n";
    else
        m_tmpStream << "W n\n";
}

void PdfPainter::EndPath()
{
    CheckStream();
    m_curPath << "n" << std::endl;
    m_tmpStream << "n\n";
}

void PdfPainter::Save()
{
    CheckStream();
    m_tmpStream << "q\n";
}

void PdfPainter::Restore()
{
    CheckStream();
    m_tmpStream << "Q\n";
}

void PdfPainter::AddToPageResources(const PdfName& identifier, const PdfReference& ref, const PdfName& name)
{
    if (m_canvas == nullptr)
        PODOFO_RAISE_ERROR(EPdfError::InvalidHandle);

    m_canvas->AddResource(identifier, ref, name);
}

void PdfPainter::ConvertRectToBezier(double x, double y, double width, double height, double pointsX[], double pointsY[])
{
    // this function is based on code from:
    // http://www.codeguru.com/Cpp/G-M/gdi/article.php/c131/
    // (Llew Goodstadt)

    // MAGICAL CONSTANT to map ellipse to beziers = 2/3*(sqrt(2)-1)
    const double dConvert = 0.2761423749154;

    double offX = width * dConvert;
    double offY = height * dConvert;
    double centerX = x + (width / 2.0);
    double centerY = y + (height / 2.0);

    //------------------------//
    //                        //
    //        2___3___4       //
    //     1             5    //
    //     |             |    //
    //     |             |    //
    //     0,12          6    //
    //     |             |    //
    //     |             |    //
    //    11             7    //
    //       10___9___8       //
    //                        //
    //------------------------//

    pointsX[0] = pointsX[1] = pointsX[11] = pointsX[12] = x;
    pointsX[5] = pointsX[6] = pointsX[7] = x + width;
    pointsX[2] = pointsX[10] = centerX - offX;
    pointsX[4] = pointsX[8] = centerX + offX;
    pointsX[3] = pointsX[9] = centerX;

    pointsY[2] = pointsY[3] = pointsY[4] = y;
    pointsY[8] = pointsY[9] = pointsY[10] = y + height;
    pointsY[7] = pointsY[11] = centerY + offY;
    pointsY[1] = pointsY[5] = centerY - offY;
    pointsY[0] = pointsY[12] = pointsY[6] = centerY;
}

void PdfPainter::SetCurrentStrokingColor()
{
    if (m_isCurColorICCDepend)
    {
        m_tmpStream << "/" << m_CSTag << " CS ";
        m_tmpStream << m_curColor.GetRed() << " "
            << m_curColor.GetGreen() << " "
            << m_curColor.GetBlue()
            << " SC" << std::endl;
    }
    else
    {
        SetStrokingColor(m_curColor);
    }
}

void PdfPainter::SetTransformationMatrix(double a, double b, double c, double d, double e, double f)
{
    CheckStream();

    // Need more precision for transformation-matrix !!
    std::streamsize oldPrecision = m_tmpStream.precision(clPainterHighPrecision);
    m_tmpStream << a << " "
        << b << " "
        << c << " "
        << d << " "
        << e << " "
        << f << " cm" << std::endl;
    m_tmpStream.precision(oldPrecision);
}

void PdfPainter::SetExtGState(const PdfExtGState& inGState)
{
    CheckStream();
    this->AddToPageResources(inGState.GetIdentifier(), inGState.GetObject().GetIndirectReference(), "ExtGState");
    m_tmpStream << "/" << inGState.GetIdentifier().GetString()
        << " gs" << std::endl;
}

void PdfPainter::SetRenderingIntent(const string_view& intent)
{
    CheckStream();
    m_tmpStream << "/" << intent << " ri" << std::endl;
}

void PdfPainter::SetDependICCProfileColor(const PdfColor& color, const string_view& csTag)
{
    m_isCurColorICCDepend = true;
    m_curColor = color;
    m_CSTag = csTag;

    m_tmpStream << "/" << m_CSTag << " cs ";
    m_tmpStream << color.GetRed() << " "
        << color.GetGreen() << " "
        << color.GetBlue()
        << " sc" << std::endl;
}

string PdfPainter::ExpandTabs(const string_view& str) const
{
    unsigned tabCount = 0;
    auto it = str.begin();
    auto end = str.end();
    while (it != end)
    {
        char32_t ch = (char32_t)utf8::next(it, end);
        if (ch == U'\t')
            tabCount++;
    }

    // if no tabs are found: bail out!
    if (tabCount == 0)
        return (string)str;

    return expandTabs(str, m_TabWidth, tabCount);
}

void PdfPainter::SetPrecision(unsigned short precision)
{
    m_tmpStream.precision(precision);
}

unsigned short PdfPainter::GetPrecision() const
{
    return static_cast<unsigned short>(m_tmpStream.precision());
}

void PdfPainter::SetClipRect(const PdfRect& rect)
{
    this->SetClipRect(rect.GetLeft(), rect.GetBottom(), rect.GetWidth(), rect.GetHeight());
}

void PdfPainter::Rectangle(const PdfRect& rect, double roundX, double roundY)
{
    this->Rectangle(rect.GetLeft(), rect.GetBottom(),
        rect.GetWidth(), rect.GetHeight(),
        roundX, roundY);
}

void PdfPainter::DrawMultiLineText(const PdfRect& rect, const string_view& str,
    PdfHorizontalAlignment hAlignment, PdfVerticalAlignment vAlignment, bool clip, bool skipSpaces)
{
    this->DrawMultiLineText(rect.GetLeft(), rect.GetBottom(), rect.GetWidth(), rect.GetHeight(),
        str, hAlignment, vAlignment, clip, skipSpaces);
}

void PdfPainter::CheckStream()
{
    if (m_stream != nullptr)
        return;

    PODOFO_RAISE_LOGIC_IF(m_canvas == nullptr, "Call SetCanvas() first before doing drawing operations");
    m_stream = &m_canvas->GetStreamForAppending((EPdfStreamAppendFlags)(m_flags & (~EPdfPainterFlags::NoSaveRestore)));
}

string expandTabs(const string_view& str, unsigned tabWidth, unsigned tabCount)
{
    auto it = str.begin();
    auto end = str.end();

    string ret;
    ret.reserve(str.length() + tabCount * (tabWidth - 1));
    while (it != end)
    {
        char32_t ch = (char32_t)utf8::next(it, end);
        if (ch == U'\t')
            ret.append(tabWidth, ' ');

        utf8::append(ch, ret);
    }

    return ret;
}