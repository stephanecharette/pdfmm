/**
 * Copyright (C) 2005 by Dominik Seichter <domseichter@web.de>
 * Copyright (C) 2020 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Library General Public License 2.0 or later.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#ifndef PDF_PAGE_H
#define PDF_PAGE_H

#include "PdfDefines.h"

#include <map>

#include "PdfElement.h"
#include "PdfArray.h"
#include "PdfCanvas.h"
#include "PdfRect.h"
#include "PdfAnnotation.h"
#include "PdfContents.h"
#include "PdfField.h"

namespace PoDoFo {

class PdfDocument;
class PdfDictionary;
class PdfVecObjects;
class PdfInputStream;

typedef std::map<PdfObject*, PdfAnnotation*>  TMapAnnotationDirect;
typedef TMapAnnotationDirect::iterator        TIMapAnnotation;
typedef TMapAnnotationDirect::const_iterator  TCIMapAnnotationDirect;

/** PdfPage is one page in the pdf document.
 *  It is possible to draw on a page using a PdfPainter object.
 *  Every document needs at least one page.
 */
class PODOFO_DOC_API PdfPage final : public PdfElement, public PdfCanvas
{
public:
    /** Create a new PdfPage object.
     *  \param size a PdfRect specifying the size of the page (i.e the /MediaBox key) in PDF units
     *  \param parent add the page to this parent
     */
    PdfPage(PdfDocument& parent, const PdfRect& size);

    /** Create a PdfPage based on an existing PdfObject
     *  \param obj an existing PdfObject
     *  \param listOfParents a list of PdfObjects that are
     *                       parents of this page and can be
     *                       queried for inherited attributes.
     *                       The last object in the list is the
     *                       most direct parent of this page.
     */
    PdfPage(PdfObject& obj, const std::deque<PdfObject*>& listOfParents);

    virtual ~PdfPage();

    PdfRect GetRect() const override;

    bool HasRotation(double& teta) const override;

    // added by Petr P. Petrov 21 Febrary 2010
    /** Set the current page width in PDF Units
     *
     * \returns true if successful, false otherwise
     *
     */
    bool SetPageWidth(int newWidth);

    // added by Petr P. Petrov 21 Febrary 2010
    /** Set the current page height in PDF Units
     *
     * \returns true if successful, false otherwise
     *
     */
    bool SetPageHeight(int newHeight);

    /** Set the mediabox in PDF Units
    *  \param size a PdfRect specifying the mediabox of the page (i.e the /TrimBox key) in PDF units
    */
    void SetMediaBox(const PdfRect& size);

    /** Set the trimbox in PDF Units
     *  \param size a PdfRect specifying the trimbox of the page (i.e the /TrimBox key) in PDF units
     */
    void SetTrimBox(const PdfRect& size);

    /** Page number inside of the document. The  first page
     *  has the number 1, the last page has the number
     *  PdfPagesTree:GetTotalNumberOfPages()
     *
     *  \returns the number of the page inside of the document
     *
     *  \see PdfPagesTree:GetTotalNumberOfPages()
     */
    unsigned GetPageNumber() const;

    /** Creates a PdfRect with the page size as values which is needed to create a PdfPage object
     *  from an enum which are defined for a few standard page sizes.
     *
     *  \param pageSize the page size you want
     *  \param landscape create a landscape pagesize instead of portrait (by exchanging width and height)
     *  \returns a PdfRect object which can be passed to the PdfPage constructor
     */
    static PdfRect CreateStandardPageSize(const PdfPageSize pageSize, bool landscape = false);

    /** Get the current MediaBox (physical page size) in PDF units.
     *  \returns PdfRect the page box
     */
    PdfRect GetMediaBox() const;

    /** Get the current CropBox (visible page size) in PDF units.
     *  \returns PdfRect the page box
     */
    PdfRect GetCropBox() const;

    /** Get the current TrimBox (cut area) in PDF units.
     *  \returns PdfRect the page box
     */
    PdfRect GetTrimBox() const;

    /** Get the current BleedBox (extra area for printing purposes) in PDF units.
     *  \returns PdfRect the page box
     */
    PdfRect GetBleedBox() const;

