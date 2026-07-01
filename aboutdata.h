/*
    SPDX-FileCopyrightText: 2007 Pino Toscano <pino@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _ABOUTDATA_H_
#define _ABOUTDATA_H_

#include <KAboutData>

#include "core/version.h"

#include <KLocalizedString>

#ifndef SCHOLIA_ABOUT_VERSION_STRING
#define SCHOLIA_ABOUT_VERSION_STRING OKULAR_VERSION_STRING
#endif

inline KAboutData okularAboutData()
{
    const QString brand = QStringLiteral("Scholia");
    KAboutData about(QStringLiteral("scholia"),
                     brand,
                     QStringLiteral(SCHOLIA_ABOUT_VERSION_STRING),
                     i18n("A PDF-centered document viewer and annotation tool based on Okular"),
                     KAboutLicense::GPL_V2,
                     i18n("(C) 2026 Yu Zhai and the %1 contributors\n"
                          "(C) 2002 Wilco Greven, Christophe Devriese\n"
                          "(C) 2004-2005 Enrico Ros\n"
                          "(C) 2005 Piotr Szymanski\n"
                          "(C) 2004-2017 Albert Astals Cid\n"
                          "(C) 2006-2009 Pino Toscano",
                          brand),
                     i18n("%1 is a modified distribution of KDE Okular. It is not an official KDE Okular release and is not affiliated with or endorsed by KDE.", brand),
                     QStringLiteral("https://github.com/zhaiyusci/scholia"));
    about.setDesktopFileName(QStringLiteral("org.jairy.scholia"));

    about.addAuthor(i18n("Yu Zhai"), i18n("%1 maintainer and PDF annotation changes", brand));

    about.addCredit(i18n("KDE Okular Developers"), i18n("Original Okular project"), QString(), QStringLiteral("https://okular.kde.org"));
    about.addAuthor(i18n("Pino Toscano"), i18n("Former maintainer"), QStringLiteral("pino@kde.org"));
    about.addAuthor(i18n("Tobias Koenig"), i18n("Lots of framework work, FictionBook backend and former ODT backend"), QStringLiteral("tokoe@kde.org"));
    about.addAuthor(i18n("Albert Astals Cid"), i18n("Developer"), QStringLiteral("aacid@kde.org"));
    about.addAuthor(i18n("Piotr Szymanski"), i18n("Created Okular from KPDF codebase"), QStringLiteral("djurban@pld-dc.org"));
    about.addAuthor(i18n("Enrico Ros"), i18n("KPDF developer"), QStringLiteral("eros.kde@email.it"));
    about.addCredit(i18n("Eugene Trounev"), i18n("Annotations artwork"), QStringLiteral("eugene.trounev@gmail.com"));
    about.addCredit(i18n("Jiri Baum - NICTA"), i18n("Table selection tool"), QStringLiteral("jiri@baum.com.au"));
    about.addCredit(i18n("Fabio D'Urso"), i18n("Annotation improvements"), QStringLiteral("fabiodurso@hotmail.it"));

    return about;
}

#endif
