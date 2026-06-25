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

#include "pagesynrm.h"
#include "ui_pagesynrm.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QVector>
#include <QDir>
#include <QtMath>

// Keep in sync with bldc datatypes.h MTPA_LUT_SIZE.
#define SYNRM_MTPA_LUT_SIZE 33

PageSynrm::PageSynrm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PageSynrm)
{
    ui->setupUi(this);
    connect(ui->loadMtpaButton, &QPushButton::clicked, this, &PageSynrm::loadMtpaFromCsv);
}

PageSynrm::~PageSynrm()
{
    delete ui;
}

VescInterface *PageSynrm::vesc() const
{
    return mVesc;
}

void PageSynrm::setVesc(VescInterface *vesc)
{
    mVesc = vesc;

    if (mVesc) {
        reloadParams();
    }
}

void PageSynrm::reloadParams()
{
    ui->paramTab->clearParams();
    ui->paramTab->addParamSubgroup(mVesc->mcConfig(), "synrm", "general");
}

// --- helpers for the MotorXP MTPA computation (port of docs/synrm/tools/mtpa_from_maps.py) ---

static bool loadCsvRow(const QString &path, QVector<double> &out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream ts(&f);
    QString line = ts.readLine();
    out.clear();
    const QStringList toks = line.split(',', QString::SkipEmptyParts);
    for (const QString &tok : toks) {
        out.append(tok.trimmed().toDouble());
    }
    return !out.isEmpty();
}

static bool loadCsvGrid(const QString &path, QVector<QVector<double>> &out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream ts(&f);
    out.clear();
    while (!ts.atEnd()) {
        QString line = ts.readLine();
        if (line.trimmed().isEmpty()) {
            continue;
        }
        QVector<double> row;
        const QStringList toks = line.split(',', QString::SkipEmptyParts);
        for (const QString &tok : toks) {
            row.append(tok.trimmed().toDouble());
        }
        out.append(row);
    }
    return !out.isEmpty();
}

// Compute the id*(|I|) MTPA table (milliamps) from a MotorXP dq export folder.
bool PageSynrm::computeMtpaTable(const QString &dir, QVector<int> &lutMilliAmp,
                                 double &imax, QString &err)
{
    QVector<double> Id;
    QVector<QVector<double>> psid, psiq;
    if (!loadCsvRow(dir + "/Id.csv", Id)) { err = "Cannot read Id.csv"; return false; }
    if (!loadCsvGrid(dir + "/Fluxlinkage_d.csv", psid)) { err = "Cannot read Fluxlinkage_d.csv"; return false; }
    if (!loadCsvGrid(dir + "/Fluxlinkage_q.csv", psiq)) { err = "Cannot read Fluxlinkage_q.csv"; return false; }

    const int N = Id.size();
    if (N < 3 || psid.size() != N || psiq.size() != N ||
            psid[0].size() != N || psiq[0].size() != N) {
        err = QString("Map dimension mismatch (axis %1, maps %2x%3).")
                .arg(N).arg(psid.size()).arg(psid.isEmpty() ? 0 : psid[0].size());
        return false;
    }
    const double imin = Id.first();
    const double istep = (Id.last() - Id.first()) / double(N - 1);
    if (istep <= 0.0) { err = "Bad Id axis."; return false; }

    // Orientation: does psiq vary across rows or columns? (psiq ~ proportional to iq.)
    const int c = N / 2;
    const double rowVarQ = qAbs(psiq[0][c] - psiq[N - 1][c]);
    const double colVarQ = qAbs(psiq[c][0] - psiq[c][N - 1]);
    const bool rowIsIq = rowVarQ > colVarQ;

    auto flux = [&](const QVector<QVector<double>> &m, double idA, double iqA) -> double {
        double fi = ((rowIsIq ? iqA : idA) - imin) / istep; // row coord
        double fj = ((rowIsIq ? idA : iqA) - imin) / istep; // col coord
        int i0 = qBound(0, int(qFloor(fi)), N - 2); double ti = fi - i0;
        int j0 = qBound(0, int(qFloor(fj)), N - 2); double tj = fj - j0;
        return (m[i0][j0] * (1 - tj) + m[i0][j0 + 1] * tj) * (1 - ti) +
               (m[i0 + 1][j0] * (1 - tj) + m[i0 + 1][j0 + 1] * tj) * ti;
    };
    // Pole pairs is only a constant torque scale; it does not change the optimal angle.
    auto torque = [&](double idA, double iqA) -> double {
        return flux(psid, idA, iqA) * iqA - flux(psiq, idA, iqA) * idA;
    };

    const double axisMax = Id.last();

    // MTPA locus: for each |I|, fine-search the angle maximising torque (motoring).
    QVector<double> locI, locId;
    for (int Im = 1; Im <= int(axisMax); Im++) {
        double bestT = -1e18, bestB = 0.0;
        for (int k = 0; k <= 3600; k++) {
            double b = qDegreesToRadians(k / 10.0);
            double idv = Im * qCos(b), iqv = Im * qSin(b);
            if (qAbs(idv) > axisMax || qAbs(iqv) > axisMax) {
                continue;
            }
            double T = torque(idv, iqv);
            if (T > bestT) { bestT = T; bestB = b; }
        }
        locI.append(Im);
        locId.append(Im * qCos(bestB));
    }
    if (locI.isEmpty()) { err = "MTPA search produced no points."; return false; }

    // Resample id*(|I|) onto a uniform axis 0..imax (imax = 70 A or the map range, whichever is smaller).
    imax = qMin(70.0, axisMax);
    lutMilliAmp.resize(SYNRM_MTPA_LUT_SIZE);
    for (int j = 0; j < SYNRM_MTPA_LUT_SIZE; j++) {
        double Ireq = j * imax / double(SYNRM_MTPA_LUT_SIZE - 1);
        double idStar;
        if (Ireq <= locI.first()) {
            idStar = 0.0;
        } else if (Ireq >= locI.last()) {
            idStar = locId.last();
        } else {
            int i1 = 0;
            while (i1 < locI.size() && locI[i1] < Ireq) { i1++; }
            double t = (Ireq - locI[i1 - 1]) / (locI[i1] - locI[i1 - 1]);
            idStar = locId[i1 - 1] + t * (locId[i1] - locId[i1 - 1]);
        }
        lutMilliAmp[j] = int(qRound(idStar * 100.0)); // centiamps (0.01 A) to fit int16
    }
    return true;
}

