/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEMPLATENOTEUTILS_H
#define TEMPLATENOTEUTILS_H

#include <QString>

namespace Okular
{
class Annotation;
class Document;
class TextAnnotation;
}

namespace TemplateNoteUtils
{
QString defaultPageNumberTemplateData();
QString templateDataWithTemplate(const QString &templateData, const QString &templateText);
bool annotationIsTemplateNote(const Okular::Annotation *annotation);
Okular::TextAnnotation *annotationAsTemplateTextAnnotation(Okular::Annotation *annotation);
const Okular::TextAnnotation *annotationAsTemplateTextAnnotation(const Okular::Annotation *annotation);
QString expandTemplate(const Okular::Document *document, int pageIndex, const Okular::TextAnnotation *annotation, QString *errorMessage = nullptr);
}

#endif
