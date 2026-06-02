# Okular – Universal Document Viewer

Okular can view and annotate documents of various formats, including PDF, Postscript, Comic Book, and various image formats.
It supports native PDF annotations.

## Changes in this fork

This fork carries local changes on top of upstream Okular for PDF-centered
research-note workflows. The main differences are:

* Interactive LaTeX note annotations:
  * adds a direct annotation action for LaTeX notes;
  * renders LaTeX notes as vector PDF FreeText appearances;
  * supports source editing, configurable preamble/executable settings, color,
    width, reflow, warning display, and persistent layout/scale metadata;
  * keeps the intended physical font size, so a 10 pt LaTeX note is stored and
    displayed as a 10 pt PDF annotation rather than being scaled by display DPI.
* MicroTeX fallback rendering:
  * adds `OKULAR_ENABLE_MICROTEX` and `MICROTEX_SRC` CMake options;
  * uses `external/MicroTeX` automatically when present;
  * falls back to MicroTeX when an external TeX executable is unavailable;
  * installs the MicroTeX resource tree needed by the runtime package.
* Local Poppler integration:
  * pins `external/poppler` to the local Poppler branch used for vector
    annotation appearance support and LaTeX note metadata round-tripping;
  * Okular still consumes Poppler through `find_package(Poppler)`, so the
    Poppler submodule should be built and installed into the prefix used for
    Okular configuration.
* Link preview improvements:
  * adds interactive internal-link previews with navigation controls;
  * improves preview behavior during scrolling and viewport updates.
* PDF-only build mode:
  * adds `OKULAR_PDF_ONLY=ON` to build only the PDF generator plugin;
  * this is used by the Windows package to keep the distributed build focused
    on PDF support.
* Annotation performance note:
  * a small FreeText/Callout annotation can become an outlier when its PDF
    appearance references a large embedded CJK font resource. For example, a
    callout containing only ASCII text may still carry a multi-megabyte
    Microsoft YaHei UI `FontFile2` after repeated incremental saves or viewer
    rewrites. Treat those files as malformed/heavy samples when measuring
    normal annotation repaint performance.
* Local component pinning:
  * adds submodules for `external/poppler` and `external/MicroTeX`;
  * see `README.local-components.md` for initialization and local build notes.

## LaTeX Annotation PDF Structure

This fork treats LaTeX as a rendering/editing property on top of standard PDF
annotation subtypes. The PDF annotation subtype must remain a standard value.
It should not be changed to a custom subtype such as `/OkularLatexNote`,
because other PDF readers are only expected to understand standard annotation
subtypes and are most likely to preserve and display a known annotation subtype
with a normal appearance stream.

The canonical LaTeX identity marker is:

```pdf
/OkularLatex true
```

`/OkularLatex true` is the only custom field that should identify an annotation
as LaTeX-backed. Style or layout fields such as `/OkularLatexScale`,
`/OkularLatexLayoutWidth`, colors, borders, and boxes must not be used as the
primary identity test; they are properties of an annotation, not proof that it
is LaTeX-backed.

The PDF subtype and standard intent fields carry the annotation semantics:

- `/Subtype /FreeText` is appropriate for visible text-note semantics.
- `/Subtype /FreeText` with `/IT /FreeTextTypeWriter` is appropriate for
  typewriter-style LaTeX text.
- `/Subtype /FreeText` with `/IT /FreeTextCallout` is appropriate for LaTeX
  callout notes.

`/Contents` stores the editable LaTeX source shown in the popup note. `/AP /N`
stores the rendered appearance that other PDF readers display. The appearance
should contain vector PDF drawing commands copied from the LaTeX renderer output
when possible. A local file under a runtime `latex-notes/*.pdf` directory may be
used only as a current-process bridge while creating or refreshing the
appearance, because Poppler's custom appearance API takes a file path. It must
not be required to reopen, display, or identify a saved LaTeX annotation. Saved
PDFs must not depend on paths such as `~/.local/share/okular/latex-notes/*.pdf`
or `C:/Users/.../okular/latex-notes/*.pdf`.

Inline/typewriter/callout LaTeX annotations should use `FreeText`, not `Stamp`,
because their PDF semantics are visible text notes:

```pdf
<<
  /Type /Annot
  /Subtype /FreeText
  /IT /FreeText
  /Rect [100 500 240 535]

  /Contents (x^2 + y^2)
  /DA (/Helv 10 Tf 0 0 0 rg)

  /OkularLatex true
  /OkularLatexLayoutWidth 120
  /OkularLatexScale 1.0

  /AP <<
    /N 21 0 R
  >>
>>
```

For callout notes, the standard FreeText callout fields remain the source of
the callout semantics:

```pdf
<<
  /Type /Annot
  /Subtype /FreeText
  /IT /FreeTextCallout
  /Rect [80 460 260 540]

  /Contents (\frac{a}{b})
  /DA (/Helv 10 Tf 0 0 0 rg)
  /CL [90 480 130 520 150 520]
  /LE /OpenArrow
  /RD [3 3 3 3]

  /OkularLatex true
  /OkularLatexLayoutWidth 100
  /OkularLatexScale 1.0

  /AP <<
    /N 31 0 R
  >>
>>
```

Boxes, callout leader lines, arrows, fills, borders, and padding are PDF
appearance details. They should be drawn by the outer appearance Form XObject,
not by the LaTeX source. The outer Form XObject may place an inner Form XObject
containing only the rendered LaTeX content:

