# Scholia Template Note PDF Specification

This document defines the intended PDF representation for Scholia template
notes. A template note is an editable annotation whose displayed text is
computed from document context, such as page number, page count, page label, or
date.

The design follows the LaTeX note model: the saved PDF must contain a normal,
self-contained appearance stream that other PDF readers can display, while
Scholia-specific edit state is stored in one private JSON payload.

## Design Goals

- A template note is a normal PDF FreeText annotation.
- Other PDF readers must be able to display it through the normal appearance
  stream.
- Scholia-specific state should be stored in one private JSON payload.
- The annotation position and size are normal annotation geometry, not template
  metadata.
- The template expression language should follow JavaScript template literal
  interpolation, because Qt already provides a JavaScript engine.
- Template notes are per-annotation objects. A page without a template note
  should not automatically inherit one from another page.

## Annotation Shape

All template notes are stored as `/FreeText` annotations:

```pdf
<<
  /Type /Annot
  /Subtype /FreeText
  /Rect [260 24 360 44]
  /Contents (3 / 24)
  /DA (/Helv 10 Tf 0 0 0 rg)
  /Q 1
  /TemplateNoteData (...JSON string...)
  /AP << /N 21 0 R >>
>>
```

Required fields:

`/Subtype`
: Must be `/FreeText`.

`/Rect`
: The annotation rectangle. This is the editable position and size of the
template note. Moving or resizing a template note updates `/Rect`, not the JSON
payload.

`/Contents`
: The last computed plain-text result. This gives ordinary PDF tools a useful
text value and provides a fallback if Scholia-specific metadata is ignored.

`/DA`
: The default appearance string for the FreeText annotation. It should describe
the font, font size, and text color used for the current appearance.

`/Q`
: Optional FreeText quadding value. If present, it should match
`style.align`: `0` for left, `1` for center, and `2` for right.

`/TemplateNoteData`
: A PDF string containing UTF-8 JSON. This is the single Scholia private field
that identifies and restores a template note.

`/AP /N`
: The normal appearance stream. It must be self-contained and display the last
computed template result without requiring Scholia.

Optional fields:

`/C`, `/IC`
: May be used for the annotation color and interior color according to normal
FreeText writer behavior.

`/BS`
: May be used for the annotation border style according to normal FreeText
writer behavior.

## Identity Rule

A PDF annotation is a Scholia template note when:

- `/Subtype` is `/FreeText`;
- `/TemplateNoteData` is present;
- `/TemplateNoteData` parses as JSON;
- the parsed JSON has `"version": 20260630`;
- the parsed JSON has `"kind": "scholia-template-note"`.

No icon name, visual appearance, `/Contents` text, or appearance-stream content
should be used as the source of identity.

## JSON Payload

`/TemplateNoteData` stores one JSON object.

Minimal schema:

```json
{
  "version": 20260630,
  "kind": "scholia-template-note",
  "template": "${frameNumber} / ${totalFrameNumber}",
  "style": {
    "fontFamily": "Helvetica",
    "fontSizePt": 10,
    "textColor": "#ff000000",
    "align": "center"
  }
}
```

Fields:

`version`
: Required integer. For this specification, the value is `20260630`.

`kind`
: Required string. Must be `"scholia-template-note"`.

`template`
: Required string. A JSON string interpreted as a restricted JavaScript
template literal body. Text outside `${...}` placeholders is copied literally.
Each placeholder contains a JavaScript expression evaluated by Scholia.

`style.fontFamily`
: Optional string. Defaults to a built-in PDF base font such as Helvetica.

`style.fontSizePt`
: Optional positive number in PDF points. Defaults to `10`.

`style.textColor`
: Optional CSS-style ARGB hex string, `#aarrggbb`. Defaults to opaque black.

`style.align`
: Optional string. One of `"left"`, `"center"`, or `"right"`. Defaults to
`"center"`.

`style.fillColor`
: Optional ARGB hex string. Transparent means no fill.

