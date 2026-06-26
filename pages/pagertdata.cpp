/*
    Copyright 2016 - 2022 Benjamin Vedder	benjamin@vedder.se

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

#include "pagertdata.h"
#include "ui_pagertdata.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QFrame>
#include <cmath>
#include "utility.h"

#include <QXmlStreamWriter>
#include <QXmlStreamReader>

PageRtData::PageRtData(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PageRtData)
{
    ui->setupUi(this);
    layout()->setContentsMargins(0, 0, 0, 0);
    mVesc = nullptr;

    ui->rescaleButton->setIcon(Utility::getIcon("icons/expand_off.png"));

    QIcon mycon = QIcon(Utility::getIcon("icons/expand_off.png"));
    mycon.addPixmap(Utility::getIcon("icons/expand_on.png"), QIcon::Normal, QIcon::On);
    mycon.addPixmap(Utility::getIcon("icons/expand_off.png"), QIcon::Normal, QIcon::Off);
    ui->zoomHButton->setIcon(mycon);

    mycon = QIcon(Utility::getIcon("icons/expand_v_off.png"));
    mycon.addPixmap(Utility::getIcon("icons/expand_v_on.png"), QIcon::Normal, QIcon::On);
    mycon.addPixmap(Utility::getIcon("icons/expand_v_off.png"), QIcon::Normal, QIcon::Off);
    ui->zoomVButton->setIcon(mycon);

    mycon = QIcon(Utility::getIcon("icons/size_off.png"));
    mycon.addPixmap(Utility::getIcon("icons/size_on.png"), QIcon::Normal, QIcon::On);
    mycon.addPixmap(Utility::getIcon("icons/size_off.png"), QIcon::Normal, QIcon::Off);
    ui->autoscaleButton->setIcon(mycon);

    mycon = QIcon(Utility::getIcon("icons/rt_off.png"));
    mycon.addPixmap(Utility::getIcon("icons/rt_on.png"), QIcon::Normal, QIcon::On);
    mycon.addPixmap(Utility::getIcon("icons/rt_off.png"), QIcon::Normal, QIcon::Off);
    ui->logRtButton->setIcon(mycon);

    mTimer = new QTimer(this);
    mTimer->start(20);

    mSecondCounter = 0.0;
    mLastUpdateTime = 0;

    mUpdateValPlot = false;
    mUpdatePosPlot = false;

    QCustomPlot* allPlots[] =
                {ui->currentPlot, ui->tempPlot, ui->focPlot,
                ui->posPlot, ui->rpmPlot};
    for(int j = 0;j < 5; j++) {
        Utility::setPlotColors(allPlots[j]);
        allPlots[j]->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    }

    // Current and duty
    int graphIndex = 0;
    ui->currentPlot->addGraph();

    ui->currentPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph1")));
    ui->currentPlot->graph(graphIndex)->setName("Current in");
    graphIndex++;

    ui->currentPlot->addGraph();
    ui->currentPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph2")));
    ui->currentPlot->graph(graphIndex)->setName("Current motor");
    graphIndex++;

    ui->currentPlot->addGraph(ui->currentPlot->xAxis, ui->currentPlot->yAxis2);
    ui->currentPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph3")));
    ui->currentPlot->graph(graphIndex)->setName("Duty cycle");
    graphIndex++;

    // RPM
    graphIndex = 0;
    ui->rpmPlot->addGraph();
    ui->rpmPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph1")));
    ui->rpmPlot->graph(graphIndex)->setName("ERPM");
    graphIndex++;

    // FOC
    graphIndex = 0;
    ui->focPlot->addGraph();
    ui->focPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph1")));
    ui->focPlot->graph(graphIndex)->setName("D Current");
    graphIndex++;

    ui->focPlot->addGraph();
    ui->focPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph2")));
    ui->focPlot->graph(graphIndex)->setName("Q Current");
    graphIndex++;

    ui->focPlot->addGraph(ui->focPlot->xAxis, ui->focPlot->yAxis2);
    ui->focPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph3")));
    ui->focPlot->graph(graphIndex)->setName("D Voltage");
    graphIndex++;

    ui->focPlot->addGraph(ui->focPlot->xAxis, ui->focPlot->yAxis2);
    ui->focPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph4")));
    ui->focPlot->graph(graphIndex)->setName("Q Voltage");
    graphIndex++;

    QFont legendFont = font();
    legendFont.setPointSize(9);

    ui->currentPlot->legend->setVisible(true);
    ui->currentPlot->legend->setFont(legendFont);
    ui->currentPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignRight|Qt::AlignBottom);
    ui->currentPlot->xAxis->setLabel("Seconds (s)");
    ui->currentPlot->yAxis->setLabel("Ampere (A)");
    ui->currentPlot->yAxis2->setLabel("Duty Cycle");

    ui->tempPlot->legend->setVisible(true);
    ui->tempPlot->legend->setFont(legendFont);
    ui->tempPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignRight|Qt::AlignBottom);
    ui->tempPlot->xAxis->setLabel("Seconds (s)");
    ui->tempPlot->yAxis->setLabel("Temperature MOSFET (\u00B0C)");
    ui->tempPlot->yAxis2->setLabel("Temperature Motor (\u00B0C)");

    ui->rpmPlot->legend->setVisible(true);
    ui->rpmPlot->legend->setFont(legendFont);
    ui->rpmPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignRight|Qt::AlignBottom);
    ui->rpmPlot->xAxis->setLabel("Seconds (s)");
    ui->rpmPlot->yAxis->setLabel("ERPM");

    ui->focPlot->legend->setVisible(true);
    ui->focPlot->legend->setFont(legendFont);
    ui->focPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignRight|Qt::AlignBottom);
    ui->focPlot->xAxis->setLabel("Seconds (s)");
    ui->focPlot->yAxis->setLabel("Current");
    ui->focPlot->yAxis2->setLabel("Voltage");

    ui->currentPlot->yAxis->setRange(-20, 130);
    ui->currentPlot->yAxis2->setRange(-0.2, 1.3);
    ui->currentPlot->yAxis2->setVisible(true);
    ui->tempPlot->yAxis->setRange(0, 120);
    ui->tempPlot->yAxis2->setRange(0, 120);
    ui->tempPlot->yAxis2->setVisible(true);
    ui->rpmPlot->yAxis->setRange(0, 120);
    ui->focPlot->yAxis->setRange(0, 120);
    ui->focPlot->yAxis2->setRange(0, 120);
    ui->focPlot->yAxis2->setVisible(true);

    ui->posPlot->addGraph();
    ui->posPlot->graph(0)->setPen(QPen(Utility::getAppQColor("plot_graph1")));
    ui->posPlot->graph(0)->setName("Position");

    ui->posPlot->legend->setVisible(true);
    ui->posPlot->legend->setFont(legendFont);
    ui->posPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignRight|Qt::AlignBottom);
    ui->posPlot->xAxis->setLabel("Sample");
    ui->posPlot->yAxis->setLabel("Degrees");

    mTrajColorMap = nullptr;
    mUpdateTrajPlot = false;
    mTrajHasTable = false;
    mTrajImax = mTrajNmax = mTrajVnorm = mTrajImotMax = 0.0;
    mTrajLambda = mTrajLdLqDiff = 0.0; mTrajPolePairs = 1.0;
    mTrajLiveId = mTrajLiveIq = mTrajLiveRpm = mTrajLiveTorque = mTrajLiveImag = 0.0;
    mBaseSpeedRpm = 0.0;
    mTrajLiveVin = 0.0;
    setupTrajTab();

    connect(mTimer, SIGNAL(timeout()),
            this, SLOT(timerSlot()));
}

PageRtData::~PageRtData()
{
    delete ui;
}

VescInterface *PageRtData::vesc() const
{
    return mVesc;
}

void PageRtData::setVesc(VescInterface *vesc)
{
    mVesc = vesc;

    if (mVesc) {
        connect(mVesc->commands(), SIGNAL(valuesReceived(MC_VALUES,uint)),
                this, SLOT(valuesReceived(MC_VALUES,uint)));
        connect(mVesc->commands(), SIGNAL(rotorPosReceived(double)),
                this, SLOT(rotorPosReceived(double)));
        // Rebuild on individual param edits...
        connect(mVesc->mcConfig(), &ConfigParams::paramChangedDouble, this,
                [this](QObject *, QString name, double) {
            if (name.startsWith("foc_traj") || name.startsWith("foc_motor") ||
                    name == "si_motor_poles" || name == "l_current_max" || name == "l_max_duty") {
                updateTrajTable();
            }
        });
        // ...and on a bulk config read (Read Motor Configuration emits updated()).
        connect(mVesc->mcConfig(), &ConfigParams::updated, this, [this]() { updateTrajTable(); });
        updateTrajTable();
    }
}

void PageRtData::timerSlot()
{
    if (mVesc) {
        if (mVesc->isRtLogOpen() != ui->logRtButton->isChecked()) {
            ui->logRtButton->setChecked(mVesc->isRtLogOpen());
        }
    }

    if (mUpdateValPlot) {
        int dataSize = mTempMosVec.size();

        QVector<double> xAxis(dataSize);
        for (int i = 0;i < mSeconds.size();i++) {
            xAxis[i] = mSeconds[i];
        }

        // Current and duty-plot
        int graphIndex = 0;
        ui->currentPlot->graph(graphIndex++)->setData(xAxis, mCurrInVec);
        ui->currentPlot->graph(graphIndex++)->setData(xAxis, mCurrMotorVec);
        ui->currentPlot->graph(graphIndex++)->setData(xAxis, mDutyVec);

        // Temperature plot
        ui->tempPlot->clearGraphs();

        graphIndex = 0;

        ui->tempPlot->addGraph();
        ui->tempPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph1")));
        ui->tempPlot->graph(graphIndex)->setName("Temperature MOSFET");
        ui->tempPlot->graph(graphIndex)->setData(xAxis, mTempMosVec);
        ui->tempPlot->graph(graphIndex)->setVisible(ui->tempShowMosfetBox->isChecked());

        graphIndex++;

        if (!mTempMos1Vec.isEmpty() && mTempMos1Vec.last() != 0.0) {
            ui->tempPlot->addGraph();
            ui->tempPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph2")));
            ui->tempPlot->graph(graphIndex)->setName("Temperature MOSFET 1");
            ui->tempPlot->graph(graphIndex)->setData(xAxis, mTempMos1Vec);
            ui->tempPlot->graph(graphIndex)->setVisible(ui->tempShowMosfetBox->isChecked());
            graphIndex++;

            ui->tempPlot->addGraph();
            ui->tempPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph3")));
            ui->tempPlot->graph(graphIndex)->setName("Temperature MOSFET 2");
            ui->tempPlot->graph(graphIndex)->setData(xAxis, mTempMos2Vec);
            ui->tempPlot->graph(graphIndex)->setVisible(ui->tempShowMosfetBox->isChecked());
            graphIndex++;

            ui->tempPlot->addGraph();
            ui->tempPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph4")));
            ui->tempPlot->graph(graphIndex)->setName("Temperature MOSFET 3");
            ui->tempPlot->graph(graphIndex)->setData(xAxis, mTempMos3Vec);
            ui->tempPlot->graph(graphIndex)->setVisible(ui->tempShowMosfetBox->isChecked());
            graphIndex++;
        }

        ui->tempPlot->addGraph(ui->tempPlot->xAxis, ui->tempPlot->yAxis2);
        ui->tempPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph5")));
        ui->tempPlot->graph(graphIndex)->setName("Temperature Motor");
        ui->tempPlot->graph(graphIndex)->setData(xAxis, mTempMotorVec);
        ui->tempPlot->graph(graphIndex)->setVisible(ui->tempShowMotorBox->isChecked());
        graphIndex++;

        // RPM plot
        graphIndex = 0;
        ui->rpmPlot->graph(graphIndex++)->setData(xAxis, mRpmVec);

        // FOC plot
        graphIndex = 0;
        ui->focPlot->graph(graphIndex++)->setData(xAxis, mIdVec);
        ui->focPlot->graph(graphIndex++)->setData(xAxis, mIqVec);
        ui->focPlot->graph(graphIndex++)->setData(xAxis, mVdVec);
        ui->focPlot->graph(graphIndex++)->setData(xAxis, mVqVec);

        if (ui->autoscaleButton->isChecked()) {
            ui->currentPlot->rescaleAxes();
            ui->tempPlot->rescaleAxes();
            ui->rpmPlot->rescaleAxes();
            ui->focPlot->rescaleAxes();
        }

        ui->currentPlot->replotWhenVisible();
        ui->tempPlot->replotWhenVisible();
        ui->rpmPlot->replotWhenVisible();
        ui->focPlot->replotWhenVisible();

        mUpdateValPlot = false;
    }

    if (mUpdatePosPlot) {
        QVector<double> xAxis(mPositionVec.size());
        for (int i = 0;i < mPositionVec.size();i++) {
            xAxis[i] = double(i);
        }

        ui->posBar->setValue(int(fabs(mPositionVec.last())));
        ui->posPlot->graph(0)->setData(xAxis, mPositionVec);

        if (ui->autoscaleButton->isChecked()) {
            ui->posPlot->rescaleAxes();
        }

        ui->posPlot->replotWhenVisible();

        mUpdatePosPlot = false;
    }

    if (mUpdateTrajPlot) {
        updateTrajLive();
        mUpdateTrajPlot = false;
    }
}

void PageRtData::valuesReceived(MC_VALUES values, unsigned int mask)
{
    (void)mask;
    if (mVesc) {
        ui->rtText->setMotorPoles(mVesc->mcConfig()->getParamInt("si_motor_poles"));
    }
    ui->rtText->setValues(values);

    const int maxS = 500;

    appendDoubleAndTrunc(&mTempMosVec, values.temp_mos, maxS);
    appendDoubleAndTrunc(&mTempMos1Vec, values.temp_mos_1, maxS);
    appendDoubleAndTrunc(&mTempMos2Vec, values.temp_mos_2, maxS);
    appendDoubleAndTrunc(&mTempMos3Vec, values.temp_mos_3, maxS);
    appendDoubleAndTrunc(&mTempMotorVec, values.temp_motor, maxS);
    appendDoubleAndTrunc(&mCurrInVec, values.current_in, maxS);
    appendDoubleAndTrunc(&mCurrMotorVec, values.current_motor, maxS);
    appendDoubleAndTrunc(&mIdVec, values.id, maxS);
    appendDoubleAndTrunc(&mIqVec, values.iq, maxS);
    appendDoubleAndTrunc(&mDutyVec, values.duty_now, maxS);
    appendDoubleAndTrunc(&mRpmVec, values.rpm, maxS);
    appendDoubleAndTrunc(&mVdVec, values.vd, maxS);
    appendDoubleAndTrunc(&mVqVec, values.vq, maxS);

    qint64 tNow = QDateTime::currentMSecsSinceEpoch();

    double elapsed = double((tNow - mLastUpdateTime)) / 1000.0;
    if (elapsed > 1.0) {
        elapsed = 1.0;
    }

    mSecondCounter += elapsed;

    appendDoubleAndTrunc(&mSeconds, mSecondCounter, maxS);

    mLastUpdateTime = tNow;

    mUpdateValPlot = true;

    // --- live operating point for the trajectory tab ---
    double pp = mTrajPolePairs > 0.0 ? mTrajPolePairs : 1.0;
    mTrajLiveId = values.id;
    mTrajLiveIq = values.iq;
    mTrajLiveImag = sqrt(values.id * values.id + values.iq * values.iq);
    // electrical -> mechanical rpm, then Vnorm-normalise so the point lines up with the
    // table the same way the firmware does its lookup (rpm *= vnorm / vbus).
    double rpm = fabs(values.rpm) / pp;
    if (mTrajVnorm > 0.0 && values.v_in > 1.0) {
        rpm *= mTrajVnorm / values.v_in;
    }
    mTrajLiveRpm = rpm;
    // T = 1.5 * p * iq * (lambda - ld_lq_diff * id)   (id is VESC-negative; reluctance term adds)
    mTrajLiveTorque = 1.5 * pp * values.iq * (mTrajLambda - mTrajLdLqDiff * values.id);

    appendDoubleAndTrunc(&mTrajIdTrail, values.id, 300);
    appendDoubleAndTrunc(&mTrajIqTrail, values.iq, 300);
    mUpdateTrajPlot = true;

    mTrajLiveVin = values.v_in; // base-speed panel tracks the live bus when not overriding
}

void PageRtData::rotorPosReceived(double pos)
{
    appendDoubleAndTrunc(&mPositionVec, pos, 1500);
    mUpdatePosPlot = true;
}

// Keep in sync with bldc datatypes.h MTPA_TRAJ_NI / MTPA_TRAJ_NS.
#define RT_TRAJ_NI 8
#define RT_TRAJ_NS 7

void PageRtData::setupTrajTab()
{
    mTrajDq = new QCustomPlot();
    mTrajTn = new QCustomPlot();
    mTrajMap = new QCustomPlot();
    QCustomPlot *plots[3] = {mTrajDq, mTrajTn, mTrajMap};
    for (int i = 0; i < 3; i++) {
        Utility::setPlotColors(plots[i]);
        plots[i]->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
        plots[i]->legend->setVisible(true);
        plots[i]->legend->setBrush(QBrush(QColor(0, 0, 0, 100)));
    }

    QColor cLive = Utility::getAppQColor("plot_graph2");
    QColor cCurve = Utility::getAppQColor("plot_graph1");
    QColor cAux = Utility::getAppQColor("plot_graph3");

    // --- dq current plane: circle (0), locus (1), trail (2), live point (3) ---
    mTrajDq->addGraph();
    mTrajDq->graph(0)->setPen(QPen(cAux, 1, Qt::DashLine));
    mTrajDq->graph(0)->setName("Current limit");
    mTrajDq->addGraph();
    mTrajDq->graph(1)->setPen(QPen(cCurve, 2));
    mTrajDq->graph(1)->setName("Table locus @ speed");
    mTrajDq->addGraph();
    mTrajDq->graph(2)->setPen(QPen(QColor(cLive.red(), cLive.green(), cLive.blue(), 110), 1));
    mTrajDq->graph(2)->setName("Recent");
    mTrajDq->addGraph();
    mTrajDq->graph(3)->setLineStyle(QCPGraph::lsNone);
    mTrajDq->graph(3)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, cLive, 12));
    mTrajDq->graph(3)->setName("Now");
    // 4: max-power / control trajectory (id*,iq* at Iₛ swept over speed)
    mTrajDq->addGraph();
    mTrajDq->graph(4)->setPen(QPen(QColor("#e377c2"), 2));
    mTrajDq->graph(4)->setName("Max-power trajectory");
    // 5..8: voltage-limit ellipses at increasing speed
    for (int e = 0; e < 4; e++) {
        mTrajDq->addGraph();
        QColor ec = cCurve; ec.setAlpha(170 - e * 32);
        mTrajDq->graph(5 + e)->setPen(QPen(ec, 1, Qt::DotLine));
        if (e == 0) {
            mTrajDq->graph(5 + e)->setName("Voltage-limit ellipse");
        } else {
            mTrajDq->graph(5 + e)->removeFromLegend();
        }
    }
    mTrajDq->xAxis->setLabel("id (A)   ← field weakening");
    mTrajDq->yAxis->setLabel("iq (A)  (torque)");

    // --- torque vs speed: envelope (0), live point (1) ---
    mTrajTn->addGraph();
    mTrajTn->graph(0)->setPen(QPen(cCurve, 2));
    mTrajTn->graph(0)->setName("Envelope (const-T → const-P)");
    mTrajTn->addGraph();
    mTrajTn->graph(1)->setLineStyle(QCPGraph::lsNone);
    mTrajTn->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, cLive, 12));
    mTrajTn->graph(1)->setName("Now");
    mTrajTn->addGraph(); // 2: base-speed line
    mTrajTn->graph(2)->setPen(QPen(cAux, 1, Qt::DashLine));
    mTrajTn->graph(2)->setName("Base speed");
    mTrajTn->xAxis->setLabel("speed (rpm, mech)");
    mTrajTn->yAxis->setLabel("torque (Nm)");

    // --- |I| vs speed, id* color map + live point (graph 0) ---
    mTrajColorMap = new QCPColorMap(mTrajMap->xAxis, mTrajMap->yAxis);
    QCPColorScale *scale = new QCPColorScale(mTrajMap);
    mTrajMap->plotLayout()->addElement(0, 1, scale);
    scale->setType(QCPAxis::atRight);
    mTrajColorMap->setColorScale(scale);
    mTrajColorMap->setGradient(QCPColorGradient::gpJet);
    mTrajColorMap->setInterpolate(true);
    scale->axis()->setLabel("id* (A)");
    mTrajMap->addGraph();
    mTrajMap->graph(0)->setLineStyle(QCPGraph::lsNone);
    mTrajMap->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCrossCircle, Qt::white, 13));
    mTrajMap->graph(0)->setName("Now");
    mTrajMap->addGraph(); // 1: base-speed line
    mTrajMap->graph(1)->setPen(QPen(Qt::white, 1, Qt::DashLine));
    mTrajMap->graph(1)->setName("Base speed");
    mTrajMap->xAxis->setLabel("speed (rpm, mech)");
    mTrajMap->yAxis->setLabel("|I| (A)");

    // --- section 4: base-speed panel ---
    // Locked by default: inputs come live from the FOC params / Motor Settings tables
    // (+ live bus). Tick "Override" to unlock the fields and try your own values. The
    // computed result is always shown (read-only) below.
    QGroupBox *bsBox = new QGroupBox(tr("Base speed (MTPA → field-weakening knee)"));
    QFormLayout *bsForm = new QFormLayout(bsBox);
    mBsOverride = new QCheckBox(tr("Override (edit values; otherwise from config + live bus)"));
    mBsOverride->setChecked(false);
    mBsType = new QComboBox();
    mBsType->addItem(tr("PMSM (ψ ≈ flux linkage)"));
    mBsType->addItem(tr("SynRM / PMa-SynRM (salient ψ at MTPA)"));
    mBsVbus = new QDoubleSpinBox(); mBsVbus->setRange(1, 1000); mBsVbus->setSuffix(" V"); mBsVbus->setValue(50);
    mBsDuty = new QDoubleSpinBox(); mBsDuty->setRange(1, 100); mBsDuty->setSuffix(" %"); mBsDuty->setValue(95);
    mBsFlux = new QDoubleSpinBox(); mBsFlux->setRange(0, 100000); mBsFlux->setDecimals(3); mBsFlux->setSuffix(" mWb"); mBsFlux->setValue(20);
    mBsCurrent = new QDoubleSpinBox(); mBsCurrent->setRange(0, 5000); mBsCurrent->setSuffix(" A"); mBsCurrent->setDecimals(0); mBsCurrent->setValue(100);
    mBsResult = new QLabel("—");
    mBsResult->setTextFormat(Qt::RichText);
    mBsResult->setWordWrap(true);
    bsForm->addRow(mBsOverride);
    bsForm->addRow(tr("Motor type"), mBsType);
    bsForm->addRow(tr("Bus voltage"), mBsVbus);
    bsForm->addRow(tr("Max duty"), mBsDuty);
    bsForm->addRow(tr("Flux linkage λ"), mBsFlux);
    bsForm->addRow(tr("Max current Iₛ"), mBsCurrent);
    QFrame *bsLine = new QFrame(); bsLine->setFrameShape(QFrame::HLine);
    bsForm->addRow(bsLine);
    bsForm->addRow(new QLabel(tr("<b>Computed:</b>")));
    bsForm->addRow(mBsResult);

    // inputs are only editable in override mode
    mBsType->setEnabled(false); mBsVbus->setEnabled(false);
    mBsDuty->setEnabled(false); mBsFlux->setEnabled(false); mBsCurrent->setEnabled(false);
    connect(mBsOverride, &QCheckBox::toggled, this, [this](bool on) {
        mBsType->setEnabled(on); mBsVbus->setEnabled(on);
        mBsDuty->setEnabled(on); mBsFlux->setEnabled(on); mBsCurrent->setEnabled(on);
        computeBaseSpeed();
    });
    connect(mBsType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){ if (mBsOverride->isChecked()) computeBaseSpeed(); });
    connect(mBsVbus, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double){ if (mBsOverride->isChecked()) computeBaseSpeed(); });
    connect(mBsDuty, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double){ if (mBsOverride->isChecked()) computeBaseSpeed(); });
    connect(mBsFlux, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double){ if (mBsOverride->isChecked()) computeBaseSpeed(); });
    connect(mBsCurrent, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double){ if (mBsOverride->isChecked()) computeBaseSpeed(); });

    QWidget *tab = new QWidget();
    QGridLayout *lay = new QGridLayout(tab);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->addWidget(mTrajDq, 0, 0);
    lay->addWidget(mTrajTn, 0, 1);
    lay->addWidget(mTrajMap, 1, 0);
    lay->addWidget(bsBox, 1, 1);
    ui->tabWidget->addTab(tab, "Trajectory");

    // Refresh from the live config whenever the user switches to this tab.
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int){ updateTrajTable(); });
}

// Base speed: where the MTPA voltage demand hits the bus limit (the FW knee).
//   Vmax = (1/√3)·duty·Vbus ;  ω_base[elec] = Vmax/|ψ| ;  rpm = ω_base·60/(2π·p)
// PMSM: |ψ| ≈ λ.  SynRM: |ψ| = √((λ+Ld·id)² + (Lq·iq)²) at the MTPA point for I = current limit.
void PageRtData::computeBaseSpeed()
{
    if (!mBsOverride || !mTrajTn || !mTrajMap) {
        return;
    }
    bool ov = mBsOverride->isChecked();
    ConfigParams *mc = mVesc ? mVesc->mcConfig() : nullptr;

    // Pole pairs, Ld/Lq and saliency always come live from the config (not overridable here).
    int poles = mc ? mc->getParamInt("si_motor_poles") : 0;
    double pp = poles >= 2 ? poles / 2.0 : (mTrajPolePairs > 0.0 ? mTrajPolePairs : 1.0);
    double ldlqdiff = mc ? mc->getParamDouble("foc_motor_ld_lq_diff") : mTrajLdLqDiff;
    double Lavg = mc ? mc->getParamDouble("foc_motor_l") : 0.0;
    double Ld = Lavg - ldlqdiff / 2.0;
    double Lq = Lavg + ldlqdiff / 2.0;

    double Vbus, duty, lambda, Is;
    int type;
    if (ov) {
        Vbus = mBsVbus->value();
        duty = mBsDuty->value() / 100.0;
        lambda = mBsFlux->value() / 1000.0;
        Is = mBsCurrent->value();
        type = mBsType->currentIndex();
    } else {
        // Inputs read LIVE from FOC params / Motor Settings + live bus; mirror into the locked fields.
        Vbus = mTrajLiveVin > 1.0 ? mTrajLiveVin : mBsVbus->value();
        duty = mc ? mc->getParamDouble("l_max_duty") : 0.95;
        lambda = mc ? mc->getParamDouble("foc_motor_flux_linkage") : mTrajLambda;
        Is = mc ? mc->getParamDouble("l_current_max") : (mTrajImax > 1.0 ? mTrajImax : 100.0);
        type = (mc && mc->getParamEnum("motor_type") == 3) ? 1 : 0;
        mBsVbus->blockSignals(true); mBsVbus->setValue(Vbus); mBsVbus->blockSignals(false);
        mBsDuty->blockSignals(true); mBsDuty->setValue(duty * 100.0); mBsDuty->blockSignals(false);
        mBsFlux->blockSignals(true); mBsFlux->setValue(lambda * 1000.0); mBsFlux->blockSignals(false);
        mBsCurrent->blockSignals(true); mBsCurrent->setValue(Is); mBsCurrent->blockSignals(false);
        mBsType->blockSignals(true); mBsType->setCurrentIndex(type); mBsType->blockSignals(false);
    }

    double Vmax = (1.0 / sqrt(3.0)) * duty * Vbus;
    double psi, idS = 0.0, iqS = 0.0;
    if (type == 0) {
        psi = lambda; // PMSM
    } else {
        idS = mTrajHasTable ? trajLookupId(Is, 0.0) : 0.0;
        iqS = sqrt(qMax(0.0, Is * Is - idS * idS));
        double psd = lambda + Ld * idS, psq = Lq * iqS;
        psi = sqrt(psd * psd + psq * psq);
    }
    double wbaseElec = psi > 1e-9 ? Vmax / psi : 0.0;
    double rpm = wbaseElec * 60.0 / (2.0 * M_PI * pp);
    mBaseSpeedRpm = rpm;

    // Peak torque at MTPA (speed 0) for current Iₛ. PMSM: id=0, iq=Iₛ.
    double id0 = (type == 0) ? 0.0 : idS;
    double iq0 = (type == 0) ? Is : iqS;
    double Tmax = 1.5 * pp * iq0 * (lambda - ldlqdiff * id0);

    // Envelope: constant torque up to base speed, then constant power (T = Tmax·base/speed).
    // Knee sits exactly on the base-speed line.
    QVector<double> ex, ey;
    double rpmMax = qMax(qMax(mTrajNmax, rpm * 2.5), mTrajLiveRpm);
    if (rpmMax < 1.0) {
        rpmMax = 3000.0;
    }
    const int NSP = 200;
    for (int s = 0; s <= NSP; s++) {
        double r = rpmMax * s / double(NSP);
        double T = (rpm < 1.0 || r <= rpm) ? Tmax : Tmax * rpm / r;
        ex.append(r);
        ey.append(T);
    }
    mTrajTn->graph(0)->setData(ex, ey);
    if (Tmax > 0.0) {
        mTrajTn->yAxis->setRange(0, Tmax * 1.15);
    }

    QString res = tr("<b>Base speed: %1 rpm</b>&nbsp; (%2 ERPM)<br>"
                     "V<sub>max</sub> = %3 V&nbsp;&nbsp; |ψ| = %4 mWb&nbsp;&nbsp; T<sub>peak</sub> = %5 Nm")
            .arg(rpm, 0, 'f', 0).arg(rpm * pp, 0, 'f', 0)
            .arg(Vmax, 0, 'f', 1).arg(psi * 1000.0, 0, 'f', 2).arg(Tmax, 0, 'f', 1);
    if (type == 1) {
        res += tr("<br>MTPA @ %1 A:&nbsp; id* = %2,&nbsp; iq* = %3 A"
                  "<br><i>PMa-SynRM: |ψ| & base speed depend on Iₛ (the L<sub>q</sub>·iq term)</i>")
                .arg(Is, 0, 'f', 0).arg(idS, 0, 'f', 1).arg(iqS, 0, 'f', 1);
    }
    if (!ov && mTrajLiveVin <= 1.0) {
        res += tr("<br><i>(bus = %1 V assumed until RT data streams)</i>").arg(Vbus, 0, 'f', 0);
    }
    mBsResult->setText(res);

    // Make sure the speed axis covers the table, the base speed AND the live point,
    // so the base-speed line never falls off the right edge.
    double xmax = qMax(qMax(mTrajNmax, rpm), mTrajLiveRpm) * 1.06;
    if (xmax < 1.0) {
        xmax = 1000.0;
    }
    mTrajTn->xAxis->setRange(0, xmax);
    mTrajMap->xAxis->setRange(0, xmax);

    // vertical base-speed line on T-N and map
    double tnTop = mTrajTn->yAxis->range().upper;
    mTrajTn->graph(2)->setData(QVector<double>() << rpm << rpm,
                               QVector<double>() << 0.0 << tnTop);
    double mapTop = mTrajImax > 0.0 ? mTrajImax : mTrajMap->yAxis->range().upper;
    mTrajMap->graph(1)->setData(QVector<double>() << rpm << rpm,
                                QVector<double>() << 0.0 << mapTop);

    // dq plane: max-power trajectory (id*,iq* at Iₛ over speed) + voltage-limit ellipses
    if (mTrajDq->graphCount() >= 9) {
        QVector<double> tx, ty;
        const int NSP2 = 80;
        double rmax = qMax(mTrajNmax, rpm * 4.0);
        for (int s = 0; s <= NSP2; s++) {
            double r = rmax * s / double(NSP2);
            double idr = mTrajHasTable ? trajLookupId(Is, r) : 0.0;
            double iqr = sqrt(qMax(0.0, Is * Is - idr * idr));
            tx.append(idr); ty.append(iqr);
        }
        mTrajDq->graph(4)->setData(tx, ty);

        // ellipse: (Ld·id+λ)² + (Lq·iq)² = (Vmax/ω_elec)²  → centre (-λ/Ld, 0)
        double idC = Ld > 1e-9 ? -lambda / Ld : 0.0;
        double mult[4] = {1.0, 1.6, 2.6, 4.2};
        for (int e = 0; e < 4; e++) {
            double r = rpm * mult[e];
            double wElec = r * 2.0 * M_PI * pp / 60.0;
            double VoW = wElec > 1e-6 ? Vmax / wElec : 0.0;
            double aId = Ld > 1e-9 ? VoW / Ld : 0.0;
            double aIq = Lq > 1e-9 ? VoW / Lq : 0.0;
            QVector<double> ux, uy;
            for (int k = 0; k <= 60; k++) {
                double th = 2.0 * M_PI * k / 60.0;
                ux.append(idC + aId * cos(th));
                uy.append(aIq * sin(th));
            }
            mTrajDq->graph(5 + e)->setData(ux, uy);
        }
        mTrajDq->replotWhenVisible();
    }

    mTrajTn->replotWhenVisible();
    mTrajMap->replotWhenVisible();
}

// Bilinear lookup of id* (VESC-negative amps) over the loaded 2-D table; mirrors the firmware.
double PageRtData::trajLookupId(double imag, double rpm)
{
    const int NI = RT_TRAJ_NI, NS = RT_TRAJ_NS;
    if (mTrajLut.size() < NI * NS || mTrajImax <= 0.0 || mTrajNmax <= 0.0) {
        return 0.0;
    }
    double fi = imag / mTrajImax * (NI - 1);
    double fs = rpm / mTrajNmax * (NS - 1);
    fi = qBound(0.0, fi, double(NI - 1));
    fs = qBound(0.0, fs, double(NS - 1));
    int i0 = int(fi), s0 = int(fs);
    int i1 = qMin(i0 + 1, NI - 1), s1 = qMin(s0 + 1, NS - 1);
    double ti = fi - i0, ts = fs - s0;
    double a = mTrajLut[i0 * NS + s0] * (1 - ts) + mTrajLut[i0 * NS + s1] * ts;
    double b = mTrajLut[i1 * NS + s0] * (1 - ts) + mTrajLut[i1 * NS + s1] * ts;
    return a * (1 - ti) + b * ti;
}

void PageRtData::updateTrajTable()
{
    if (!mVesc) {
        return;
    }
    const int NI = RT_TRAJ_NI, NS = RT_TRAJ_NS;
    ConfigParams *mc = mVesc->mcConfig();
    mTrajImax = mc->getParamDouble("foc_traj_imax");
    mTrajNmax = mc->getParamDouble("foc_traj_nmax");
    mTrajVnorm = mc->getParamDouble("foc_traj_vnorm");
    mTrajLambda = mc->getParamDouble("foc_motor_flux_linkage");
    mTrajLdLqDiff = mc->getParamDouble("foc_motor_ld_lq_diff");
    int poles = mc->getParamInt("si_motor_poles");
    mTrajPolePairs = poles >= 2 ? poles / 2.0 : 1.0;
    mTrajImotMax = mc->getParamDouble("l_current_max");
    mTrajLut.resize(NI * NS);
    for (int i = 0; i < NI * NS; i++) {
        mTrajLut[i] = mc->getParamDouble(QString("foc_traj_lut__%1").arg(i));
    }
    mTrajHasTable = (mTrajImax > 0.0 && mTrajNmax > 0.0);
    if (!mTrajHasTable) {
        computeBaseSpeed();
        return;
    }

    // dq: current-limit circle (radius = motor current max, fall back to table imax)
    double R = mTrajImotMax > 1.0 ? mTrajImotMax : mTrajImax;
    QVector<double> cx, cy;
    for (int k = 0; k <= 120; k++) {
        double a = 2.0 * M_PI * k / 120.0;
        cx.append(R * cos(a));
        cy.append(R * sin(a));
    }
    mTrajDq->graph(0)->setData(cx, cy);
    mTrajDq->xAxis->setRange(-R * 1.1, R * 1.1);
    mTrajDq->yAxis->setRange(-R * 1.1, R * 1.1);

    // map: id* color over (speed, |I|)
    mTrajColorMap->data()->setSize(NS, NI);
    mTrajColorMap->data()->setRange(QCPRange(0, mTrajNmax), QCPRange(0, mTrajImax));
    for (int i = 0; i < NI; i++) {
        for (int s = 0; s < NS; s++) {
            mTrajColorMap->data()->setCell(s, i, mTrajLut[i * NS + s]);
        }
    }
    mTrajColorMap->rescaleDataRange(true);
    mTrajMap->xAxis->setRange(0, mTrajNmax);
    mTrajMap->yAxis->setRange(0, mTrajImax);

    mTrajDq->replotWhenVisible();
    mTrajTn->replotWhenVisible();
    mTrajMap->replotWhenVisible();

    computeBaseSpeed(); // redraw the base-speed line against the rebuilt axes
}

void PageRtData::updateTrajLive()
{
    QVector<double> px, py;

    // dq plane: live point + trail + table locus at the current speed
    px = QVector<double>() << mTrajLiveId;
    py = QVector<double>() << mTrajLiveIq;
    mTrajDq->graph(3)->setData(px, py);
    mTrajDq->graph(2)->setData(mTrajIdTrail, mTrajIqTrail);
    if (mTrajHasTable) {
        QVector<double> lx, ly;
        for (int k = 0; k <= 40; k++) {
            double I = mTrajImax * k / 40.0;
            double idS = trajLookupId(I, mTrajLiveRpm);
            double iqS = sqrt(qMax(0.0, I * I - idS * idS));
            lx.append(idS);
            ly.append(iqS);
        }
        mTrajDq->graph(1)->setData(lx, ly);
    }
    mTrajDq->replotWhenVisible();

    // torque-speed
    mTrajTn->graph(1)->setData(QVector<double>() << mTrajLiveRpm,
                               QVector<double>() << mTrajLiveTorque);
    mTrajTn->replotWhenVisible();

    // current-speed map
    mTrajMap->graph(0)->setData(QVector<double>() << mTrajLiveRpm,
                                QVector<double>() << mTrajLiveImag);

    // grow the speed axis if the live point runs past the right edge
    if (mTrajLiveRpm * 1.06 > mTrajTn->xAxis->range().upper) {
        double xmax = mTrajLiveRpm * 1.06;
        mTrajTn->xAxis->setRange(0, xmax);
        mTrajMap->xAxis->setRange(0, xmax);
    }
    // grow the torque axis if the live torque exceeds it
    if (fabs(mTrajLiveTorque) > mTrajTn->yAxis->range().upper) {
        mTrajTn->yAxis->setRange(0, fabs(mTrajLiveTorque) * 1.1);
    }
    mTrajMap->replotWhenVisible();

    // when not overriding, keep base speed tracking the live bus voltage
    if (mBsOverride && !mBsOverride->isChecked()) {
        computeBaseSpeed();
    }
}

void PageRtData::appendDoubleAndTrunc(QVector<double> *vec, double num, int maxSize)
{
    vec->append(num);

    if(vec->size() > maxSize) {
        vec->remove(0, vec->size() - maxSize);
    }
}

void PageRtData::updateZoom()
{
    Qt::Orientations plotOrientations = Qt::Orientations(
            ((ui->zoomHButton->isChecked() ? Qt::Horizontal : 0) |
             (ui->zoomVButton->isChecked() ? Qt::Vertical : 0)));

    ui->currentPlot->axisRect()->setRangeZoom(plotOrientations);
    ui->tempPlot->axisRect()->setRangeZoom(plotOrientations);
    ui->rpmPlot->axisRect()->setRangeZoom(plotOrientations);
    ui->focPlot->axisRect()->setRangeZoom(plotOrientations);
    ui->posPlot->axisRect()->setRangeZoom(plotOrientations);
}

void PageRtData::on_zoomHButton_toggled(bool checked)
{
    (void)checked;
    updateZoom();
}

void PageRtData::on_zoomVButton_toggled(bool checked)
{
    (void)checked;
    updateZoom();
}

void PageRtData::on_rescaleButton_clicked()
{
    ui->currentPlot->rescaleAxes();
    ui->tempPlot->rescaleAxes();
    ui->rpmPlot->rescaleAxes();
    ui->focPlot->rescaleAxes();
    ui->posPlot->rescaleAxes();

    ui->currentPlot->replotWhenVisible();
    ui->tempPlot->replotWhenVisible();
    ui->rpmPlot->replotWhenVisible();
    ui->focPlot->replotWhenVisible();
    ui->posPlot->replotWhenVisible();
}

void PageRtData::on_posInductanceButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_INDUCTANCE);
    }
}

void PageRtData::on_posObserverButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_OBSERVER);
    }
}

void PageRtData::on_posEncoderButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_ENCODER);
    }
}

void PageRtData::on_posPidButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_PID_POS);
    }
}

void PageRtData::on_posPidErrorButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_PID_POS_ERROR);
    }
}

void PageRtData::on_posEncoderObserverErrorButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_ENCODER_OBSERVER_ERROR);
    }
}

void PageRtData::on_posStopButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_NONE);
    }
}

void PageRtData::on_tempShowMosfetBox_toggled(bool checked)
{
    if (ui->tempPlot->graphCount() > 0) {
        ui->tempPlot->graph(0)->setVisible(checked);
    }
}

void PageRtData::on_tempShowMotorBox_toggled(bool checked)
{
    if (ui->tempPlot->graphCount() > 1) {
        ui->tempPlot->graph(1)->setVisible(checked);
    }
}

void PageRtData::on_logRtButton_toggled(bool checked)
{
    QSettings set;
    set.sync();
    if (checked) {
        if (mVesc) {
            mVesc->openRtLogFile(set.value("path_rt_log", "./log").toString());
        }
    } else {
        mVesc->closeRtLogFile();
    }
}

void PageRtData::on_posHallObserverErrorButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_HALL_OBSERVER_ERROR);
    }
}

