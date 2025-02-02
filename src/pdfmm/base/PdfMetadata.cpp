/**
 * Copyright (C) 2021 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Lesser General Public License 2.1.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#include <pdfmm/private/PdfDeclarationsPrivate.h>
#include "PdfMetadata.h"

#include "PdfDocument.h"
#include "PdfDictionary.h"
#include <pdfmm/private/XMPUtils.h>

using namespace std;
using namespace mm;

PdfMetadata::PdfMetadata(PdfDocument& doc)
    : m_doc(&doc), m_initialized(false), m_xmpSynced(false)
{
}

void PdfMetadata::SetTitle(nullable<const PdfString&> title, bool syncXMP)
{
    ensureInitialized();
    if (m_metadata.Title == title)
        return;

    m_doc->GetInfo().SetTitle(title);
    if (title == nullptr)
        m_metadata.Title = nullptr;
    else
        m_metadata.Title = *title;
    if (syncXMP)
        syncXMPMetadata(false);
    else
        m_xmpSynced = false;
}

const nullable<PdfString>& PdfMetadata::GetTitle() const
{
    const_cast<PdfMetadata&>(*this).ensureInitialized();
    return m_metadata.Title;
}

void PdfMetadata::SetAuthor(nullable<const PdfString&> author, bool syncXMP)
{
    if (m_metadata.Author == author)
        return;

    m_doc->GetInfo().SetAuthor(author);
    if (author == nullptr)
        m_metadata.Author = nullptr;
    else
        m_metadata.Author = *author;
    if (syncXMP)
        syncXMPMetadata(false);
    else
        m_xmpSynced = false;
}

const nullable<PdfString>& PdfMetadata::GetAuthor() const
{
    const_cast<PdfMetadata&>(*this).ensureInitialized();
    return m_metadata.Author;
}

void PdfMetadata::SetSubject(nullable<const PdfString&> subject, bool syncXMP)
{
    ensureInitialized();
    if (m_metadata.Subject == subject)
        return;

    m_doc->GetInfo().SetSubject(subject);
    if (subject == nullptr)
        m_metadata.Subject = nullptr;
    else
        m_metadata.Subject = *subject;
    if (syncXMP)
        syncXMPMetadata(false);
    else
        m_xmpSynced = false;
}

const nullable<PdfString>& PdfMetadata::GetSubject() const
{
    const_cast<PdfMetadata&>(*this).ensureInitialized();
    return m_metadata.Subject;
}

const nullable<PdfString>& PdfMetadata::GetKeywordsRaw() const
{
    const_cast<PdfMetadata&>(*this).ensureInitialized();
    return m_metadata.Keywords;
}

void PdfMetadata::SetKeywords(vector<string> keywords, bool syncXMP)
{
    if (keywords.size() == 0)
        setKeywords(nullptr, syncXMP);
    else
        setKeywords(PdfString(mm::ToPdfKeywordsString(keywords)), syncXMP);
}

void PdfMetadata::setKeywords(nullable<const PdfString&> keywords, bool syncXMP)
{
    ensureInitialized();
    if (m_metadata.Keywords == keywords)
        return;

    m_doc->GetInfo().SetKeywords(keywords);
    if (keywords == nullptr)
        m_metadata.Keywords = nullptr;
    else
        m_metadata.Keywords = *keywords;
    if (syncXMP)
        syncXMPMetadata(false);
    else
        m_xmpSynced = false;
}

vector<string> PdfMetadata::GetKeywords() const
{
    const_cast<PdfMetadata&>(*this).ensureInitialized();
    if (m_metadata.Keywords == nullptr)
        return vector<string>();
    else
        return mm::ToPdfKeywordsList(*m_metadata.Keywords);
}

void PdfMetadata::SetCreator(nullable<const PdfString&> creator, bool syncXMP)
{
    ensureInitialized();
    if (m_metadata.Creator == creator)
        return;

    m_doc->GetInfo().SetCreator(creator);
    if (creator == nullptr)
        m_metadata.Creator = nullptr;
    else
        m_metadata.Creator = *creator;
    if (syncXMP)
        syncXMPMetadata(false);
    else
        m_xmpSynced = false;
}

const nullable<PdfString>& PdfMetadata::GetCreator() const
{
    const_cast<PdfMetadata&>(*this).ensureInitialized();
    return m_metadata.Creator;
}

void PdfMetadata::SetProducer(nullable<const PdfString&> producer, bool syncXMP)
{
    ensureInitialized();
    if (m_metadata.Producer == producer)
        return;

    m_doc->GetInfo().SetProducer(producer);
    if (producer == nullptr)
        m_metadata.Producer = nullptr;
    else
        m_metadata.Producer = *producer;
    if (syncXMP)
        syncXMPMetadata(false);
    else
        m_xmpSynced = false;
}

const nullable<PdfString>& PdfMetadata::GetProducer() const
{
    const_cast<PdfMetadata&>(*this).ensureInitialized();
    return m_metadata.Producer;
}

void PdfMetadata::SetCreationDate(nullable<PdfDate> date, bool syncXMP)
{
    ensureInitialized();
    if (m_metadata.CreationDate == date)
        return;

    m_doc->GetInfo().SetCreationDate(date);
    m_metadata.CreationDate = date;
    if (syncXMP)
        syncXMPMetadata(false);
    else
        m_xmpSynced = false;
}

const nullable<PdfDate>& PdfMetadata::GetCreationDate() const
{
    const_cast<PdfMetadata&>(*this).ensureInitialized();
    return m_metadata.CreationDate;
}

void PdfMetadata::SetModifyDate(nullable<PdfDate> date, bool syncXMP)
{
    ensureInitialized();
    if (m_metadata.ModDate == date)
        return;

    m_doc->GetInfo().SetModDate(date);
    m_metadata.ModDate = date;
    if (syncXMP)
        syncXMPMetadata(false);
    else
        m_xmpSynced = false;
}

const nullable<PdfDate>& PdfMetadata::GetModifyDate() const
{
    const_cast<PdfMetadata&>(*this).ensureInitialized();
    return m_metadata.ModDate;
}

void PdfMetadata::SetTrapped(const PdfName& trapped)
{
    m_doc->GetInfo().SetTrapped(trapped);
}

const PdfName& PdfMetadata::GetTrapped() const
{
    return m_doc->GetInfo().GetTrapped();
}

void PdfMetadata::SetPdfVersion(PdfVersion version)
{
    m_doc->SetPdfVersion(version);
}

PdfVersion PdfMetadata::GetPdfVersion() const
{
    return m_doc->GetPdfVersion();
}

PdfALevel PdfMetadata::GetPdfALevel() const
{
    const_cast<PdfMetadata&>(*this).ensureInitialized();
    return m_metadata.PdfaLevel;
}

void PdfMetadata::SetPdfALevel(PdfALevel level, bool syncXMP)
{
    ensureInitialized();
    if (m_metadata.PdfaLevel == level)
        return;

    if (level != PdfALevel::Unknown)
    {
        // The PDF/A level can be set only in XMP,
        // metadata let's ensure it exists
        EnsureXMPMetadata();
    }

    m_metadata.PdfaLevel = level;
    if (syncXMP)
        syncXMPMetadata(false);
    else
        m_xmpSynced = false;
}

void PdfMetadata::SyncXMPMetadata(bool forceCreationXMP)
{
    ensureInitialized();
    if (m_xmpSynced)
        return;

    syncXMPMetadata(forceCreationXMP);
}

unique_ptr<PdfXMPPacket> PdfMetadata::TakeXMPPacket()
{
    if (m_packet == nullptr)
        return nullptr;

    if (!m_xmpSynced)
    {
        // If the XMP packet is not synced, do it now
        mm::UpdateOrCreateXMPMetadata(m_packet, m_metadata);
    }

    invalidate();
    return std::move(m_packet);
}

void PdfMetadata::EnsureXMPMetadata()
{
    if (m_packet == nullptr)
        mm::UpdateOrCreateXMPMetadata(m_packet, m_metadata);

    // NOTE: Found dates without prefix "D:" that
    // won't validate in veraPDF. Let's reset them
    m_doc->GetInfo().SetCreationDate(m_metadata.CreationDate);
    m_doc->GetInfo().SetModDate(m_metadata.ModDate);
}

void PdfMetadata::Invalidate()
{
    invalidate();
    m_packet = nullptr;
}

void PdfMetadata::invalidate()
{
    m_initialized = false;
    m_xmpSynced = false;
    m_metadata = { };
}

void PdfMetadata::ensureInitialized()
{
    if (m_initialized)
        return;

    auto& info = m_doc->GetInfo();
    m_metadata.Title = info.GetTitle();
    m_metadata.Author = info.GetAuthor();
    m_metadata.Subject = info.GetSubject();
    m_metadata.Keywords = info.GetKeywords();
    m_metadata.Creator = info.GetCreator();
    m_metadata.Producer = info.GetProducer();
    m_metadata.CreationDate = info.GetCreationDate();
    m_metadata.ModDate = info.GetModDate();
    auto metadataValue = m_doc->GetCatalog().GetMetadataStreamValue();
    auto xmpMetadata = mm::GetXMPMetadata(metadataValue, m_packet);
    if (m_packet != nullptr)
    {
        if (m_metadata.Title == nullptr)
            m_metadata.Title = xmpMetadata.Title;
        if (m_metadata.Author == nullptr)
            m_metadata.Author = xmpMetadata.Author;
        if (m_metadata.Subject == nullptr)
            m_metadata.Subject = xmpMetadata.Subject;
        if (m_metadata.Keywords == nullptr)
            m_metadata.Keywords = xmpMetadata.Keywords;
        if (m_metadata.Creator == nullptr)
            m_metadata.Creator = xmpMetadata.Creator;
        if (m_metadata.Producer == nullptr)
            m_metadata.Producer = xmpMetadata.Producer;
        if (m_metadata.CreationDate == nullptr)
            m_metadata.CreationDate = xmpMetadata.CreationDate;
        if (m_metadata.ModDate == nullptr)
            m_metadata.ModDate = xmpMetadata.ModDate;
        m_metadata.PdfaLevel = xmpMetadata.PdfaLevel;
        m_xmpSynced = true;
    }

    m_initialized = true;
}

void PdfMetadata::syncXMPMetadata(bool forceCreationXMP)
{
    if (m_packet == nullptr && !forceCreationXMP)
        return;

    mm::UpdateOrCreateXMPMetadata(m_packet, m_metadata);
    m_doc->GetCatalog().SetMetadataStreamValue(m_packet->ToString());
    m_xmpSynced = true;
}
