#include "TradeStudySetupPanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

QDoubleSpinBox* TradeStudySetupPanel::createDoubleSpin(
    QWidget* parent,
    double value,
    double minimum,
    double maximum,
    int decimals)
{
    auto* widget = new QDoubleSpinBox(parent);
    widget->setRange(minimum, maximum);
    widget->setDecimals(decimals);
    widget->setValue(value);
    return widget;
}

TradeStudySetupPanel::TradeStudySetupPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* shellLayout = new QVBoxLayout(this);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(scroll);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto* headerGroup = new QGroupBox("", content);
    auto* headerLayout = new QVBoxLayout(headerGroup);
    headerLayout->setContentsMargins(14, 14, 14, 14);
    headerLayout->setSpacing(4);
    auto* header = new QLabel("Trade Study Setup", headerGroup);
    header->setStyleSheet("font-size:16px; font-weight:700; color:#f3f7fb;");
    auto* subheader = new QLabel(
        "Choose an archetype, tune pack parameters, refresh the generated layout, then run a flagship workflow against a saved baseline.",
        headerGroup);
    subheader->setWordWrap(true);
    subheader->setStyleSheet("font-size:12px; color:#93a6ba;");
    headerLayout->addWidget(header);
    headerLayout->addWidget(subheader);
    layout->addWidget(headerGroup);

    systemPresetCombo = new QComboBox(content);
    systemPresetDescription = new QLabel("Loading trade-study archetypes...", content);
    systemPresetDescription->setWordWrap(true);
    systemPresetDescription->setStyleSheet("font-size:12px; color:#93a6ba;");
    applySystemPresetButton = new QPushButton("Apply Archetype", content);

    auto* presetGroup = new QGroupBox("1. Choose Archetype", content);
    auto* presetLayout = new QFormLayout(presetGroup);
    presetLayout->setContentsMargins(14, 16, 14, 14);
    presetLayout->setHorizontalSpacing(10);
    presetLayout->setVerticalSpacing(8);
    presetLayout->addRow("Archetype", systemPresetCombo);
    presetLayout->addRow("", applySystemPresetButton);
    presetLayout->addRow("Study brief", systemPresetDescription);
    layout->addWidget(presetGroup);

    referencePreset = new QComboBox(content);
    referencePreset->addItem("Custom");
    referencePreset->addItem("Panasonic NCR18650B");
    referencePreset->addItem("Samsung INR18650-30Q");
    referencePreset->addItem("A123 ANR26650M1-B");

    cellNominalVoltage = createDoubleSpin(content, 3.6, 0.1, 100.0, 3);
    cellFullVoltage = createDoubleSpin(content, 4.2, 0.1, 100.0, 3);
    cellEmptyVoltage = createDoubleSpin(content, 3.0, 0.0, 100.0, 3);
    cellCutoffVoltage = createDoubleSpin(content, 3.0, 0.0, 100.0, 3);
    cellCapacity = createDoubleSpin(content, 3.35, 0.1, 1000.0, 3);
    internalResistance = createDoubleSpin(content, 0.035, 0.0, 10.0, 4);

    auto* cellGroup = new QGroupBox("2. Reference Cell", content);
    auto* cellLayout = new QFormLayout(cellGroup);
    cellLayout->setContentsMargins(14, 16, 14, 14);
    cellLayout->setHorizontalSpacing(10);
    cellLayout->setVerticalSpacing(8);
    cellLayout->addRow("Cell reference", referencePreset);
    cellLayout->addRow("Nominal voltage (V)", cellNominalVoltage);
    cellLayout->addRow("Full voltage (V)", cellFullVoltage);
    cellLayout->addRow("Empty voltage (V)", cellEmptyVoltage);
    cellLayout->addRow("Cutoff voltage (V)", cellCutoffVoltage);
    cellLayout->addRow("Capacity (Ah)", cellCapacity);
    cellLayout->addRow("Internal resistance (Ohm)", internalResistance);
    layout->addWidget(cellGroup);

    cellsInSeries = createDoubleSpin(content, 4, 1, 1000, 0);
    cellsInParallel = createDoubleSpin(content, 2, 1, 1000, 0);
    initialSoc = createDoubleSpin(content, 1.0, 0.01, 1.0, 3);

    auto* packGroup = new QGroupBox("3. Pack Layout Parameters", content);
    auto* packLayout = new QFormLayout(packGroup);
    packLayout->setContentsMargins(14, 16, 14, 14);
    packLayout->setHorizontalSpacing(10);
    packLayout->setVerticalSpacing(8);
    packLayout->addRow("Cells in series", cellsInSeries);
    packLayout->addRow("Cells in parallel", cellsInParallel);
    packLayout->addRow("Initial SOC", initialSoc);
    layout->addWidget(packGroup);

    ambientTemp = createDoubleSpin(content, 25.0, -100.0, 200.0, 2);
    dischargeCurrent = createDoubleSpin(content, 5.0, 0.0, 5000.0, 3);
    packMass = createDoubleSpin(content, 1.0, 0.01, 10000.0, 3);
    packHeatCapacity = createDoubleSpin(content, 900.0, 1.0, 10000.0, 2);
    coolingCoeff = createDoubleSpin(content, 1.0, 0.0, 10000.0, 3);

    auto* thermalGroup = new QGroupBox("4. Thermal Assumptions", content);
    auto* thermalLayout = new QFormLayout(thermalGroup);
    thermalLayout->setContentsMargins(14, 16, 14, 14);
    thermalLayout->setHorizontalSpacing(10);
    thermalLayout->setVerticalSpacing(8);
    thermalLayout->addRow("Ambient temp (C)", ambientTemp);
    thermalLayout->addRow("Discharge current (A)", dischargeCurrent);
    thermalLayout->addRow("Pack mass (kg)", packMass);
    thermalLayout->addRow("Heat capacity (J/kgK)", packHeatCapacity);
    thermalLayout->addRow("Cooling coeff (W/K)", coolingCoeff);
    layout->addWidget(thermalGroup);

    duration = createDoubleSpin(content, 1200, 1, 1000000, 0);
    timeStep = createDoubleSpin(content, 1, 1, 3600, 0);

    auto* horizonGroup = new QGroupBox("5. Run Horizon", content);
    auto* horizonLayout = new QFormLayout(horizonGroup);
    horizonLayout->setContentsMargins(14, 16, 14, 14);
    horizonLayout->setHorizontalSpacing(10);
    horizonLayout->setVerticalSpacing(8);
    horizonLayout->addRow("Duration (s)", duration);
    horizonLayout->addRow("Time step (s)", timeStep);
    layout->addWidget(horizonGroup);

    updateLayoutButton = new QPushButton("Generate / Update Layout", content);
    runButton = new QPushButton("Run Candidate Study", content);
    captureBaselineButton = new QPushButton("Capture Baseline", content);
    compareBaselineButton = new QPushButton("Compare to Baseline", content);
    exportReportButton = new QPushButton("Export Report", content);
    saveProjectButton = new QPushButton("Save Project", content);
    loadProjectButton = new QPushButton("Load Project", content);

    auto* layoutActionsGroup = new QGroupBox("6. Layout & Baseline", content);
    auto* layoutActions = new QGridLayout(layoutActionsGroup);
    layoutActions->setContentsMargins(14, 16, 14, 14);
    layoutActions->setHorizontalSpacing(8);
    layoutActions->setVerticalSpacing(8);
    layoutActions->addWidget(updateLayoutButton, 0, 0, 1, 2);
    layoutActions->addWidget(runButton, 1, 0, 1, 2);
    layoutActions->addWidget(captureBaselineButton, 2, 0);
    layoutActions->addWidget(compareBaselineButton, 2, 1);
    layout->addWidget(layoutActionsGroup);

    auto* projectActionsGroup = new QGroupBox("7. Report & Project", content);
    auto* projectActions = new QGridLayout(projectActionsGroup);
    projectActions->setContentsMargins(14, 16, 14, 14);
    projectActions->setHorizontalSpacing(8);
    projectActions->setVerticalSpacing(8);
    projectActions->addWidget(exportReportButton, 0, 0, 1, 2);
    projectActions->addWidget(saveProjectButton, 1, 0);
    projectActions->addWidget(loadProjectButton, 1, 1);
    layout->addWidget(projectActionsGroup);

    layout->addStretch(1);

    scroll->setWidget(content);
    shellLayout->addWidget(scroll);
}