void PageSynrm::loadMtpaFromCsv()
{
    if (!mVesc) {
        return;
    }

    QString dir = QFileDialog::getExistingDirectory(this,
            tr("Select MotorXP dq-parameter export folder"),
            QDir::homePath());
    if (dir.isEmpty()) {
        return;
    }

    QVector<int> lut;
    double imax = 70.0;
    QString err;
    if (!computeMtpaTable(dir, lut, imax, err)) {
        QMessageBox::warning(this, tr("MTPA Load Failed"),
                tr("Could not compute the MTPA table:\n%1").arg(err));
        return;
    }

    // Write the computed table into the (hidden) mcconf array fields. The user then
    // Writes Motor Configuration to upload it.
    ConfigParams *mc = mVesc->mcConfig();
    for (int i = 0; i < lut.size() && i < SYNRM_MTPA_LUT_SIZE; i++) {
        mc->updateParamInt(QString("foc_mtpa_lut__%1").arg(i), lut[i], this);
    }
    mc->updateParamDouble("foc_mtpa_lut_imax", imax, this);

    int idxRated = int((54.0 / imax) * (SYNRM_MTPA_LUT_SIZE - 1));
    idxRated = qBound(0, idxRated, lut.size() - 1);
    double idAtRated = lut.isEmpty() ? 0.0 : lut[idxRated] / 100.0; // centiamps -> A
    ui->mtpaInfoLabel->setText(tr("MTPA loaded: %1 pts, |I| 0..%2 A (id* ~ %3 A @ ~54 A)")
            .arg(SYNRM_MTPA_LUT_SIZE).arg(imax, 0, 'f', 0).arg(idAtRated, 0, 'f', 1));

    QMessageBox::information(this, tr("MTPA Table Loaded"),
            tr("Computed the MTPA id*(|I|) table from the MotorXP maps and wrote it into the "
               "configuration (%1 points, |I| 0..%2 A).\n\n"
               "Now click \"Write Motor Configuration\" to upload it to the motor.")
            .arg(SYNRM_MTPA_LUT_SIZE).arg(imax, 0, 'f', 0));
}
