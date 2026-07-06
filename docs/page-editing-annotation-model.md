# Scholia Page Editing and Annotation Model

This document records the intended model for page-level editing in Scholia,
especially how it interacts with PDF annotations. It exists to prevent a common
implementation mistake: treating page reordering as a PDF save/rewrite problem
instead of a document-model operation.

## Core Model

Scholia edits an open document. The source of truth while the document is open
is the live document model, not the PDF file currently on disk.

The live state includes:

- the ordered list of pages;
- each page's Okular `Page` object and generated page data;
- each page's `Okular::Annotation` objects;
- each annotation's geometry, style, contents, appearance, private metadata,
  and undo state;
- the Poppler native annotation objects that mirror Okular annotations;
- unsaved changes that have not yet been written to the user's PDF file.

Saving the PDF is a persistence step. It serializes the current live document
state to a PDF file. Page-level editing operations must not use "save PDF" as
their conceptual model.

## Annotation Lifecycle

When the user creates an annotation, Scholia does not merely record a sidecar
entry for later saving.

The current code path is:

1. The UI creates an `Okular::Annotation` subtype, such as highlight, stamp,
   free text, or LaTeX note.
2. The UI calls `Document::addPageAnnotation(page, annotation)`.
3. `Document::addPageAnnotation()` transforms the annotation geometry into the
   page's unrotated coordinates and pushes an `AddAnnotationCommand` onto the
   undo stack.
4. `AddAnnotationCommand::redo()` calls
   `DocumentPrivate::performAddPageAnnotation(page, annotation)`.
5. `DocumentPrivate::performAddPageAnnotation()` attaches the annotation to the
   target Okular `Page` first, through `Page::addAnnotation()`.
6. `Page::addAnnotation()` assigns a stable `scholia-{UUID}` unique name if the
   annotation does not already have one.
7. `PagePrivate::addAnnotation()` records the Okular ownership:
   `annotation->d_ptr->m_page` is set to the page private object, the annotation
   is appended to `Page::m_annotations`, and an `AnnotationObjectRect` is added
   for hit testing and painting.
8. `DocumentPrivate::performAddPageAnnotation()` then asks the generator for its
   `AnnotationProxy`.
9. The Poppler backend's `PopplerAnnotationProxy::notifyAddition()` creates the
   matching `Poppler::Annotation`, copies shared properties such as contents,
   unique name, flags, boundary, style, popup, and timestamps, and calls
   `Poppler::Page::addAnnotation()` on the native Poppler page.
10. The Okular annotation stores the native Poppler annotation pointer in
    `Annotation::nativeId()`.

Modification and removal follow the same two-layer pattern: Okular's page model
is updated, and the Poppler native annotation is updated through the annotation
proxy.

Therefore, the live annotation state exists in both:

- Okular's `Page::m_annotations`;
- Poppler's in-memory PDF document and page objects.

The disk PDF is updated only when Scholia explicitly serializes changes, such
as through `Document::saveChanges()` or the user's save operation.

This means an annotation that the user can see in an open document is already a
live document object. It is not merely pending data waiting to become a PDF
annotation later.

When a PDF is opened, the direction is reversed: the Poppler backend reads
native Poppler annotations from each `Poppler::Page`, converts them into
Okular annotations, and calls `Page::addAnnotation()` to attach them to the
Okular page model. Existing native annotations are tracked so later
modifications can be written back through Poppler.

Therefore, any operation that replaces the current `Page` objects by reloading
a rewritten PDF also replaces the annotation model. If the rewritten PDF omits
annotations, has invalid annotation references, or cannot be reloaded into
Okular annotations, the new `Page::m_annotations` lists will be empty even
though the annotations existed correctly before the operation.

## Page Editing Semantics

Page editing operations are operations on the live document model.

For page reordering, the intended user-visible operation is:

> Move this page, together with all of its annotations and live edit state, to
> another position in the document.

The moved unit is the logical page:

- page canvas/content;
- page dimensions and rotation;
- annotations on that page;
- annotation geometry and appearance;
- annotation private metadata, such as LaTeX note or template note data;
- native backend state needed to save the annotations later.

The operation is not:

> Read pages from the PDF file on disk, write a new PDF with a different page
> order, and reload it.

That file-level rewrite model loses the distinction between live state and
persistent state. It can drop annotations that exist only in memory, lose
appearance streams, break native annotation ids, or mismatch annotations with
their pages.

## Persistence Boundary

Page editing must not rely on a temporary PDF rewrite as its primary semantic
model. Saving or exporting a PDF is a persistence boundary, not the way Scholia
should represent ordinary page movement in an open document.

A temporary PDF may be used as an internal persistence mechanism only when the
operation is explicitly crossing that persistence boundary, or as a transitional
implementation detail that is proven to be equivalent to the live model.

The temporary file must not be treated as the semantic source of truth. If page
editing uses a PDF serialization internally, it must preserve:

- all annotations currently visible in Scholia;
- annotation `/Rect`, `/Contents`, `/AP`, style, flags, and private metadata;
- page-to-annotation attachment through `/Annots`;
- annotation-to-page references such as `/P`, when present;
- unique names and identity used by undo commands;
- generated appearances required by other PDF readers.

If this cannot be guaranteed, the operation should stay in the document model
and defer PDF serialization until save time.

For page reordering, the preferred implementation is to reorder the live page
sequence and the corresponding backend page mapping without serializing and
reloading the document. A file-level rewrite may be useful for final PDF
output, but it is too fragile to define the in-memory editing operation.

## Page Reordering Requirements

Page reordering must satisfy these invariants:

- Moving a page must not remove annotations from that page.
- Moving a page must not reset annotation geometry, style, contents, or
  appearance.
- Moving a page must not duplicate annotation unique names.
- Undo and redo must restore both page order and annotation ownership.
- Existing annotation undo commands must either keep valid object references or
  be rebound by stable annotation identity.
- Template notes may need content refresh because page index, page number, or
  page count can change, but their annotation identity and style must remain
  unchanged.
- LaTeX notes should not re-render merely because the page moved unless their
  own source, layout, or appearance-affecting state changed.

## Practical Guidance

Prefer model-level operations for page editing. A page move should be expressed
as a reordering of the live page list plus a corresponding backend page-order
change, not as an early save of the PDF file.

If a file-level helper is used, it must operate on a snapshot produced from the
current live generator state, not on the original backing file unless the
document is known to have no unsaved live changes. The result must then be
loaded in a way that preserves page topology changes and refreshes internal
annotation references.

When debugging page editing bugs, first identify which layer is wrong:

- Okular model: the annotation is no longer in `Page::m_annotations`.
- Poppler native state: the annotation exists in Okular but cannot be saved.
- PDF serialization: the annotation exists before serialization but is missing
  from the generated PDF.
- Reload/rebind: the generated PDF contains the annotation, but Scholia fails to
  rebuild the corresponding Okular annotation after swapping backing files.

These are different failures and should not be fixed with the same patch.

## Relation to Slide Editing

Scholia's slide-editor direction depends on this model. A PDF page is treated
as a slide, and annotations are slide objects. Reordering slides must preserve
the objects on each slide exactly as a presentation editor would.

PDF portability remains a save-time requirement: when the user saves, the PDF
must contain standard annotation structures and self-contained appearance
streams so other PDF readers can display the result.
