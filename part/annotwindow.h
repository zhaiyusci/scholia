/*
    SPDX-FileCopyrightText: 2006 Chu Xiaodong <xiaodongchu@gmail.com>
    SPDX-FileCopyrightText: 2006 Pino Toscano <pino@kde.org>

    Work sponsored by the LiMux project of the city of Munich:
    SPDX-FileCopyrightText: 2017 Klarälvdalens Datakonsult AB a KDAB Group company <info@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _ANNOTWINDOW_H_
#define _ANNOTWINDOW_H_

#include <QColor>
#include <QFrame>
#include <QPalette>
#include <QString>

namespace Okular
{
class Annotation;
class Document;
}

namespace GuiUtils
{
class LatexRenderer;
}

class KTextEdit;
class MovableTitle;
class QCloseEvent;
class QWidget;

#if HAVE_QSCINTILLA
class QsciScintilla;
#endif

class AnnotWindow : public QFrame
{
    Q_OBJECT
public:
    AnnotWindow(QWidget *parent, QRect initialViewportBounds, Okular::Annotation *annot, Okular::Document *document, int page);
    ~AnnotWindow() override;

    void reloadInfo();

    Okular::Annotation *annotation() const;
    int pageNumber() const;

    void updateAnnotation(Okular::Annotation *a);
    void updateViewportBounds(QRect bounds);

private:
    void fixupGeometry();
    static constexpr const QPoint defaultPosition {10, 10};
    static constexpr const QSize defaultSize {300, 300};

    QRect m_viewportBounds;
    MovableTitle *m_title;
    KTextEdit *textEdit = nullptr;
#if HAVE_QSCINTILLA
    QsciScintilla *m_latexSourceEdit = nullptr;
#endif
    QWidget *m_editorWidget = nullptr;
    QColor m_color;
    GuiUtils::LatexRenderer *m_latexRenderer;
    Okular::Annotation *m_annot;
    Okular::Document *m_document;
    int m_page;
    int m_prevCursorPos;
    int m_prevAnchorPos;
    QString m_lastLatexNoteCompileSource;

    void commitWindowText();
    void updateLatexNoteAppearance();
    QString editorPlainText() const;
    void setEditorPlainText(const QString &text);
    int editorCursorPosition() const;
    int editorAnchorPosition() const;
    void setEditorSelection(int anchorPos, int cursorPos);
    bool editorUndoAvailable() const;
    bool editorRedoAvailable() const;
    void clearEditorUndoRedo();
    void setEditorReadOnly(bool readOnly);
    void setEditorPalette(const QPalette &palette);
    QPalette editorPalette() const;

public Q_SLOTS:
    void renderLatex(bool render);

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    void slotOptionBtn();
    void slotWindowTextChanged();
    void slotHandleContentsChangedByUndoRedo(Okular::Annotation *annot, const QString &contents, int cursorPos, int anchorPos);

Q_SIGNALS:
    void containsLatex(bool);
};

#endif
