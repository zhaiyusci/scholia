/*
    SPDX-FileCopyrightText: 2006 Pino Toscano <toscano.pino@tiscali.it>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _DLGANNOTATIONS_H_
#define _DLGANNOTATIONS_H_

#include <QWidget>

class QLabel;
class QComboBox;
class QLineEdit;
class QTimer;

class DlgAnnotations : public QWidget
{
    Q_OBJECT

public:
    explicit DlgAnnotations(QWidget *parent = nullptr);

private:
    void refreshStemTeXStatus();
    void reloadStemTeXProfiles();
    void syncStemTeXProfileCombo(const QString &profileName);

    QComboBox *m_stemTeXProfileCombo = nullptr;
    QLineEdit *m_stemTeXProfileNameEdit = nullptr;
    QLabel *m_stemTeXStatusLabel = nullptr;
    QTimer *m_stemTeXStatusTimer = nullptr;
};

#endif
