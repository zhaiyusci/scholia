# Scholia as a LaTeX-Native Slide Editor

Scholia is evolving from a PDF viewer with annotation tools into a
PDF-centered slide editor for research, teaching, and technical presentation.
The defining idea is simple: a page in a PDF can be treated as a slide, and the
objects placed on that slide can keep the typographic quality of LaTeX instead
of falling back to office-style text boxes.

This is not a plan to clone PowerPoint. PowerPoint is built around editable
office objects: text boxes, shapes, images, and theme layouts. Scholia is built
around a different primitive:

- a stable PDF page as the canvas;
- vector LaTeX notes as first-class editable visual objects;
- standard PDF annotations for shapes, callouts, highlights, and handwritten
  explanation;
- page-level editing for inserting, deleting, and reordering slides.

The result should feel closer to making slides inside a PDF document than to
annotating an already-finished PDF.

## Why This Matters

Many scientific and mathematical presentations already want the output quality
of LaTeX, but the editing loop of ordinary slide software. Traditional LaTeX
slide systems such as Beamer give excellent typography, but small visual edits
often require recompiling and rethinking the source structure. PowerPoint gives
direct manipulation, but formula-heavy content, references, spacing, and visual
consistency often degrade quickly.

Scholia can occupy the middle ground:

- It keeps the final artifact as a PDF, which is stable, portable, archivable,
  and easy to distribute.
- It allows blank pages to be inserted directly, so a user can build slides
  inside the document instead of preparing every page elsewhere.
- It renders text and formulas through LaTeX/StemTeX, so slide content has the
  visual texture of a paper, lecture note, or Beamer deck.
- It keeps annotation and slide construction in the same workspace, so a user
  can move between reading, explaining, extending, and presenting without
  changing applications.

## Product Direction

Scholia should become a technical slide editor whose native file is still a PDF.
The editing model should make these workflows natural:

- Start from an existing paper, lecture note, or blank PDF.
- Insert blank pages and use them as new slides.
- Add LaTeX notes for titles, formulas, derivations, definitions, and
  explanations.
- Add callouts, arrows, highlights, freehand marks, and simple shapes as visual
  lecture objects.
- Reorder pages from the thumbnail sidebar like rearranging slides.
- Save the result as a normal PDF that other readers can display.

The important constraint is that the saved PDF must remain self-contained.
Rendered LaTeX notes should be visible through normal PDF appearance streams;
the source and Scholia-specific edit state can be stored as metadata, but the
document must not depend on temporary TeX files or user-local paths.

## Design Principles

1. PDF is the slide deck.

   Blank-page insertion and page reordering are not secondary utilities. They
   are the beginning of a slide-deck editing model. Page editing follows the
   live document model described in `page-editing-annotation-model.md`: moving a
   page means moving the page together with its annotations and edit state, not
   performing an early PDF save/rewrite.

2. LaTeX notes are visual objects, not comments.

   A LaTeX note should feel like a slide text object with high-quality math and
   typography. It may use annotation infrastructure internally, but the user
   should not have to think of it as a popup comment.

3. Direct manipulation matters.

   Users should be able to drag, resize, align, and reorder objects and pages
   without leaving the visual canvas. Source editing is important, but it should
   support the visual workflow rather than replace it.

4. The PDF must stay portable.

   Scholia-specific editability is a bonus layer. Display in other PDF readers
   must rely on standard PDF structures and self-contained appearance streams.

5. Technical content is the center.

   The target audience is not generic office slide making. The priority is
   scientific talks, group meetings, teaching, derivations, paper reading,
   annotated lecture notes, and research explanation.

## Near-Term Priorities

- Make page-level operations reliable and discoverable:
  blank-page insertion, page deletion, thumbnail drag reordering, undo, redo,
  and save.
- Improve the blank-page experience:
  sensible page sizes, optional backgrounds, and simple slide templates.
- Make LaTeX notes feel like slide objects:
  predictable sizing, visual alignment, better editing, copy/paste, and stable
  appearance refresh.
- Keep the StemTeX path fast:
  slide editing only works if LaTeX notes refresh quickly enough to feel
  interactive.
- Improve presentation behavior:
  hide editing handles when presenting, keep annotation tools available, and
  make full-screen explanation smooth.
- Add export-oriented polish:
  preserve a clean PDF, and eventually support separate presentation and
  handout variants if that becomes useful.

## Non-Goals

- Scholia should not become a general office suite.
- It does not need to match every PowerPoint transition, theme, animation, or
  collaboration feature.
- It should not require the user to author a whole Beamer source file when the
  task is visual slide construction.
- It should not sacrifice PDF portability for Scholia-only rendering.

## Summary

The long-term vision is a LaTeX-native slide editor built on PDF. Scholia can
let users read a paper, insert a blank page, write a derivation, add arrows and
callouts, reorder the resulting pages, and present or distribute the same PDF.

That is a different product shape from both PowerPoint and Beamer: direct,
visual, PDF-native, and technically typeset.
