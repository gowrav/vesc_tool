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
#include "utility.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QVector>
#include <QDir>
#include <QtMath>
#include <QMap>

// Keep in sync with bldc datatypes.h: 2-D trajectory grid (current x speed), flat ci*NS+si.
#define SYNRM_TRAJ_NI 12
#define SYNRM_TRAJ_NS 10
#define SYNRM_TRAJ_SIZE (SYNRM_TRAJ_NI * SYNRM_TRAJ_NS)

PageSynrm::PageSynrm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PageSynrm)
{
    ui->setupUi(this);
    connect(ui->loadMtpaButton, &QPushButton::clicked, this, &PageSynrm::loadTrajFromCsv);

    // Three id*(|I|) curves: MTPA (speed 0), mid speed, top speed (shows field weakening).
    const char *names[3] = {"id* @ 0 rpm (MTPA)", "id* @ mid speed", "id* @ top speed (FW)"};
    const char *cols[3] = {"plot_graph1", "plot_graph2", "plot_graph3"};
    for (int g = 0; g < 3; g++) {
        ui->mtpaPlot->addGraph();
        ui->mtpaPlot->graph(g)->setName(names[g]);
        ui->mtpaPlot->graph(g)->setPen(QPen(Utility::getAppQColor(cols[g])));
    }
    ui->mtpaPlot->xAxis->setLabel("Current magnitude |I| (A, peak)");
    ui->mtpaPlot->yAxis->setLabel("d-current id* (A)");
    ui->mtpaPlot->legend->setVisible(true);
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

    connect(mVesc->mcConfig(), &ConfigParams::paramChangedDouble, this,
            [this](QObject *, QString name, double) {
        if (name.startsWith("foc_traj")) {
            updateTrajPlot();
        }
    });
    updateTrajPlot();
}

// Plot id* vs |I| for speed = 0, mid, top — reading the 2-D table from the config.
void PageSynrm::updateTrajPlot()
{
    if (!mVesc) {
        return;
    }
    ConfigParams *mc = mVesc->mcConfig();
    double imax = mc->getParamDouble("foc_traj_imax");
    if (imax <= 0.0) {
        imax = 1.0;
    }
    const int sIdx[3] = {0, SYNRM_TRAJ_NS / 2, SYNRM_TRAJ_NS - 1};
    for (int g = 0; g < 3; g++) {
        QVector<double> x(SYNRM_TRAJ_NI), y(SYNRM_TRAJ_NI);
        for (int i = 0; i < SYNRM_TRAJ_NI; i++) {
            x[i] = i * imax / double(SYNRM_TRAJ_NI - 1);
            y[i] = mc->getParamDouble(QString("foc_traj_lut__%1").arg(i * SYNRM_TRAJ_NS + sIdx[g]));
        }
        ui->mtpaPlot->graph(g)->setData(x, y);
    }
    ui->mtpaPlot->rescaleAxes();
    ui->mtpaPlot->replot();
}

