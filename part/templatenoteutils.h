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
class StampAnnotation;
}

namespace TemplateNoteUtils
{
QString defaultPageNumberTemplateData();
bool annotationIsTemplateNote(const Okular::Annotation *annotation);
Okular::StampAnnotation *annotationAsTemplateStampAnnotation(Okular::Annotation *annotation);
const Okular::StampAnnotation *annotationAsTemplateStampAnnotation(const Okular::Annotation *annotation);
QString expandTemplate(const Okular::Document *document, int pageIndex, const Okular::StampAnnotation *annotation, QString *errorMessage = nullptr);
}

#endif

