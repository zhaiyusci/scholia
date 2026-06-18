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

namespace GuiUtils
{
enum class LatexRenderWarningType { None, ClippingRisk, LooseLayout, BackendLimitation, CompileFallback };

struct LatexRenderWarning {
    LatexRenderWarningType type = LatexRenderWarningType::None;
    QString message;
    double severity = 0.0;
    bool isValid() const
    {
        return type != LatexRenderWarningType::None && !message.isEmpty();
    }
};

class LatexRenderer
{
public:
    enum Error { NoError, LatexNotFound, DvipngNotFound, LatexFailed, DvipngFailed, PdfToImageNotFound, PdfToImageFailed, MicrotexFailed };

    LatexRenderer();
    ~LatexRenderer();

    LatexRenderer(const LatexRenderer &) = delete;
    LatexRenderer &operator=(const LatexRenderer &) = delete;

    Error renderLatexInHtml(QString &html, const QColor &textColor, int fontSize, int resolution, QString &latexOutput);
    Error renderLatexToImage(const QString &latexFormula, const QColor &textColor, int fontSize, int resolution, QString &fileName, QString &latexOutput);
    Error renderLatexToPdf(const QString &latexFormula, const QColor &textColor, int fontSize, QString &pdfFileName, QString &latexOutput, double maxWidth = 0.0, const QString &sourcePreamble = QString());
    QString lastBackendName() const;
    LatexRenderWarning lastWarning() const;
    QString lastWarningMessage() const;
    static bool mightContainLatex(const QString &text);
    static QString defaultSourcePreamble();
    static QString compactErrorMessage(const QString &latexOutput);
    static void prewarmStemTeX();

private:
    enum class BodyMode { Math, Source };

    Error handleLatex(QString &fileName, QString *pdfFileName, const QString &latexSource, const QColor &textColor, int fontSize, int resolution, QString &latexOutput, BodyMode bodyMode = BodyMode::Math, double maxWidth = 0.0, const QString &sourcePreamble = QString());
    static bool securityCheck(const QString &latexFormula);

    QStringList m_fileList;
    QString m_lastBackendName;
    LatexRenderWarning m_lastWarning;
};

}

#endif // LATEXRENDERER_H
