# Scholia

Scholia is a PDF-centered document and slide editor derived from KDE Okular.
It is focused on technical reading, annotation, LaTeX-backed notes, and
PDF-native slide construction.

Scholia is not an official KDE Okular release and is not affiliated with or
endorsed by KDE. It inherits a large amount of Okular's document-viewing
infrastructure, but the product direction in this repository is
Scholia-specific.

## What Scholia Is For

Scholia treats a PDF as an editable technical workspace:

- read and annotate PDFs;
- insert, delete, duplicate, and reorder pages;
- use pages as slide canvases;
- add LaTeX-rendered notes as editable visual objects;
- add template notes such as auto-updating page numbers;
- save a normal PDF that other readers can display through standard annotation
  appearance streams.

The long-term direction is a LaTeX-native slide editor built on PDF: direct
visual editing like a slide tool, with the typography and formula quality of a
LaTeX workflow.

## Main Features In This Fork

- **LaTeX notes**
  - Stored as standard PDF stamp annotations.
  - Editable source is preserved in the PDF.
  - Rendered appearance is written as a self-contained PDF appearance stream.
  - Other PDF readers can display the note without StemTeX or temporary files.

- **StemTeX renderer integration**
  - Integrates [StemTeX](https://github.com/zhaiyusci/stemtex) as a fast XeTeX
    renderer for LaTeX notes.
  - Uses rendering profiles for technical text, formulas, chemistry, physics,
    and CJK content.
  - Users may select an external TeX Live tree for packages and fonts.
  - Runtime state, fontconfig files, caches, traces, and rendered-note outputs
    are written under a per-user temporary StemTeX directory, not under the
    installation directory.

- **Template notes**
  - Stored as standard PDF FreeText annotations.
  - Scholia-specific template metadata lives in one JSON payload.
  - The visible text and normal appearance are refreshed from document context,
    such as page number, page count, page label, title, author, or date.

- **Page editing**
  - Blank page insertion, page deletion, page duplication, and thumbnail
    reordering are part of the slide-editing model.
  - Page moves preserve annotations and live edit state.
  - Saved PDFs keep standard annotation structures and normal appearance
    streams.

- **Internal-link previews**
  - Internal PDF links can show a preview instead of immediately navigating.

## Downloads

Releases are published on GitHub:

https://github.com/zhaiyusci/scholia/releases

## Documentation Map

- `docs/latex-native-slides-vision.md`
  - Product direction for Scholia as a PDF-centered, LaTeX-native slide editor.
- `docs/latex-note-pdf-spec.md`
  - PDF representation and rendering contract for LaTeX notes.
- `docs/template-note-pdf-spec.md`
  - PDF representation, JSON payload, expression language, predefined
    variables, and refresh rules for template notes.
- `docs/page-editing-annotation-model.md`
  - Live document model for page editing and annotation preservation.
- `README.local-components.md`
  - Local component and submodule notes.
- `README.local-linux-build.md`
  - Local Linux build notes, when needed.

The main README is intentionally a project entry point. Detailed PDF schemas
and implementation contracts belong in `docs/`.

## Source Layout

- `shell/`
  - Scholia desktop application shell.
- `part/`
  - Main viewer part, annotation UI, LaTeX note logic, template note logic, and
    page-view interactions.
- `core/`
  - Document model and shared Okular core code.
- `generators/poppler/`
  - PDF backend integration.
- `external/poppler/`
  - Local Poppler branch used by Scholia.
- `docs/`
  - Scholia-specific design specs.

## PDF Portability Contract

Scholia-specific editability is stored as private metadata on standard PDF
annotations. Display in other PDF readers must rely on standard annotation
subtypes and self-contained `/AP /N` appearance streams.

This means:

- LaTeX notes are standard `/Stamp` annotations with LaTeX metadata and a normal
  appearance stream.
- Template notes are standard `/FreeText` annotations with template metadata
  and a normal appearance stream.
- Image notes are standard `/Stamp` annotations with embedded image appearance.
- Page editing must preserve annotation identity, geometry, style, contents,
  appearance, and private metadata.

When in doubt, the saved PDF should be readable in Scholia and displayable in
Adobe Acrobat or another standards-oriented PDF reader.

## Reporting Issues

Report Scholia-specific issues in this repository:

https://github.com/zhaiyusci/scholia/issues

Do not report Scholia fork bugs to KDE Okular unless the problem has been
confirmed in upstream Okular without Scholia-specific changes.

## Upstream

Scholia is derived from KDE Okular:

https://invent.kde.org/graphics/okular

Okular's original license and copyright notices remain in the inherited source
files. Scholia-specific changes in this repository follow the same licensing
terms as the surrounding Okular code unless a file states otherwise.