    /** Get the current ArtBox in PDF units.
     *  \returns PdfRect the page box
     */
    PdfRect GetArtBox() const;

    /** Get the current page rotation (if any), it's a clockwise rotation
     *  \returns int 0, 90, 180 or 270
     */
    int GetRotationRaw() const;

    /** Set the current page rotation.
     *  \param iRotation Rotation to set to the page. Valid value are 0, 90, 180, 270.
     */
    void SetRotationRaw(int rotation);

    /** Get the number of annotations associated with this page
     * \ returns int number of annotations
     */
    unsigned GetAnnotationCount() const;

    /** Create a new annotation to this page.
     *  \param annotType the type of the annotation
     *  \param rect rectangle of the annotation on the page
     *
     *  \returns the annotation object which is owned by the PdfPage
     */
    PdfAnnotation* CreateAnnotation(PdfAnnotationType annotType, const PdfRect& rect);

    /** Get the annotation with index index of the current page.
     *  \param index the index of the annotation to retrieve
     *
     *  \returns a annotation object. The annotation object is owned by the PdfPage.
     *
     *  \see GetAnnotationCount
     */
    PdfAnnotation* GetAnnotation(unsigned index);

    /** Delete the annotation with index index from this page.
     *  \param index the index of the annotation to delete
     *
     *  \see GetAnnotationCount
     */
    void DeleteAnnotation(unsigned index);

    /** Delete the annotation with the given object
     *  \param annotObj the object of an annotation
     *
     *  \see GetAnnotationCount
     */
    void DeleteAnnotation(PdfObject& annotObj);

    /** Get an element from the pages resources dictionary,
     *  using a type (category) and a key.
     *
     *  \param rType the type of resource to fetch (e.g. /Font, or /XObject)
     *  \param rKey the key of the resource
     *
     *  \returns the object of the resource or nullptr if it was not found
     */
    PdfObject* GetFromResources(const PdfName& type, const PdfName& key);

    /** Method for getting a value that can be inherited
     *  Possible names that can be inherited according to
     *  the PDF specification are: Resources, MediaBox, CropBox and Rotate
     *
     *  \returns PdfObject - the result of the key fetching or nullptr
     */
    const PdfObject* GetInheritedKey(const PdfName& name) const;

    /** Set an ICC profile for this page
     *
     *  \param csTag a ColorSpace tag
     *  \param stream an input stream from which the ICC profiles data can be read
     *  \param colorComponents the number of colorcomponents of the ICC profile (expected is 1, 3 or 4 components)
     *  \param alternateColorSpace an alternate colorspace to use if the ICC profile cannot be used
     *
     *  \see PdfPainter::SetDependICCProfileColor()
     */
    void SetICCProfile(const std::string_view& csTag, PdfInputStream& stream, int64_t colorComponents,
        PdfColorSpace alternateColorSpace = PdfColorSpace::DeviceRGB);

    PdfObject& GetContents() override;

    PdfObject& GetResources() override;

private:
    PdfStream& GetStreamForAppending(EPdfStreamAppendFlags flags) override;

    /**
     * Initialize a new page object.
     * m_Contents must be initialized before calling this!
     *
     * \param size page size
     */
    void InitNewPage(const PdfRect& size);

    void EnsureContentsCreated() const;
    void EnsureResourcesCreated() const;

    /** Get the bounds of a specified page box in PDF units.
     * This function is internal, since there are wrappers for all standard boxes
     *  \returns PdfRect the page box
     */
    PdfRect GetPageBox(const std::string_view& inBox) const;

    /** Method for getting a key value that could be inherited (such as the boxes, resources, etc.)
     *  \returns PdfObject - the result of the key fetching or nullptr
     */
    const PdfObject* GetInheritedKeyFromObject(const std::string_view& key, const PdfObject& inObject, int depth = 0) const;

private:
    PdfArray* GetAnnotationsArray() const;
    PdfArray& GetOrCreateAnnotationsArray();

private:
    PdfContents* m_contents;
    PdfObject* m_Resources;
    TMapAnnotationDirect m_mapAnnotations;
};

};

#endif // PDF_PAGE_H