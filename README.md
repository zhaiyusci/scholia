# Okular – Universal Document Viewer

Okular can view and annotate documents of various formats, including PDF, Postscript, Comic Book, and various image formats.
It supports native PDF annotations.

## Changes in this fork

This fork carries local changes on top of upstream Okular for PDF-centered
research-note workflows. The main differences are:

* Interactive LaTeX note annotations:
  * adds a direct annotation action for LaTeX notes;
  * renders LaTeX notes as vector PDF stamp appearances;
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
  * pins `external/poppler` to the local Poppler branch used for vector stamp
    appearance support and LaTeX note metadata round-tripping;
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
