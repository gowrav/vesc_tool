/*
    Copyright 2026 - PMa-SynRM integration

    This file is part of VESC Tool.

    VESC Tool is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    VESC Tool is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

#ifndef PAGESYNRM_H
#define PAGESYNRM_H

#include <QWidget>
#include "vescinterface.h"

namespace Ui {
class PageSynrm;
}

class PageSynrm : public QWidget
{
    Q_OBJECT

public:
    explicit PageSynrm(QWidget *parent = nullptr);
    ~PageSynrm();

    VescInterface *vesc() const;
    void setVesc(VescInterface *vesc);
    void reloadParams();

private slots:
    void loadMtpaFromCsv();

private:
    Ui::PageSynrm *ui;
    VescInterface *mVesc;

    bool computeMtpaTable(const QString &dir, QVector<int> &lutMilliAmp,
                          double &imax, QString &err);
};

#endif // PAGESYNRM_H