`style.borderColor`
: Optional ARGB hex string. Transparent means no stroke.

`style.borderWidthPt`
: Optional non-negative number in PDF points. Defaults to `0`.

Unknown JSON fields must be preserved when possible. Readers may ignore fields
they do not understand.

The JSON payload must not contain the annotation's page position. Position and
size belong to `/Rect`, just as they do for LaTeX notes and ordinary
annotations.

## Template Expression Language

Template notes use the interpolation syntax of JavaScript template literals.
Conceptually, Scholia evaluates the user string as if it were inside backticks:

```js
`Frame ${frameNumber} of ${totalFrameNumber}`
```

However, the JSON payload stores only the template body. The user does not
write the outer backticks:

```text
Frame ${frameNumber} of ${totalFrameNumber}
${pageLabel || frameNumber}
${formatDate(now, "yyyy-MM-dd")}
```

Text outside `${...}` is literal output. Each `${...}` placeholder contains a
restricted JavaScript expression. The expression result is converted to a
string and inserted into the output.

This is not a general scripting interface. Template evaluation must not be
allowed to modify the document, access files, access the network, or call
arbitrary application APIs.

The expression context contains only Scholia predefined read-only variables and
predefined pure functions. User-defined global variables, user-defined
functions, statements, assignments, imports, file access, network access, and
document mutation are outside this specification.

The predefined names are inspired by Beamer's `\insert...` commands. Beamer
uses inserts such as `\insertframenumber`, `\inserttotalframenumber`,
`\insertshorttitle`, `\insertshortauthor`, `\insertsection`, and
`\insertshortdate` inside headline and footline templates. Scholia exposes the
same concepts as JavaScript-style camelCase values, usually without the
`insert` prefix.

The naming rule is:

- Beamer `\insert...` commands become Scholia camelCase variables.
- The `insert` prefix is removed when the remaining name is clear.
- Frame-oriented names are preferred for slide templates, because this feature
  is meant to support slide-like PDFs.
- Page-oriented names remain available when the user wants literal PDF page
  semantics.

Mapping examples:

| Beamer concept | Beamer name | Scholia template name |
| --- | --- | --- |
| current frame number | `\insertframenumber` | `frameNumber` |
| total frame number | `\inserttotalframenumber` | `totalFrameNumber` |
| current slide number | `\insertslidenumber` | `slideNumber` |
| current overlay number | `\insertoverlaynumber` | `overlayNumber` |
| short title | `\insertshorttitle` | `shortTitle` |
| title | `\inserttitle` | `title` |
| short subtitle | `\insertshortsubtitle` | `shortSubtitle` |
| subtitle | `\insertsubtitle` | `subtitle` |
| short author | `\insertshortauthor` | `shortAuthor` |
| author | `\insertauthor` | `author` |
| short institute | `\insertshortinstitute` | `shortInstitute` |
| institute | `\insertinstitute` | `institute` |
| short date | `\insertshortdate` | `shortDate` |
| date text | `\insertdate` | `dateText` |
| part title | `\insertpart` | `part` |
| short part title | `\insertshortpart` | `shortPart` |
| part number | `\insertpartnumber` | `partNumber` |
| section title | `\insertsection` | `section` |
| short section title | `\insertshortsection` | `shortSection` |
| section number | `\insertsectionnumber` | `sectionNumber` |
| subsection title | `\insertsubsection` | `subsection` |
| short subsection title | `\insertshortsubsection` | `shortSubsection` |
| subsection number | `\insertsubsectionnumber` | `subsectionNumber` |

Required predefined variables:

### Page And Frame Variables

`pageIndex`
: Zero-based page index of the page containing the annotation.

`pageNumber`
: One-based page number of the page containing the annotation.

`pageCount`
: Total number of pages in the document.

`pageLabel`
: PDF page label if available; otherwise an empty string.

`frameIndex`
: Zero-based slide/frame index. In the first implementation this is the same as
`pageIndex`.

