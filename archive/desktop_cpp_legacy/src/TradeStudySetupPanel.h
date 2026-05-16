#pragma once

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

class TradeStudySetupPanel : public QWidget
{
public:
    explicit TradeStudySetupPanel(QWidget* parent = nullptr);

    QComboBox* systemPresetCombo = nullptr;
    QLabel* systemPresetDescription = nullptr;
    QPushButton* applySystemPresetButton = nullptr;
    QComboBox* referencePreset = nullptr;
    QDoubleSpinBox* cellNominalVoltage = nullptr;
    QDoubleSpinBox* cellFullVoltage = nullptr;
    QDoubleSpinBox* cellEmptyVoltage = nullptr;
    QDoubleSpinBox* cellCutoffVoltage = nullptr;
    QDoubleSpinBox* cellCapacity = nullptr;
    QDoubleSpinBox* cellsInSeries = nullptr;
    QDoubleSpinBox* cellsInParallel = nullptr;
    QDoubleSpinBox* internalResistance = nullptr;
    QDoubleSpinBox* ambientTemp = nullptr;
    QDoubleSpinBox* dischargeCurrent = nullptr;
    QDoubleSpinBox* duration = nullptr;
    QDoubleSpinBox* timeStep = nullptr;
    QDoubleSpinBox* initialSoc = nullptr;
    QDoubleSpinBox* packMass = nullptr;
    QDoubleSpinBox* packHeatCapacity = nullptr;
    QDoubleSpinBox* coolingCoeff = nullptr;
    QPushButton* updateLayoutButton = nullptr;
    QPushButton* runButton = nullptr;
    QPushButton* captureBaselineButton = nullptr;
    QPushButton* compareBaselineButton = nullptr;
    QPushButton* exportReportButton = nullptr;
    QPushButton* saveProjectButton = nullptr;
    QPushButton* loadProjectButton = nullptr;

private:
    static QDoubleSpinBox* createDoubleSpin(QWidget* parent, double value, double minimum, double maximum, int decimals);
};

