# install annotator xml tools description
install(FILES
   tools.xml
   toolsQuick.xml
   drawingtools.xml
   DESTINATION ${KDE_INSTALL_DATADIR}/scholia)

# install annotation tool images
install(FILES
   annotation-advanced-settings.svg
   annotation-arrow.svg
   annotation-callout.svg
   annotation-constrain.svg
   annotation-ellipse.svg
   annotation-favorite.svg
   annotation-font.svg
   annotation-freehand.svg
   annotation-highlight.svg
   annotation-image-note.svg
   annotation-inline-note.svg
   annotation-latex-callout.svg
   annotation-latex-inline-note.svg
   annotation-latex-note.svg
   annotation-line-width.svg
   annotation-opacity.svg
   annotation-pin.svg
   annotation-polygon.svg
   annotation-popup-note.svg
   annotation-quick-tools.svg
   annotation-rectangle.svg
   annotation-squiggle.svg
   annotation-stamp.svg
   annotation-straight-line.svg
   annotation-strikeout.svg
   annotation-typewriter.svg
   annotation-underline.svg
   tool-base-okular.png
   tool-base-okular@2x.png
   tool-highlighter-okular-colorizable.png
   tool-highlighter-okular-colorizable@2x.png
   tool-ink-okular-colorizable.png
   tool-ink-okular-colorizable@2x.png
   tool-note.png
   tool-note-okular-colorizable.png
   tool-note-okular-colorizable@2x.png
   tool-note-inline.png
   tool-note-inline-okular-colorizable.png
   tool-note-inline-okular-colorizable@2x.png
   tool-typewriter-okular-colorizable.png
   tool-typewriter-okular-colorizable@2x.png
   DESTINATION ${KDE_INSTALL_DATADIR}/scholia/pics)
# install annotation page images
install(FILES
   checkmark.png
   circle.png
   comment.png
   cross.png
   help.png
   insert.png
   key.png
   newparagraph.png
   note.png
   paperclip.png
   paragraph.png
   pushpin.png
   rightarrow.png
   rightpointer.png
   star.png
   uparrow.png
   upleftarrow.png
   DESTINATION ${KDE_INSTALL_DATADIR}/scholia/pics)
# install misc images
