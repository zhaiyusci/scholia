# Okular LaTeX Note PDF Specification

This document defines the intended PDF representation for Okular LaTeX notes.
The goal is to keep the PDF annotation itself standard and self-contained,
while keeping Okular-specific editing state in one private JSON payload.

## Design Goals

- A LaTeX note is a normal PDF stamp annotation.
- Other PDF readers must be able to display it through the normal appearance
  stream.
- Okular-specific state should not be spread across many custom PDF dictionary
  keys.
- The three user-facing variants should be expressed by one explicit `type`
  field, not by many boolean marker fields.

## Annotation Shape

All LaTeX notes are stored as `/Stamp` annotations:

```pdf
<<
  /Type /Annot
  /Subtype /Stamp
  /Rect [100 500 260 550]
  /Contents (editable LaTeX source)
  /LatexNoteData (...JSON string...)
  /AP << /N 21 0 R >>
>>
```

Required fields:

`/Subtype`
: Must be `/Stamp`.

`/Rect`
: The annotation rectangle. For callouts, this rectangle may cover the whole
visual appearance, including the note box and leader line.

`/Contents`
: The editable LaTeX source. This remains outside the JSON so ordinary PDF
tools can still expose the annotation text in a familiar place.

`/LatexNoteData`
: A PDF string containing UTF-8 JSON. This is the single Okular private field
that identifies and restores a LaTeX note.

`/AP /N`
: The normal appearance stream. It must be self-contained and display the
rendered note without requiring TeX or a temporary source PDF file.

Optional fields:

`/Name`
: May be present as a normal stamp name, but it is not part of the LaTeX note
identity. New files should not require `/Name /latex-notes`.

## Identity Rule

A PDF annotation is an Okular LaTeX note when:

- `/Subtype` is `/Stamp`;
- `/LatexNoteData` is present;
- `/LatexNoteData` parses as JSON;
- the parsed JSON has `"version": 20260610`.

`/OkularLatex true` is not required in the new format.

## JSON Payload

`/LatexNoteData` stores one JSON object.

Minimal schema:

```json
{
  "version": 20260610,
  "type": "plain",
  "layout": {
    "widthPt": 0,
    "scale": 1
  },
  "style": {
    "textColor": "#ff000000"
  }
}
```

Fields:

`version`
: Required integer. For this specification, the value is `20260610`.

`type`
: Required string. One of `"plain"`, `"boxed"`, or `"callout"`.

`layout.widthPt`
: Optional number in PDF points. `0` or absence means natural width. A positive
value is the TeX paragraph width used for reflow.

`layout.scale`
: Optional positive number. Defaults to `1`.

`style.textColor`
: Optional CSS-style ARGB hex string, `#aarrggbb`. Defaults to opaque black.

`style.fillColor`
: Optional ARGB hex string. Used by boxed and callout notes. Transparent means
no fill.

`style.borderColor`
: Optional ARGB hex string. Used by boxed and callout notes. Transparent means
no stroke.

`style.borderWidthPt`
: Optional non-negative number in PDF points. Defaults to `0` for plain notes
and `1` for boxed/callout notes.

`callout`
: Required only when `type` is `"callout"`. Stores the editable callout
geometry.

Unknown JSON fields must be preserved when possible. Readers may ignore fields
they do not understand.

## Variant: Plain Note

A plain note is unboxed rendered LaTeX content.

```pdf
<<
  /Type /Annot
  /Subtype /Stamp
  /Rect [100 500 220 530]
  /Contents (E = mc^2)
  /LatexNoteData ({"version":20260610,"type":"plain","layout":{"widthPt":0,"scale":1},"style":{"textColor":"#ff000000"}})
  /AP << /N 21 0 R >>
>>
```

Plain-note JSON:

```json
{
  "version": 20260610,
  "type": "plain",
  "layout": {
    "widthPt": 0,
    "scale": 1
  },
  "style": {
    "textColor": "#ff000000"
  }
}
```

## Variant: Boxed Note

A boxed note corresponds to the current LaTeX Inline Note. The PDF annotation
is still a stamp; the box is part of the appearance stream and its editing
style is stored in JSON.

```pdf
<<
  /Type /Annot
  /Subtype /Stamp
  /Rect [100 500 260 550]
  /Contents (\int_a^b f(x)\,dx)
  /LatexNoteData ({"version":20260610,"type":"boxed","layout":{"widthPt":140,"scale":1},"style":{"textColor":"#ff000000","fillColor":"#ffffff00","borderColor":"#ff000000","borderWidthPt":1}})
  /AP << /N 31 0 R >>
>>
```

Boxed-note JSON:

```json
{
  "version": 20260610,
  "type": "boxed",
  "layout": {
    "widthPt": 140,
    "scale": 1
  },
  "style": {
    "textColor": "#ff000000",
    "fillColor": "#ffffff00",
    "borderColor": "#ff000000",
    "borderWidthPt": 1
  }
}
```

## Variant: Callout

A callout is a boxed LaTeX note plus a three-point leader line. It is one stamp
annotation, not a FreeText callout and not multiple annotations.

The appearance stream must draw:

- the rendered LaTeX content;
- the optional fill and border box;
- the callout leader line and arrow.

Callout JSON:

```json
{
  "version": 20260610,
  "type": "callout",
  "layout": {
    "widthPt": 100,
    "scale": 1
  },
  "style": {
    "textColor": "#ff000000",
    "fillColor": "#ffffffff",
    "borderColor": "#ff000000",
    "borderWidthPt": 1
  },
  "callout": {
    "boxRectPt": [120, 500, 260, 545],
    "pointsNorm": [
      [0.12, 0.42],
      [0.18, 0.48],
      [0.22, 0.48]
    ]
  }
}
```

Callout fields:

`callout.boxRectPt`
: The editable text-box rectangle in PDF page coordinates:
`[x1, y1, x2, y2]`.

`callout.pointsNorm`
: Three points in Okular normalized page coordinates. Point 0 is the arrow tip,
point 1 is the elbow, and point 2 is the connection point on the box.

`callout.pointsPt`
: Optional fallback array of three points in PDF page coordinates. Writers may
include it if useful, but `pointsNorm` is the preferred editing geometry.

## Appearance Stream Contract

The saved PDF must not depend on a temporary file path such as
`latex-notes/*.pdf`. Okular may use such files while rendering, but saving must
embed a self-contained normal appearance stream.

The normal appearance may be an outer Form XObject that draws Okular note
geometry and places an inner Form XObject containing the rendered LaTeX page.
That is an implementation detail; readers should treat `/AP /N` as the source
of visual truth.

Poppler should not need to know the `/LatexNoteData` schema. Okular owns JSON
parsing and passes only generic appearance geometry to the PDF backend when
building `/AP`.

## Layout And Resize Rules

LaTeX notes separate layout from visual scale:

- `layout.widthPt` controls TeX paragraph reflow;
- `layout.scale` controls visual size;
- changing `layout.widthPt` may require re-rendering;
- changing only `layout.scale` should reuse the existing appearance whenever
  possible;
- vertical-only resizing should not invoke TeX or StemTeX.

All dimensions ending in `Pt` are PDF points. Values ending in `Norm` are
normalized page coordinates.