`frameNumber`
: One-based slide/frame number. This mirrors Beamer's `\insertframenumber`. In
the first implementation this is the same as `pageNumber`.

`totalFrameNumber`
: Total number of slide frames. This mirrors Beamer's
`\inserttotalframenumber`. In the first implementation this is the same as
`pageCount`.

`slideNumber`
: One-based slide number within the current frame. This mirrors Beamer's
`\insertslidenumber`. Since Scholia PDF pages do not have Beamer overlays in
the first implementation, this value is `1`.

`overlayNumber`
: Current overlay number. This mirrors Beamer's `\insertoverlaynumber`. Since
Scholia PDF pages do not have overlays in the first implementation, this value
is `1`.

### Document Metadata Variables

`title`
: Document or deck title if available; otherwise an empty string.

`shortTitle`
: Short title for headers and footers. This mirrors Beamer's
`\insertshorttitle`. Defaults to `title`.

`subtitle`
: Document or deck subtitle if available; otherwise an empty string.

`shortSubtitle`
: Short subtitle. This mirrors Beamer's `\insertshortsubtitle`. Defaults to
`subtitle`.

`author`
: Author string if available; otherwise an empty string.

`shortAuthor`
: Short author string for headers and footers. This mirrors Beamer's
`\insertshortauthor`. Defaults to `author`.

`institute`
: Institute string if available; otherwise an empty string.

`shortInstitute`
: Short institute string for headers and footers. This mirrors Beamer's
`\insertshortinstitute`. Defaults to `institute`.

`dateText`
: Deck date text if available; otherwise an empty string. This mirrors
Beamer's `\insertdate` as text.

`shortDate`
: Short date text for headers and footers. This mirrors Beamer's
`\insertshortdate`. Defaults to `dateText`.

`documentTitle`
: Alias for `title`.

`fileName`
: Base name of the current document if available; otherwise an empty string.

`now`
: JavaScript `Date` object representing the refresh time.

### Section Variables

`part`
: Current part title if available; otherwise an empty string.

`shortPart`
: Short current part title. This mirrors Beamer's `\insertshortpart`. Defaults
to `part`.

`partNumber`
: Current part number if available; otherwise `0`.

`section`
: Current section title if available; otherwise an empty string.

`shortSection`
: Short current section title for navigation/header/footer use. Defaults to
`section`.

`sectionNumber`
: Current section number if available; otherwise `0`.

`subsection`
: Current subsection title if available; otherwise an empty string.

`shortSubsection`
: Short current subsection title. Defaults to `subsection`.

`subsectionNumber`
: Current subsection number if available; otherwise `0`.

Required predefined functions:

JavaScript already has formatting APIs such as `Intl.DateTimeFormat`,
`Intl.NumberFormat`, `Date.prototype.toLocaleDateString()`, and
`String.prototype.padStart()`. Template notes should not expose the whole
runtime formatting surface directly as the stable document format, because
native locale behavior can vary across platforms and Qt versions. Instead,
Scholia exposes a small stable wrapper API. Implementations may use JavaScript
`Intl`, Qt `QLocale`, or custom code internally, but the behavior documented
here is the compatibility contract.

Formatting rule:

- Template authors should use Scholia wrapper functions for portable output.
- Direct JS formatting methods, such as `now.toLocaleDateString()`, are not
  part of the stable document-format contract.
- Scholia may reject, warn about, or treat unsupported formatting calls as
  implementation-defined.
- Wrapper functions are pure: the same input values, locale, and pattern must
  produce the same output for a given Scholia version.

`formatDate(date, patternOrOptions, locale)`
: Formats a JavaScript `Date` object. If `patternOrOptions` is a string,
Scholia-owned stable patterns are used. The first required pattern is
`"yyyy-MM-dd"`. If `patternOrOptions` is an object, it follows the spirit of
JavaScript `Intl.DateTimeFormat` options, but Scholia owns the supported subset
and must keep existing behavior stable. `locale` is optional.

Required string patterns:

