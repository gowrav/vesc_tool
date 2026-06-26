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

#ifndef PAGERTDATA_H
#define PAGERTDATA_H

#include <QWidget>
#include <QVector>
#include <QTimer>
#include "vescinterface.h"

namespace Ui {
class PageRtData;
}

class QCustomPlot;
class QCPColorMap;

class PageRtData : public QWidget
{
    Q_OBJECT

public:
    explicit PageRtData(QWidget *parent = nullptr);
    ~PageRtData();

    VescInterface *vesc() const;
    void setVesc(VescInterface *vesc);

private slots:
    void timerSlot();
    void valuesReceived(MC_VALUES values, unsigned int mask);
    void rotorPosReceived(double pos);

    void on_zoomHButton_toggled(bool checked);
    void on_zoomVButton_toggled(bool checked);
    void on_rescaleButton_clicked();
    void on_posInductanceButton_clicked();
    void on_posObserverButton_clicked();
    void on_posEncoderButton_clicked();
    void on_posPidButton_clicked();
    void on_posPidErrorButton_clicked();
    void on_posEncoderObserverErrorButton_clicked();
    void on_posStopButton_clicked();
    void on_tempShowMosfetBox_toggled(bool checked);
    void on_tempShowMotorBox_toggled(bool checked);
    void on_logRtButton_toggled(bool checked);
    void on_posHallObserverErrorButton_clicked();

private:
    Ui::PageRtData *ui;
    VescInterface *mVesc;
    QTimer *mTimer;

    QVector<double> mTempMosVec;
    QVector<double> mTempMos1Vec;
    QVector<double> mTempMos2Vec;
    QVector<double> mTempMos3Vec;
    QVector<double> mTempMotorVec;
    QVector<double> mCurrInVec;
    QVector<double> mCurrMotorVec;
    QVector<double> mIdVec;
    QVector<double> mIqVec;
    QVector<double> mDutyVec;
    QVector<double> mRpmVec;
    QVector<double> mPositionVec;
    QVector<double> mSeconds;
    QVector<double> mVdVec;
    QVector<double> mVqVec;

    double mSecondCounter;
    qint64 mLastUpdateTime;

    bool mUpdateValPlot;
    bool mUpdatePosPlot;

    // --- SynRM trajectory "operating point" tab ---
    QCustomPlot *mTrajDq;       // dq current plane (id vs iq)
    QCustomPlot *mTrajTn;       // torque vs speed
    QCustomPlot *mTrajMap;      // |I| vs speed, id* color map
    QCPColorMap *mTrajColorMap;
    QVector<double> mTrajIdTrail, mTrajIqTrail;
    bool mUpdateTrajPlot;
    // cached config (refreshed in updateTrajTable)
    QVector<double> mTrajLut;
    double mTrajImax, mTrajNmax, mTrajVnorm, mTrajImotMax;
    double mTrajLambda, mTrajLdLqDiff, mTrajPolePairs;
    bool mTrajHasTable;
    // live operating point
    double mTrajLiveId, mTrajLiveIq, mTrajLiveRpm, mTrajLiveTorque, mTrajLiveImag;

    void setupTrajTab();
    void updateTrajTable();                       // rebuild static curves + color map from config
    double trajLookupId(double imag, double rpm); // bilinear over foc_traj_lut (mirrors firmware)
    void updateTrajLive();                        // push the live point to the 3 plots

    void appendDoubleAndTrunc(QVector<double> *vec, double num, int maxSize);
    void updateZoom();

};

#endif // PAGERTDATA_H
