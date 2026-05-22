/*
    SPDX-FileCopyrightText: 2004 Duncan Mac-Vicar Prett <duncan@kde.org>
    SPDX-FileCopyrightText: 2004-2005 Olivier Goffart <ogoffart@kde.org>
    SPDX-FileCopyrightText: 2011 Niels Ole Salscheider
    <niels_ole@salscheider-online.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LATEXRENDERER_H
#define LATEXRENDERER_H

#include <QStringList>

class QString;
class QColor;
class QImage;

namespace GuiUtils
{
class LatexRenderer
{
public:
    enum Error { NoError, LatexNotFound, DvipngNotFound, LatexFailed, DvipngFailed, PdfToImageNotFound, PdfToImageFailed };

    LatexRenderer();
    ~LatexRenderer();

    LatexRenderer(const LatexRenderer &) = delete;
    LatexRenderer &operator=(const LatexRenderer &) = delete;

    Error renderLatexInHtml(QString &html, const QColor &textColor, int fontSize, int resolution, QString &latexOutput);
    Error renderLatexToImage(const QString &latexFormula, const QColor &textColor, int fontSize, int resolution, QString &fileName, QString &latexOutput);
    Error renderLatexToPdfAndImage(const QString &latexFormula, const QColor &textColor, int fontSize, int resolution, QString &imageFileName, QString &pdfFileName, QString &latexOutput, double maxWidth = 0.0, const QString &sourcePreamble = QString());
    static bool mightContainLatex(const QString &text);
    static QString defaultSourcePreamble();
    static QString compactErrorMessage(const QString &latexOutput);
    static QImage createErrorImage(const QString &message, int resolution);

private:
    enum class BodyMode { Math, Source };

    Error handleLatex(QString &fileName, QString *pdfFileName, const QString &latexSource, const QColor &textColor, int fontSize, int resolution, QString &latexOutput, BodyMode bodyMode = BodyMode::Math, double maxWidth = 0.0, const QString &sourcePreamble = QString());
    static bool securityCheck(const QString &latexFormula);

    QStringList m_fileList;
};

}

#endif // LATEXRENDERER_H