// Compute the downsampled 2-D trajectory id*(|I|, speed) (amps, VESC sign) from a LUT.xls CSV
// export with 4 numeric columns: line_current, speed(rpm), id, iq. Current/id are taken as PEAK
// (per the motor owner) and id is negated to VESC's convention.
bool PageSynrm::computeTrajTable(const QString &csv, QVector<double> &lutAmps,
                                 double &imax, double &nmax, QString &err)
{
    QFile f(csv);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err = "Cannot open " + csv;
        return false;
    }
    QTextStream ts(&f);
    // collect (current, speed) -> id; build sorted unique axes
    QMap<double, QMap<double, double>> grid; // cur -> (spd -> id)
    while (!ts.atEnd()) {
        const QStringList t = ts.readLine().split(QRegExp("[,;\\t]"), QString::SkipEmptyParts);
        if (t.size() < 3) {
            continue;
        }
        bool a, b, c;
        double cur = t[0].trimmed().toDouble(&a);
        double spd = t[1].trimmed().toDouble(&b);
        double id  = t[2].trimmed().toDouble(&c);
        if (!a || !b || !c) {
            continue; // header / junk row
        }
        grid[cur][spd] = id;
    }
    if (grid.size() < 2) {
        err = "CSV needs >=2 current levels (cols: current, speed, id, iq).";
        return false;
    }
    QVector<double> cur = grid.keys().toVector();
    QVector<double> spd = grid.first().keys().toVector();
    if (spd.size() < 2) {
        err = "CSV needs >=2 speed levels per current.";
        return false;
    }
    imax = cur.last();
    nmax = spd.last();

    auto sample = [&](double Ipk, double rpm) -> double {
        // bilinear over the (cur, spd) full grid; clamp at edges
        int ci = 0; while (ci < cur.size() - 2 && cur[ci + 1] < Ipk) { ci++; }
        int si = 0; while (si < spd.size() - 2 && spd[si + 1] < rpm) { si++; }
        double tc = (Ipk - cur[ci]) / (cur[ci + 1] - cur[ci]);
        double tspd = (rpm - spd[si]) / (spd[si + 1] - spd[si]);
        tc = qBound(0.0, tc, 1.0);
        tspd = qBound(0.0, tspd, 1.0);
        double g00 = grid[cur[ci]].value(spd[si]);
        double g01 = grid[cur[ci]].value(spd[si + 1]);
        double g10 = grid[cur[ci + 1]].value(spd[si]);
        double g11 = grid[cur[ci + 1]].value(spd[si + 1]);
        double a0 = g00 * (1 - tspd) + g01 * tspd;
        double a1 = g10 * (1 - tspd) + g11 * tspd;
        return a0 * (1 - tc) + a1 * tc;
    };

    lutAmps.resize(SYNRM_TRAJ_SIZE);
    for (int i = 0; i < SYNRM_TRAJ_NI; i++) {
        double Ipk = i * imax / double(SYNRM_TRAJ_NI - 1);
        for (int s = 0; s < SYNRM_TRAJ_NS; s++) {
            double rpm = s * nmax / double(SYNRM_TRAJ_NS - 1);
            lutAmps[i * SYNRM_TRAJ_NS + s] = -sample(Ipk, rpm); // VESC sign (id < 0)
        }
    }
    return true;
}

void PageSynrm::loadTrajFromCsv()
{
    if (!mVesc) {
        return;
    }
    QString csv = QFileDialog::getOpenFileName(this,
            tr("Select LUT.xls export CSV (columns: current, speed, id, iq)"),
            QDir::homePath(), tr("CSV files (*.csv);;All files (*)"));
    if (csv.isEmpty()) {
        return;
    }

    QVector<double> lut;
    double imax = 282.0, nmax = 7500.0;
    QString err;
    if (!computeTrajTable(csv, lut, imax, nmax, err)) {
        QMessageBox::warning(this, tr("Trajectory Load Failed"),
                tr("Could not build the 2-D trajectory table:\n%1").arg(err));
        return;
    }

    ConfigParams *mc = mVesc->mcConfig();
    for (int i = 0; i < lut.size() && i < SYNRM_TRAJ_SIZE; i++) {
        mc->updateParamDouble(QString("foc_traj_lut__%1").arg(i), lut[i], this);
    }
    mc->updateParamDouble("foc_traj_imax", imax, this);
    mc->updateParamDouble("foc_traj_nmax", nmax, this);
    updateTrajPlot();

    double idMax = lut.isEmpty() ? 0.0 : lut.last();
    ui->mtpaInfoLabel->setText(tr("2-D trajectory loaded: %1×%2 grid, |I| 0..%3 A, speed 0..%4 rpm, id* to %5 A. Now Write Motor Configuration ▶")
            .arg(SYNRM_TRAJ_NI).arg(SYNRM_TRAJ_NS).arg(imax, 0, 'f', 0).arg(nmax, 0, 'f', 0).arg(idMax, 0, 'f', 1));

    QMessageBox::information(this, tr("Trajectory Table Loaded"),
            tr("Built the 2-D MTPA + field-weakening trajectory from the CSV and wrote it into the "
               "configuration:\n\n  • grid %1×%2 (current × speed)\n  • current 0 .. %3 A (peak)\n"
               "  • speed 0 .. %4 rpm\n\nSet foc_traj_vnorm to the bus voltage the table was generated for, "
               "then click \"Write Motor Configuration\" to upload it.")
            .arg(SYNRM_TRAJ_NI).arg(SYNRM_TRAJ_NS).arg(imax, 0, 'f', 0).arg(nmax, 0, 'f', 0));
}