| Pattern | Meaning | Example |
| --- | --- | --- |
| `"yyyy-MM-dd"` | four-digit year, two-digit month, two-digit day | `2026-06-30` |

`formatNumber(value, options, locale)`
: Formats a number. If `options` is omitted, returns a plain decimal string.
If `options` is an object, it follows the spirit of JavaScript
`Intl.NumberFormat` options, but Scholia owns the supported subset and must
keep existing behavior stable. `locale` is optional.

Required options:

| Option | Meaning |
| --- | --- |
| `minimumIntegerDigits` | Minimum integer width, zero padded if needed. |
| `useGrouping` | Whether to use thousands separators. Defaults to `false`. |

`pad(value, width)`
: Converts `value` to a decimal string and left-pads it with zeroes until it has
at least `width` characters.

`roman(value)`
: Converts a positive integer to lower-case Roman numerals.

`upperRoman(value)`
: Converts a positive integer to upper-case Roman numerals.

Examples:

```text
${shortTitle}
${shortAuthor}
${shortDate}
${frameNumber}
${frameNumber} / ${totalFrameNumber}
${pageLabel || frameNumber}
${pad(frameNumber, 2)}
${upperRoman(partNumber)}
${section}
${formatDate(now, "yyyy-MM-dd")}
${formatNumber(totalFrameNumber)}
```

## Appearance Stream Contract

The saved PDF must not depend on Scholia to display the template note. Scholia
may recompute template text while editing, but every save must write a normal
appearance stream for the current computed result.

The normal appearance should be a standard FreeText appearance stream. Since
template note output is plain text, the appearance should draw text with PDF
text operators and vector drawing commands for optional background and border.
It must not rasterize the template text into an image merely to produce the
appearance.

The exact PDF drawing implementation is not part of the JSON schema. Other PDF
readers should treat `/AP /N` as the visual source of truth.

`/Contents` and `/AP /N` must be updated together whenever Scholia refreshes a
template note successfully.

If template evaluation fails, Scholia should keep the previous `/Contents` and
`/AP /N` visible, and expose the error in the Scholia UI rather than saving a
broken appearance.

## Refresh Rules

Template notes should refresh when document context relevant to their computed
text may have changed:

- after creating a template note;
- after editing its template string or style;
- after moving or resizing it, so its appearance matches the new `/Rect`;
- after inserting, deleting, or reordering pages;
- after opening a document containing template notes;
- before saving a document containing template notes.

Refresh is per annotation. Scholia should not scan for one "master" template
note and copy it to other pages. If multiple pages should contain page numbers,
the UI should create one template note per target page.

A full-document refresh is acceptable for page insertion, deletion, reordering,
open, and save because page numbers and page counts can change globally. For
direct edits to a single template note, only that annotation needs to be
refreshed unless the edit explicitly applies to multiple notes.

## Examples

Footer page number:

```json
{
  "version": 20260630,
  "kind": "scholia-template-note",
  "template": "${frameNumber} / ${totalFrameNumber}",
  "style": {
    "fontFamily": "Helvetica",
    "fontSizePt": 9,
    "textColor": "#ff404040",
    "align": "center"
  }
}
```

Page label fallback:

```json
{
  "version": 20260630,
  "kind": "scholia-template-note",
  "template": "${pageLabel || frameNumber}",
  "style": {
    "fontSizePt": 10,
    "textColor": "#ff000000",
    "align": "right"
  }
}
```

Date note:

```json
{
  "version": 20260630,
  "kind": "scholia-template-note",
  "template": "${formatDate(now, \"yyyy-MM-dd\")}",
  "style": {
    "fontSizePt": 9,
    "textColor": "#ff000000",
    "align": "left"
  }
}
```

## Non-Goals

- Template notes do not define a document-wide master slide system.
- Template notes do not automatically appear on pages that do not contain a
  template note annotation.
- Template notes do not require Acrobat JavaScript or PDF document-level
  scripts for display.
- Template notes do not store editable geometry in the JSON payload.