```pdf
21 0 obj
<<
  /Type /XObject
  /Subtype /Form
  /BBox [0 0 140 35]
  /Resources <<
    /XObject << /Fm0 22 0 R >>
  >>
>>
stream
q
1 1 0.8 rg
0 0 126 35 re
f
0 0 0 RG
1 w
0.5 0.5 125 34 re
S
Q

q
1 0 0 1 3 3 cm
/Fm0 Do
Q
endstream
endobj
```

The read-side recognition rule should therefore be:

```text
annotation subtype is FreeText
and /OkularLatex is true
and /Contents is non-empty
```

## LaTeX Note Rendering And Resize Contract

LaTeX notes separate the editable layout width from the visual scale:

- The layout width controls TeX reflow. Horizontal width-handle changes may
  re-render the note because they change the TeX paragraph width.
- The visual scale controls display size. Vertical-only resizing is a pure
  rescale operation and must not call TeX, MicroTeX, or any TeX Live helper.
  If the runtime appearance PDF path is unavailable, the resize path derives
  the unscaled visual size from the current annotation rectangle and
  `/OkularLatexScale` instead of re-rendering.
- Overflow is a warning state, not a reason to run TeX twice. When system TeX
  renders source-PDF appearances, Okular runs TeX once on a large page
  (`article` with `geometry` `paper=a0paper,margin=0pt` and no page numbers)
  and then runs `pdfcrop --hires --margins "0 0 0 0"` to crop the PDF to the
  actual drawn ink. The first TeX log is kept for `Overfull \hbox` warning
  detection. The old second TeX pass for overflow compensation must not be
  restored.
- MicroTeX does not use `pdfcrop`. It renders directly into a PDF sized from
  its measured vector bounds and writes the needed crop box itself. Therefore a
  MicroTeX-only runtime is expected to work without any system TeX Live tools.

When debugging renderer selection, enable `org.kde.okular.ui.debug`. Every TeX
or TeX-like render operation writes a line to
`okular-tex-debug.log` under the platform local application data directory. On
Windows this is normally:

```text
%LOCALAPPDATA%\okular\okular-tex-debug.log
```

Expected operations are:

- `system-latex` for a system `xelatex` or `lualatex` compile.
- `texlive-pdfcrop` for the system-TeX PDF crop post-process.
- `microtex-render` for MicroTeX rendering or Auto fallback when system TeX is
  unavailable.

## Image Note PDF Structure

Image notes use standard PDF stamp annotations. The semantic PDF shape is:

```pdf
<<
  /Type /Annot
  /Subtype /Stamp
  /Name /Image
  /Rect [100 500 220 580]
  /AP << /N 41 0 R >>
>>
```

`/AP /N` contains the rendered image appearance, so conforming PDF viewers can
display the note without access to the original image file. The original file
path is an Okular editing/tool source only. In Okular's tool XML it is stored as
`imagePath`, while the stamp `icon` remains a normal PDF stamp name such as
`Image`. The local path must not be written as the PDF stamp `/Name`.

Poppler should expose this as generic stamp appearance support, not as an
Okular-specific image-note feature. Okular decides when a local file should
become a stamp appearance and passes the resulting image through Poppler's
stamp appearance API.

For the Windows packaging workflow used by this fork, see the scripts and notes
under `windows-build`.

### Downloads

For download and installation instructions, see https://okular.kde.org/download.php

### User manual

https://docs.kde.org/?application=okular&branch=stable5

### Bugs

https://bugs.kde.org/buglist.cgi?product=okular

Please report bugs on Bugzilla (https://bugs.kde.org/enter_bug.cgi?product=okular), and not on our GitLab instance (https://invent.kde.org).

### Mailing list

https://mail.kde.org/mailman/listinfo/okular-devel

### Source code

https://invent.kde.org/graphics/okular.git

The Okular repository contains the source code for:
 * the `okular` desktop application (the “shell”),
 * the `okularpart` KParts plugin,
 * the `okularkirigami` mobile application,
 * several `okularGenerator_xyz` plugins, which provide backends for different document types.

### Apidox

https://api.kde.org/okular/html/index.html

## Contributing

Okular uses the merge request workflow.
Merge requests are required to run pre-commit CI jobs; please don’t push to the master branch directly.
See https://community.kde.org/Infrastructure/GitLab for an introduction.

### Build instructions

Okular can be built like many other applications developed by KDE.
See https://community.kde.org/Get_Involved/development for an introduction.

If your build environment is set up correctly, you can also build Okular using CMake:

```bash
git clone https://invent.kde.org/graphics/okular.git
cd okular
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/path/to/your/install/dir ..
make
make install
```

Okular also builds tests in the build tree. To run them, you have to run `make install` first.

If you install Okular in a different path than your system install directory it is possible that you need to run

```bash
source prefix.sh
```

so that the correct Okular instance and libraries are picked up.
Afterwards one can run `okular` inside the shell instance.
The source command is also required to run the tests manually.

As stated above, Okular has various build targets.
Two of them are executables.
You can choose which executable to build by passing a flag to CMake:

```bash
cmake -DCMAKE_INSTALL_PREFIX=/path/to/your/install/dir -DOKULAR_UI=desktop ..
```
Available options are `desktop`, `mobile`, and `both`.

### clang-format

The Okular project uses clang-format to enforce source code formatting.
See [README.clang_format](https://invent.kde.org/graphics/okular/-/blob/master/README.clang-format) for more information.
