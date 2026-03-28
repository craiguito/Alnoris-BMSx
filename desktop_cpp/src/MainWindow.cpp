#include "MainWindow.h"
#include "SimulationMappingBuilder.h"

#include <QComboBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QFrame>
#include <QFile>
#include <QScrollArea>
#include <QStringList>
#include <QTabWidget>
#include <QToolBar>
#include <QHBoxLayout>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <functional>
#include <string>

MainWindow::MainWindow(QString projectRoot, QWidget* parent)
    : QMainWindow(parent)
    , m_client(std::move(projectRoot))
{
    setWindowTitle("Alnoris Battery Simulator");
    resize(1560, 940);
    createMainToolbar();

    auto* fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction("Load Project", this, &MainWindow::loadProject);
    fileMenu->addAction("Save Project", this, &MainWindow::saveProject);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu("Edit");
    editMenu->addAction("Capture Baseline", this, &MainWindow::captureBaseline);

    auto* viewMenu = menuBar()->addMenu("View");
    viewMenu->addAction("Compare to Baseline", this, &MainWindow::compareAgainstBaseline);

    auto* optionsMenu = menuBar()->addMenu("Options");
    optionsMenu->addAction("Run Simulation", this, &MainWindow::runSimulation);
    optionsMenu->addAction("Customization...", this, &MainWindow::openCustomizationDialog);

    auto* simulationMenu = menuBar()->addMenu("Simulation");
    simulationMenu->addAction("Run", this, &MainWindow::runSimulation);
    simulationMenu->addAction("Capture Baseline", this, &MainWindow::captureBaseline);
    simulationMenu->addAction("Compare", this, &MainWindow::compareAgainstBaseline);

    auto* central = new QWidget(this);
    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(10);

    m_referencePreset = new QComboBox(central);
    m_referencePreset->addItem("Custom");
    m_referencePreset->addItem("Panasonic NCR18650B");
    m_referencePreset->addItem("Samsung INR18650-30Q");
    m_referencePreset->addItem("A123 ANR26650M1-B");
    connect(m_referencePreset, &QComboBox::currentIndexChanged, this, &MainWindow::applyReferencePreset);
    connect(m_referencePreset, &QComboBox::currentTextChanged, this, [this](const QString&) { updateCadWorkspace(); });

    m_cellNominalVoltage = createDoubleSpin(3.6, 0.1, 100.0, 3);
    m_cellFullVoltage = createDoubleSpin(4.2, 0.1, 100.0, 3);
    m_cellEmptyVoltage = createDoubleSpin(3.0, 0.0, 100.0, 3);
    m_cellCutoffVoltage = createDoubleSpin(3.0, 0.0, 100.0, 3);
    m_cellCapacity = createDoubleSpin(3.35, 0.1, 1000.0, 3);
    m_cellsInSeries = createDoubleSpin(4, 1, 1000, 0);
    m_cellsInParallel = createDoubleSpin(2, 1, 1000, 0);
    m_internalResistance = createDoubleSpin(0.035, 0.0, 10.0, 4);
    m_ambientTemp = createDoubleSpin(25.0, -100.0, 200.0, 2);
    m_dischargeCurrent = createDoubleSpin(5.0, 0.0, 5000.0, 3);
    m_duration = createDoubleSpin(1200, 1, 1000000, 0);
    m_timeStep = createDoubleSpin(1, 1, 3600, 0);
    m_initialSoc = createDoubleSpin(1.0, 0.01, 1.0, 3);
    m_packMass = createDoubleSpin(1.0, 0.01, 10000.0, 3);
    m_packHeatCapacity = createDoubleSpin(900.0, 1.0, 10000.0, 2);
    m_coolingCoeff = createDoubleSpin(1.0, 0.0, 10000.0, 3);
    const auto bindCadRefresh = [this](QDoubleSpinBox* spinBox) {
        connect(
            spinBox,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            [this](double) { updateCadWorkspace(); }
        );
    };
    bindCadRefresh(m_cellNominalVoltage);
    bindCadRefresh(m_cellFullVoltage);
    bindCadRefresh(m_cellEmptyVoltage);
    bindCadRefresh(m_cellCutoffVoltage);
    bindCadRefresh(m_cellCapacity);
    bindCadRefresh(m_cellsInSeries);
    bindCadRefresh(m_cellsInParallel);
    bindCadRefresh(m_internalResistance);
    bindCadRefresh(m_ambientTemp);
    bindCadRefresh(m_dischargeCurrent);
    bindCadRefresh(m_duration);
    bindCadRefresh(m_timeStep);
    bindCadRefresh(m_initialSoc);
    bindCadRefresh(m_packMass);
    bindCadRefresh(m_packHeatCapacity);
    bindCadRefresh(m_coolingCoeff);

    m_runButton = new QPushButton("Run Simulation", central);
    m_captureBaselineButton = new QPushButton("Capture Baseline", central);
    m_compareBaselineButton = new QPushButton("Compare to Baseline", central);
    m_saveProjectButton = new QPushButton("Save Project", central);
    m_loadProjectButton = new QPushButton("Load Project", central);
    connect(m_runButton, &QPushButton::clicked, this, &MainWindow::runSimulation);
    connect(m_captureBaselineButton, &QPushButton::clicked, this, &MainWindow::captureBaseline);
    connect(m_compareBaselineButton, &QPushButton::clicked, this, &MainWindow::compareAgainstBaseline);
    connect(m_saveProjectButton, &QPushButton::clicked, this, &MainWindow::saveProject);
    connect(m_loadProjectButton, &QPushButton::clicked, this, &MainWindow::loadProject);

    auto* mainSplitter = new QSplitter(Qt::Horizontal, central);
    mainSplitter->setChildrenCollapsible(false);

    auto* centerSplitter = new QSplitter(Qt::Vertical, mainSplitter);
    centerSplitter->setChildrenCollapsible(false);

    auto* workspaceGroup = new QGroupBox("", centerSplitter);
    auto* workspaceLayout = new QVBoxLayout(workspaceGroup);
    workspaceLayout->setContentsMargins(12, 12, 12, 12);
    workspaceLayout->setSpacing(8);
    auto* workspaceHeader = new QLabel("CAD / 3D Simulation Workspace", workspaceGroup);
    workspaceHeader->setStyleSheet("font-size:16px; font-weight:700; color:#f3f7fb;");
    auto* workspaceSubheader = new QLabel("Primary battery design view. Select entities to inspect and edit them from the sidebar.", workspaceGroup);
    workspaceSubheader->setWordWrap(true);
    workspaceSubheader->setStyleSheet("font-size:12px; color:#93a6ba;");
    workspaceLayout->addWidget(workspaceHeader);
    workspaceLayout->addWidget(workspaceSubheader);
    workspaceLayout->addWidget(createWorkspacePanel(), 1);

    auto* outputGroup = createOutputPanel();

    centerSplitter->addWidget(workspaceGroup);
    centerSplitter->addWidget(outputGroup);
    centerSplitter->setStretchFactor(0, 8);
    centerSplitter->setStretchFactor(1, 2);

    auto* rightTabs = new QTabWidget(mainSplitter);
    rightTabs->setMinimumWidth(360);
    rightTabs->addTab(createCadPropertiesPanel(), "Inspector");
    rightTabs->addTab(createResultsPanel(), "Results");

    mainSplitter->addWidget(createInputsSidebar());
    mainSplitter->addWidget(centerSplitter);
    mainSplitter->addWidget(rightTabs);
    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 7);
    mainSplitter->setStretchFactor(2, 3);

    rootLayout->addWidget(mainSplitter, 1);

    setCentralWidget(central);
    applyTheme();
    updateCadWorkspace();
    refreshCadProperties();
}

void MainWindow::runSimulation()
{
    const SimulationClient::Result result = m_client.runSimulation(buildSimulationConfig());
    if (!result.ok) {
        QMessageBox::critical(this, "Simulation Error", result.error);
        return;
    }

    renderResult(result.payload);
}

void MainWindow::captureBaseline()
{
    m_baselineConfig = buildSimulationConfig();
    const SimulationClient::Result result = m_client.runSimulation(m_baselineConfig);
    if (!result.ok) {
        QMessageBox::critical(this, "Simulation Error", result.error);
        return;
    }

    m_baselineResult = result.payload;
    m_summaryLabel->setText("Baseline captured. You can now tweak parameters and use Compare to Baseline.");
    const QJsonArray timeSeries = m_baselineResult.value("time_series").toArray();
    populateChart(m_voltageChartView, timeSeries, "terminal_voltage_v", "Voltage vs Time", "Voltage (V)");
    populateChart(m_temperatureChartView, timeSeries, "temp_c", "Temperature vs Time", "Temperature (C)");
    populateChart(m_socChartView, timeSeries, "soc", "SOC vs Time", "SOC");
    populateChart(m_powerChartView, timeSeries, "power_w", "Power vs Time", "Power (W)");
    m_outputText->setPlainText(formatSummaryLines(m_baselineResult) + "\n\n" + formatTraceLines(m_baselineResult));
}

void MainWindow::compareAgainstBaseline()
{
    if (m_baselineConfig.isEmpty() || m_baselineResult.isEmpty()) {
        QMessageBox::information(this, "No Baseline", "Capture a baseline simulation before comparing.");
        return;
    }

    const SimulationClient::Result candidate = m_client.runSimulation(buildSimulationConfig());
    if (!candidate.ok) {
        QMessageBox::critical(this, "Simulation Error", candidate.error);
        return;
    }

    renderComparison(m_baselineResult, candidate.payload);
}

void MainWindow::saveProject()
{
    const QString path = QFileDialog::getSaveFileName(
        this,
        "Save Project",
        QString(),
        "Alnoris Project (*.json)"
    );
    if (path.isEmpty()) {
        return;
    }

    QJsonObject root;
    root.insert("reference_preset_index", m_referencePreset->currentIndex());
    root.insert("simulation_config", buildSimulationConfig());
    root.insert("baseline_config", m_baselineConfig);
    root.insert("baseline_result", m_baselineResult);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, "Save Failed", "Could not write the selected project file.");
        return;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    m_summaryLabel->setText(QString("Saved project to %1").arg(path));
}

void MainWindow::loadProject()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Load Project",
        QString(),
        "Alnoris Project (*.json)"
    );
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Load Failed", "Could not read the selected project file.");
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        QMessageBox::critical(this, "Load Failed", "The selected file is not a valid project document.");
        return;
    }

    const QJsonObject root = document.object();
    applySimulationConfig(root.value("simulation_config").toObject());
    m_baselineConfig = root.value("baseline_config").toObject();
    m_baselineResult = root.value("baseline_result").toObject();
    m_referencePreset->setCurrentIndex(root.value("reference_preset_index").toInt(0));

    if (!m_baselineResult.isEmpty()) {
        m_summaryLabel->setText(QString("Loaded project from %1 with a saved baseline.").arg(path));
        m_outputText->setPlainText(formatSummaryLines(m_baselineResult) + "\n\n" + formatTraceLines(m_baselineResult));
    } else {
        m_summaryLabel->setText(QString("Loaded project from %1.").arg(path));
    }
}

void MainWindow::applyReferencePreset(int index)
{
    switch (index) {
    case 1:
        m_cellNominalVoltage->setValue(3.6);
        m_cellFullVoltage->setValue(4.2);
        m_cellEmptyVoltage->setValue(3.0);
        m_cellCutoffVoltage->setValue(3.0);
        m_cellCapacity->setValue(3.35);
        m_internalResistance->setValue(0.035);
        m_dischargeCurrent->setValue(1.675);
        m_packMass->setValue(0.048);
        m_packHeatCapacity->setValue(900.0);
        m_coolingCoeff->setValue(0.35);
        break;
    case 2:
        m_cellNominalVoltage->setValue(3.6);
        m_cellFullVoltage->setValue(4.2);
        m_cellEmptyVoltage->setValue(2.5);
        m_cellCutoffVoltage->setValue(2.5);
        m_cellCapacity->setValue(3.0);
        m_internalResistance->setValue(0.02);
        m_dischargeCurrent->setValue(3.0);
        m_packMass->setValue(0.046);
        m_packHeatCapacity->setValue(900.0);
        m_coolingCoeff->setValue(0.45);
        break;
    case 3:
        m_cellNominalVoltage->setValue(3.3);
        m_cellFullVoltage->setValue(3.6);
        m_cellEmptyVoltage->setValue(2.0);
        m_cellCutoffVoltage->setValue(2.0);
        m_cellCapacity->setValue(2.5);
        m_internalResistance->setValue(0.006);
        m_dischargeCurrent->setValue(7.5);
        m_packMass->setValue(0.076);
        m_packHeatCapacity->setValue(950.0);
        m_coolingCoeff->setValue(0.55);
        break;
    default:
        break;
    }
}

QDoubleSpinBox* MainWindow::createDoubleSpin(double value, double min, double max, int decimals)
{
    auto* widget = new QDoubleSpinBox(this);
    widget->setRange(min, max);
    widget->setDecimals(decimals);
    widget->setValue(value);
    return widget;
}

void MainWindow::createMainToolbar()
{
    auto* toolbar = addToolBar("Primary");
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addAction("Load", this, &MainWindow::loadProject);
    toolbar->addAction("Save", this, &MainWindow::saveProject);
    toolbar->addSeparator();
    toolbar->addAction("CAD Undo", this, &MainWindow::undoCadEdit);
    toolbar->addAction("CAD Redo", this, &MainWindow::redoCadEdit);
    toolbar->addSeparator();
    toolbar->addAction("Run", this, &MainWindow::runSimulation);
    toolbar->addAction("Baseline", this, &MainWindow::captureBaseline);
    toolbar->addAction("Compare", this, &MainWindow::compareAgainstBaseline);
}

QWidget* MainWindow::createInputsSidebar()
{
    auto* container = new QWidget(this);
    auto* shellLayout = new QVBoxLayout(container);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    auto* scroll = new QScrollArea(container);
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
    auto* header = new QLabel("Simulation Setup", headerGroup);
    header->setStyleSheet("font-size:16px; font-weight:700; color:#f3f7fb;");
    auto* subheader = new QLabel("Configure cell, pack, environment, and run settings for the active battery scenario.", headerGroup);
    subheader->setWordWrap(true);
    subheader->setStyleSheet("font-size:12px; color:#93a6ba;");
    headerLayout->addWidget(header);
    headerLayout->addWidget(subheader);
    layout->addWidget(headerGroup);

    auto* cellGroup = new QGroupBox("Cell", content);
    auto* cellLayout = new QFormLayout(cellGroup);
    cellLayout->setContentsMargins(14, 16, 14, 14);
    cellLayout->setHorizontalSpacing(10);
    cellLayout->setVerticalSpacing(8);
    cellLayout->addRow("Reference preset", m_referencePreset);
    cellLayout->addRow("Nominal voltage (V)", m_cellNominalVoltage);
    cellLayout->addRow("Full voltage (V)", m_cellFullVoltage);
    cellLayout->addRow("Empty voltage (V)", m_cellEmptyVoltage);
    cellLayout->addRow("Cutoff voltage (V)", m_cellCutoffVoltage);
    cellLayout->addRow("Capacity (Ah)", m_cellCapacity);
    cellLayout->addRow("Internal resistance (Ohm)", m_internalResistance);
    layout->addWidget(cellGroup);

    auto* packGroup = new QGroupBox("Pack Layout", content);
    auto* packLayout = new QFormLayout(packGroup);
    packLayout->setContentsMargins(14, 16, 14, 14);
    packLayout->setHorizontalSpacing(10);
    packLayout->setVerticalSpacing(8);
    packLayout->addRow("Cells in series", m_cellsInSeries);
    packLayout->addRow("Cells in parallel", m_cellsInParallel);
    packLayout->addRow("Initial SOC", m_initialSoc);
    layout->addWidget(packGroup);

    auto* thermalGroup = new QGroupBox("Electrical / Thermal", content);
    auto* thermalLayout = new QFormLayout(thermalGroup);
    thermalLayout->setContentsMargins(14, 16, 14, 14);
    thermalLayout->setHorizontalSpacing(10);
    thermalLayout->setVerticalSpacing(8);
    thermalLayout->addRow("Ambient temp (C)", m_ambientTemp);
    thermalLayout->addRow("Discharge current (A)", m_dischargeCurrent);
    thermalLayout->addRow("Pack mass (kg)", m_packMass);
    thermalLayout->addRow("Heat capacity (J/kgK)", m_packHeatCapacity);
    thermalLayout->addRow("Cooling coeff (W/K)", m_coolingCoeff);
    layout->addWidget(thermalGroup);

    auto* simGroup = new QGroupBox("Simulation Run", content);
    auto* simLayout = new QFormLayout(simGroup);
    simLayout->setContentsMargins(14, 16, 14, 14);
    simLayout->setHorizontalSpacing(10);
    simLayout->setVerticalSpacing(8);
    simLayout->addRow("Duration (s)", m_duration);
    simLayout->addRow("Time step (s)", m_timeStep);
    layout->addWidget(simGroup);

    auto* actionsGroup = new QGroupBox("Actions", content);
    auto* actionsLayout = new QGridLayout(actionsGroup);
    actionsLayout->setContentsMargins(14, 16, 14, 14);
    actionsLayout->setHorizontalSpacing(8);
    actionsLayout->setVerticalSpacing(8);
    actionsLayout->addWidget(m_runButton, 0, 0, 1, 2);
    actionsLayout->addWidget(m_captureBaselineButton, 1, 0);
    actionsLayout->addWidget(m_compareBaselineButton, 1, 1);
    actionsLayout->addWidget(m_saveProjectButton, 2, 0);
    actionsLayout->addWidget(m_loadProjectButton, 2, 1);
    layout->addWidget(actionsGroup);
    layout->addStretch(1);

    scroll->setWidget(content);
    shellLayout->addWidget(scroll);
    return container;
}

QJsonObject MainWindow::buildSimulationConfig() const
{
    QJsonObject config{
        {"cell_nominal_voltage", m_cellNominalVoltage->value()},
        {"cell_full_voltage", m_cellFullVoltage->value()},
        {"cell_empty_voltage", m_cellEmptyVoltage->value()},
        {"cell_cutoff_voltage", m_cellCutoffVoltage->value()},
        {"cell_capacity_ah", m_cellCapacity->value()},
        {"cells_in_series", static_cast<int>(m_cellsInSeries->value())},
        {"cells_in_parallel", static_cast<int>(m_cellsInParallel->value())},
        {"internal_resistance_ohm_per_cell", m_internalResistance->value()},
        {"ambient_temp_c", m_ambientTemp->value()},
        {"discharge_current_a", m_dischargeCurrent->value()},
        {"duration_s", static_cast<int>(m_duration->value())},
        {"time_step_s", static_cast<int>(m_timeStep->value())},
        {"initial_soc", m_initialSoc->value()},
        {"pack_mass_kg", m_packMass->value()},
        {"pack_heat_capacity_j_per_kgk", m_packHeatCapacity->value()},
        {"cooling_coeff_w_per_k", m_coolingCoeff->value()}
    };

    const int fallbackGroupCount = std::max(1, static_cast<int>(m_cellsInSeries->value()));
    config.insert("group_count", fallbackGroupCount);

    if (m_cadWorkspaceView != nullptr) {
        const auto mapping = SimulationMappingBuilder::build(
            m_cadWorkspaceView->document(),
            m_ambientTemp->value(),
            m_coolingCoeff->value()
        );
        config.insert("group_count", mapping.groupCount);
        config.insert("thermal_zones", mapping.thermalZones);
        config.insert("group_zone_assignments", mapping.groupZoneAssignments);
        config.insert("group_labels", mapping.groupLabels);
        config.insert("group_entity_ids", mapping.groupEntityIds);
    }

    return config;
}

void MainWindow::renderResult(const QJsonObject& payload)
{
    m_summaryLabel->setText("Single scenario results from the desktop shell. Capture a baseline to compare how design changes shift performance.");
    const QJsonArray timeSeries = payload.value("time_series").toArray();
    populateChart(m_voltageChartView, timeSeries, "terminal_voltage_v", "Voltage vs Time", "Voltage (V)");
    populateChart(m_temperatureChartView, timeSeries, "temp_c", "Temperature vs Time", "Temperature (C)");
    populateChart(m_socChartView, timeSeries, "soc", "SOC vs Time", "SOC");
    populateChart(m_powerChartView, timeSeries, "power_w", "Power vs Time", "Power (W)");
    m_outputText->setPlainText(formatSummaryLines(payload) + "\n\n" + formatTraceLines(payload));
}

void MainWindow::renderComparison(const QJsonObject& baselinePayload, const QJsonObject& candidatePayload)
{
    const QJsonObject baseSummary = baselinePayload.value("summary").toObject();
    const QJsonObject candidateSummary = candidatePayload.value("summary").toObject();

    const double runtimeDelta = candidateSummary.value("runtime_s").toDouble() - baseSummary.value("runtime_s").toDouble();
    const double energyDelta = candidateSummary.value("delivered_energy_wh").toDouble() - baseSummary.value("delivered_energy_wh").toDouble();
    const double tempDelta = candidateSummary.value("peak_temp_c").toDouble() - baseSummary.value("peak_temp_c").toDouble();
    const double voltageDelta = candidateSummary.value("min_terminal_voltage_v").toDouble() - baseSummary.value("min_terminal_voltage_v").toDouble();

    QStringList lines;
    lines << "Baseline Summary";
    lines << formatSummaryLines(baselinePayload);
    lines << "";
    lines << "Candidate Summary";
    lines << formatSummaryLines(candidatePayload);
    lines << "";
    lines << "Comparison Deltas";
    lines << QString("Runtime delta: %1 s").arg(runtimeDelta, 0, 'f', 1);
    lines << QString("Delivered energy delta: %1 Wh").arg(energyDelta, 0, 'f', 2);
    lines << QString("Peak temperature delta: %1 C").arg(tempDelta, 0, 'f', 2);
    lines << QString("Minimum voltage delta: %1 V").arg(voltageDelta, 0, 'f', 2);
    lines << "";
    lines << "Candidate Trace";
    lines << formatTraceLines(candidatePayload);

    m_summaryLabel->setText("Scenario comparison view. Positive energy/runtime deltas are usually good; negative peak-temperature deltas are usually good.");
    populateComparisonChart(
        m_voltageChartView,
        baselinePayload.value("time_series").toArray(),
        candidatePayload.value("time_series").toArray(),
        "terminal_voltage_v",
        "Voltage Comparison",
        "Voltage (V)"
    );
    populateComparisonChart(
        m_temperatureChartView,
        baselinePayload.value("time_series").toArray(),
        candidatePayload.value("time_series").toArray(),
        "temp_c",
        "Temperature Comparison",
        "Temperature (C)"
    );
    populateComparisonChart(
        m_socChartView,
        baselinePayload.value("time_series").toArray(),
        candidatePayload.value("time_series").toArray(),
        "soc",
        "SOC Comparison",
        "SOC"
    );
    populateComparisonChart(
        m_powerChartView,
        baselinePayload.value("time_series").toArray(),
        candidatePayload.value("time_series").toArray(),
        "power_w",
        "Power Comparison",
        "Power (W)"
    );
    m_outputText->setPlainText(lines.join('\n'));
}

void MainWindow::applySimulationConfig(const QJsonObject& config)
{
    if (config.isEmpty()) {
        return;
    }

    m_cellNominalVoltage->setValue(config.value("cell_nominal_voltage").toDouble(m_cellNominalVoltage->value()));
    m_cellFullVoltage->setValue(config.value("cell_full_voltage").toDouble(m_cellFullVoltage->value()));
    m_cellEmptyVoltage->setValue(config.value("cell_empty_voltage").toDouble(m_cellEmptyVoltage->value()));
    m_cellCutoffVoltage->setValue(config.value("cell_cutoff_voltage").toDouble(m_cellCutoffVoltage->value()));
    m_cellCapacity->setValue(config.value("cell_capacity_ah").toDouble(m_cellCapacity->value()));
    m_cellsInSeries->setValue(config.value("cells_in_series").toInt(static_cast<int>(m_cellsInSeries->value())));
    m_cellsInParallel->setValue(config.value("cells_in_parallel").toInt(static_cast<int>(m_cellsInParallel->value())));
    m_internalResistance->setValue(config.value("internal_resistance_ohm_per_cell").toDouble(m_internalResistance->value()));
    m_ambientTemp->setValue(config.value("ambient_temp_c").toDouble(m_ambientTemp->value()));
    m_dischargeCurrent->setValue(config.value("discharge_current_a").toDouble(m_dischargeCurrent->value()));
    m_duration->setValue(config.value("duration_s").toInt(static_cast<int>(m_duration->value())));
    m_timeStep->setValue(config.value("time_step_s").toInt(static_cast<int>(m_timeStep->value())));
    m_initialSoc->setValue(config.value("initial_soc").toDouble(m_initialSoc->value()));
    m_packMass->setValue(config.value("pack_mass_kg").toDouble(m_packMass->value()));
    m_packHeatCapacity->setValue(config.value("pack_heat_capacity_j_per_kgk").toDouble(m_packHeatCapacity->value()));
    m_coolingCoeff->setValue(config.value("cooling_coeff_w_per_k").toDouble(m_coolingCoeff->value()));
}

QString MainWindow::formatSummaryLines(const QJsonObject& payload) const
{
    const QJsonObject summary = payload.value("summary").toObject();
    const QJsonArray warnings = summary.value("warnings").toArray();

    QStringList lines;
    lines << QString("Pack nominal voltage: %1 V").arg(payload.value("pack_nominal_voltage_v").toDouble(), 0, 'f', 2);
    lines << QString("Pack capacity: %1 Ah").arg(payload.value("pack_capacity_ah").toDouble(), 0, 'f', 2);
    lines << QString("Theoretical energy: %1 Wh").arg(payload.value("theoretical_energy_wh").toDouble(), 0, 'f', 2);
    lines << QString("Delivered energy: %1 Wh").arg(summary.value("delivered_energy_wh").toDouble(), 0, 'f', 2);
    lines << QString("Delivered capacity: %1 Ah").arg(summary.value("delivered_capacity_ah").toDouble(), 0, 'f', 3);
    lines << QString("Peak temperature: %1 C").arg(summary.value("peak_temp_c").toDouble(), 0, 'f', 2);
    lines << QString("Minimum voltage: %1 V").arg(summary.value("min_terminal_voltage_v").toDouble(), 0, 'f', 2);
    lines << QString("Final SOC: %1").arg(summary.value("final_soc").toDouble(), 0, 'f', 3);
    lines << QString("Runtime: %1 s").arg(summary.value("runtime_s").toInt());
    lines << QString("Termination: %1").arg(summary.value("termination_reason").toString());

    if (!warnings.isEmpty()) {
        lines << "Warnings:";
        for (const QJsonValue& warningValue : warnings) {
            const QJsonObject warning = warningValue.toObject();
            lines << QString("[%1] %2")
                         .arg(warning.value("severity").toString().toUpper())
                         .arg(warning.value("message").toString());
        }
    }

    return lines.join('\n');
}

QString MainWindow::formatTraceLines(const QJsonObject& payload) const
{
    const QJsonArray timeSeries = payload.value("time_series").toArray();
    QStringList lines;
    for (const QJsonValue& pointValue : timeSeries) {
        const QJsonObject point = pointValue.toObject();
        lines << QString("t=%1s  soc=%2  V=%3  P=%4W  T=%5C")
                     .arg(point.value("time_s").toInt(), 5)
                     .arg(point.value("soc").toDouble(), 0, 'f', 3)
                     .arg(point.value("terminal_voltage_v").toDouble(), 0, 'f', 3)
                     .arg(point.value("power_w").toDouble(), 0, 'f', 3)
                     .arg(point.value("temp_c").toDouble(), 0, 'f', 3);
    }
    return lines.join('\n');
}

ChartWidget* MainWindow::createGraphWidget()
{
    auto* view = new ChartWidget(this);
    view->setMinimumHeight(240);
    applyChartTheme(view);
    return view;
}

QFrame* MainWindow::createWorkspacePanel()
{
    auto* frame = new QFrame(this);
    frame->setObjectName("workspacePanel");
    frame->setStyleSheet(
        "#workspacePanel {"
        "background:#e9edf2;"
        "border:1px solid #48515b;"
        "border-radius:12px;"
        "}"
    );

    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_cadWorkspaceView = new CadViewportWidget(frame);
    m_cadWorkspaceView->setCellMeshPath(QStringLiteral(ALNORIS_DEFAULT_CELL_STL));
    m_cadWorkspaceView->setBackgroundColor(m_theme.cadBackground);
    connect(m_cadWorkspaceView, &CadViewportWidget::selectionChanged, this, &MainWindow::refreshCadProperties);
    layout->addWidget(m_cadWorkspaceView, 1);

    return frame;
}

QGroupBox* MainWindow::createCadPropertiesPanel()
{
    auto* group = new QGroupBox("", this);
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    auto* header = new QLabel("CAD Inspector", group);
    header->setStyleSheet("font-size:16px; font-weight:700; color:#f3f7fb;");
    layout->addWidget(header);

    auto* subheader = new QLabel("Review the selected entity, edit its transform or geometry, then apply or undo changes.", group);
    subheader->setWordWrap(true);
    subheader->setStyleSheet("font-size:12px; color:#93a6ba;");
    layout->addWidget(subheader);

    m_cadSelectedType = new QLabel("No selection", group);
    m_cadSelectedId = new QLabel("-", group);
    m_cadLabelEdit = new QLineEdit(group);
    m_cadVisibleCheck = new QCheckBox("Visible", group);

    m_cadPosXLabel = new QLabel("Pos X", group);
    m_cadPosYLabel = new QLabel("Pos Y", group);
    m_cadPosZLabel = new QLabel("Pos Z", group);
    m_cadPosX = createDoubleSpin(0.0, -10000.0, 10000.0, 2);
    m_cadPosY = createDoubleSpin(0.0, -10000.0, 10000.0, 2);
    m_cadPosZ = createDoubleSpin(0.0, -10000.0, 10000.0, 2);

    m_cadRadiusLabel = new QLabel("Radius", group);
    m_cadHeightLabel = new QLabel("Height", group);
    m_cadRadius = createDoubleSpin(0.0, 0.0, 10000.0, 2);
    m_cadHeight = createDoubleSpin(0.0, 0.0, 10000.0, 2);

    m_cadSizeXLabel = new QLabel("Size X", group);
    m_cadSizeYLabel = new QLabel("Size Y", group);
    m_cadSizeZLabel = new QLabel("Size Z", group);
    m_cadSizeX = createDoubleSpin(0.0, 0.0, 10000.0, 2);
    m_cadSizeY = createDoubleSpin(0.0, 0.0, 10000.0, 2);
    m_cadSizeZ = createDoubleSpin(0.0, 0.0, 10000.0, 2);
    m_cadThicknessLabel = new QLabel("Wall Thickness", group);
    m_cadThickness = createDoubleSpin(0.0, 0.0, 10000.0, 2);

    auto* selectionGroup = new QGroupBox("Selection Summary", group);
    auto* selectionLayout = new QFormLayout(selectionGroup);
    selectionLayout->setContentsMargins(14, 16, 14, 14);
    selectionLayout->setHorizontalSpacing(10);
    selectionLayout->setVerticalSpacing(8);
    selectionLayout->addRow("Type", m_cadSelectedType);
    selectionLayout->addRow("Entity ID", m_cadSelectedId);
    selectionLayout->addRow("Label", m_cadLabelEdit);
    selectionLayout->addRow(QString(), m_cadVisibleCheck);
    layout->addWidget(selectionGroup);

    auto* transformGroup = new QGroupBox("Transform", group);
    auto* transformLayout = new QFormLayout(transformGroup);
    transformLayout->setContentsMargins(14, 16, 14, 14);
    transformLayout->setHorizontalSpacing(10);
    transformLayout->setVerticalSpacing(8);
    transformLayout->addRow(m_cadPosXLabel, m_cadPosX);
    transformLayout->addRow(m_cadPosYLabel, m_cadPosY);
    transformLayout->addRow(m_cadPosZLabel, m_cadPosZ);
    layout->addWidget(transformGroup);

    auto* geometryGroup = new QGroupBox("Geometry", group);
    auto* geometryLayout = new QFormLayout(geometryGroup);
    geometryLayout->setContentsMargins(14, 16, 14, 14);
    geometryLayout->setHorizontalSpacing(10);
    geometryLayout->setVerticalSpacing(8);
    geometryLayout->addRow(m_cadRadiusLabel, m_cadRadius);
    geometryLayout->addRow(m_cadHeightLabel, m_cadHeight);
    geometryLayout->addRow(m_cadSizeXLabel, m_cadSizeX);
    geometryLayout->addRow(m_cadSizeYLabel, m_cadSizeY);
    geometryLayout->addRow(m_cadSizeZLabel, m_cadSizeZ);
    geometryLayout->addRow(m_cadThicknessLabel, m_cadThickness);
    layout->addWidget(geometryGroup);

    auto* buttonLayout = new QGridLayout();
    m_cadApplyButton = new QPushButton("Apply", group);
    m_cadUndoButton = new QPushButton("Undo", group);
    m_cadRedoButton = new QPushButton("Redo", group);
    m_cadResetPositionButton = new QPushButton("Reset Position", group);
    m_cadResetGeometryButton = new QPushButton("Reset Geometry", group);
    m_cadResetLabelButton = new QPushButton("Reset Label", group);
    connect(m_cadApplyButton, &QPushButton::clicked, this, &MainWindow::applyCadPropertyChanges);
    connect(m_cadUndoButton, &QPushButton::clicked, this, &MainWindow::undoCadEdit);
    connect(m_cadRedoButton, &QPushButton::clicked, this, &MainWindow::redoCadEdit);
    connect(m_cadResetPositionButton, &QPushButton::clicked, this, &MainWindow::resetCadPosition);
    connect(m_cadResetGeometryButton, &QPushButton::clicked, this, &MainWindow::resetCadGeometry);
    connect(m_cadResetLabelButton, &QPushButton::clicked, this, &MainWindow::resetCadLabel);
    buttonLayout->addWidget(m_cadApplyButton, 0, 0, 1, 2);
    buttonLayout->addWidget(m_cadUndoButton, 1, 0);
    buttonLayout->addWidget(m_cadRedoButton, 1, 1);
    buttonLayout->addWidget(m_cadResetPositionButton, 2, 0);
    buttonLayout->addWidget(m_cadResetGeometryButton, 2, 1);
    buttonLayout->addWidget(m_cadResetLabelButton, 3, 0, 1, 2);
    auto* actionsGroup = new QGroupBox("Actions", group);
    auto* actionsGroupLayout = new QVBoxLayout(actionsGroup);
    actionsGroupLayout->setContentsMargins(14, 16, 14, 14);
    actionsGroupLayout->addLayout(buttonLayout);
    layout->addWidget(actionsGroup);
    layout->addStretch(1);

    return group;
}

QGroupBox* MainWindow::createResultsPanel()
{
    auto* group = new QGroupBox("", this);
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* header = new QLabel("Results & Charts", group);
    header->setStyleSheet("font-size:16px; font-weight:700; color:#f3f7fb;");
    auto* subheader = new QLabel("Keep simulation performance visible without stealing focus from the CAD workspace.", group);
    subheader->setWordWrap(true);
    subheader->setStyleSheet("font-size:12px; color:#93a6ba;");
    layout->addWidget(header);
    layout->addWidget(subheader);

    auto* summaryGroup = new QGroupBox("Run Summary", group);
    auto* summaryLayout = new QVBoxLayout(summaryGroup);
    summaryLayout->setContentsMargins(14, 16, 14, 14);
    m_summaryLabel = new QLabel("Run a simulation to view the desktop-first results. Capture a baseline when you want to compare design changes.", summaryGroup);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setStyleSheet("font-size: 13px; color: #d8e5f2;");
    summaryLayout->addWidget(m_summaryLabel);
    layout->addWidget(summaryGroup);

    auto* chartTabs = new QTabWidget(group);
    auto* electricalTab = new QWidget(chartTabs);
    auto* electricalLayout = new QVBoxLayout(electricalTab);
    electricalLayout->setContentsMargins(8, 8, 8, 8);
    electricalLayout->setSpacing(10);
    m_topChartTabs = new QTabWidget(electricalTab);
    m_voltageChartView = createGraphWidget();
    m_powerChartView = createGraphWidget();
    m_topChartTabs->addTab(m_voltageChartView, "Voltage");
    m_topChartTabs->addTab(m_powerChartView, "Power");
    electricalLayout->addWidget(m_topChartTabs, 1);

    auto* thermalTab = new QWidget(chartTabs);
    auto* thermalLayout = new QVBoxLayout(thermalTab);
    thermalLayout->setContentsMargins(8, 8, 8, 8);
    thermalLayout->setSpacing(10);
    m_bottomChartTabs = new QTabWidget(thermalTab);
    m_temperatureChartView = createGraphWidget();
    m_socChartView = createGraphWidget();
    m_bottomChartTabs->addTab(m_temperatureChartView, "Temperature");
    m_bottomChartTabs->addTab(m_socChartView, "SOC");
    thermalLayout->addWidget(m_bottomChartTabs, 1);

    chartTabs->addTab(electricalTab, "Electrical");
    chartTabs->addTab(thermalTab, "Thermal / State");
    layout->addWidget(chartTabs, 1);

    return group;
}

QGroupBox* MainWindow::createOutputPanel()
{
    auto* outputGroup = new QGroupBox("", this);
    auto* outputLayout = new QVBoxLayout(outputGroup);
    outputLayout->setContentsMargins(12, 12, 12, 12);
    outputLayout->setSpacing(8);
    auto* outputHeader = new QLabel("Logs / Output", outputGroup);
    outputHeader->setStyleSheet("font-size:14px; font-weight:700; color:#f3f7fb;");
    auto* outputHint = new QLabel("Diagnostics stay available here, but the workspace remains the primary focus.", outputGroup);
    outputHint->setWordWrap(true);
    outputHint->setStyleSheet("font-size:12px; color:#93a6ba;");
    m_outputText = new QPlainTextEdit(outputGroup);
    m_outputText->setReadOnly(true);
    m_outputText->setMinimumHeight(110);
    outputLayout->addWidget(outputHeader);
    outputLayout->addWidget(outputHint);
    outputLayout->addWidget(m_outputText, 1);
    return outputGroup;
}

void MainWindow::applyTheme()
{
    const QString appBg = m_theme.appBackground.name();
    const QString text = m_theme.textColor.name();
    const QString panelBg = QString("#262a31");
    const QString fieldBg = QString("#30343c");
    const QString border = QString("#5c6672");
    const QString accent = QString("#3182f6");

    setStyleSheet(QString(
        "QMainWindow, QWidget { background:%1; color:%2; }"
        "QToolBar { background:%3; border:1px solid %4; spacing:6px; padding:4px; }"
        "QToolBar QToolButton { background:%3; color:%2; border:1px solid %4; border-radius:5px; padding:6px 10px; }"
        "QToolBar QToolButton:hover { border:1px solid %5; }"
        "QGroupBox { background:%3; border:1px solid %4; border-radius:8px; margin-top:10px; padding-top:8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left:12px; padding:0 4px; color:%2; font-weight:600; }"
        "QLabel { color:%2; background:transparent; }"
        "QMenuBar { background:%1; color:%2; }"
        "QMenuBar::item:selected { background:%3; }"
        "QMenu { background:%3; color:%2; border:1px solid %4; }"
        "QMenu::item:selected { background:%4; }"
        "QPushButton, QComboBox, QDoubleSpinBox, QLineEdit, QPlainTextEdit, QTabWidget::pane { background:%3; color:%2; border:1px solid %4; border-radius:6px; }"
        "QPushButton:hover, QComboBox:hover, QDoubleSpinBox:hover, QLineEdit:hover { border:1px solid %5; }"
        "QTabWidget::pane { padding:2px; }"
        "QTabBar::tab { background:%3; color:%2; border:1px solid %4; border-top-left-radius:6px; border-top-right-radius:6px; padding:7px 12px; }"
        "QTabBar::tab:selected { background:%4; border-color:%5; }"
        "QPlainTextEdit { background:%3; }"
        "QScrollArea { border:none; background:transparent; }"
    ).arg(appBg, text, fieldBg, border, accent));

    if (m_cadWorkspaceView != nullptr) {
        m_cadWorkspaceView->setBackgroundColor(m_theme.cadBackground);
    }

    applyChartTheme(m_voltageChartView);
    applyChartTheme(m_temperatureChartView);
    applyChartTheme(m_socChartView);
    applyChartTheme(m_powerChartView);
}

void MainWindow::applyChartTheme(ChartWidget* graphWidget)
{
    if (graphWidget == nullptr) {
        return;
    }

    graphWidget->setThemeColors(m_theme.chartBackground, m_theme.textColor);
}

void MainWindow::openCustomizationDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Customization");
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();

    auto makeColorButton = [&](const QString& label, QColor initial, const std::function<void(const QColor&)>& setter) {
        auto* button = new QPushButton(label, &dialog);
        auto refresh = [button](const QColor& color) {
            button->setText(color.name());
            button->setStyleSheet(QString("background:%1; color:%2; border:1px solid #666;")
                                      .arg(color.name(), color.lightness() < 128 ? "#f5f5f5" : "#111111"));
        };
        refresh(initial);
        QObject::connect(button, &QPushButton::clicked, &dialog, [&, initial, setter, refresh]() mutable {
            QColor current = initial;
            const QColor picked = QColorDialog::getColor(current, &dialog, "Select Color");
            if (picked.isValid()) {
                initial = picked;
                setter(picked);
                refresh(picked);
            }
        });
        return button;
    };

    ThemeSettings draft = m_theme;
    form->addRow("Software Background", makeColorButton("Software Background", draft.appBackground, [&](const QColor& c) { draft.appBackground = c; }));
    form->addRow("Text Color", makeColorButton("Text Color", draft.textColor, [&](const QColor& c) { draft.textColor = c; }));
    form->addRow("CAD Background", makeColorButton("CAD Background", draft.cadBackground, [&](const QColor& c) { draft.cadBackground = c; }));
    form->addRow("Charts Background", makeColorButton("Charts Background", draft.chartBackground, [&](const QColor& c) { draft.chartBackground = c; }));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        m_theme = draft;
        applyTheme();
    }
}

void MainWindow::updateCadWorkspace()
{
    if (m_cadWorkspaceView == nullptr) {
        return;
    }

    cad::battery::BatteryCadConfig cadConfig;
    cadConfig.layout.preset_name = m_referencePreset != nullptr ? m_referencePreset->currentText().toStdString() : std::string("Custom");
    cadConfig.layout.cells_in_series = m_cellsInSeries != nullptr ? static_cast<int>(m_cellsInSeries->value()) : 0;
    cadConfig.layout.cells_in_parallel = m_cellsInParallel != nullptr ? static_cast<int>(m_cellsInParallel->value()) : 0;
    cadConfig.electrical.cell_nominal_voltage = m_cellNominalVoltage != nullptr ? m_cellNominalVoltage->value() : 0.0;
    cadConfig.electrical.cell_capacity_ah = m_cellCapacity != nullptr ? m_cellCapacity->value() : 0.0;
    cadConfig.thermal.ambient_temp_c = m_ambientTemp != nullptr ? m_ambientTemp->value() : 0.0;
    cadConfig.electrical.internal_resistance_ohm = m_internalResistance != nullptr ? m_internalResistance->value() : 0.0;
    cadConfig.electrical.discharge_current_a = m_dischargeCurrent != nullptr ? m_dischargeCurrent->value() : 0.0;
    cadConfig.thermal.pack_mass_kg = m_packMass != nullptr ? m_packMass->value() : 0.0;
    cadConfig.thermal.cooling_coeff_w_per_k = m_coolingCoeff != nullptr ? m_coolingCoeff->value() : 0.0;
    cadConfig.electrical.initial_soc = m_initialSoc != nullptr ? m_initialSoc->value() : 0.0;
    cadConfig.metrics.pack_voltage = m_cellNominalVoltage != nullptr && m_cellsInSeries != nullptr
        ? m_cellNominalVoltage->value() * m_cellsInSeries->value()
        : 0.0;
    cadConfig.metrics.pack_capacity_ah = m_cellCapacity != nullptr && m_cellsInParallel != nullptr
        ? m_cellCapacity->value() * m_cellsInParallel->value()
        : 0.0;
    m_cadWorkspaceView->setPackConfig(cadConfig);
}

void MainWindow::setCadEditorEnabled(bool enabled)
{
    QWidget* widgets[] = {
        m_cadLabelEdit,
        m_cadVisibleCheck,
        m_cadPosX,
        m_cadPosY,
        m_cadPosZ,
        m_cadRadius,
        m_cadHeight,
        m_cadSizeX,
        m_cadSizeY,
        m_cadSizeZ,
        m_cadThickness,
        m_cadApplyButton,
        m_cadUndoButton,
        m_cadRedoButton,
        m_cadResetPositionButton,
        m_cadResetGeometryButton,
        m_cadResetLabelButton
    };
    for (QWidget* widget : widgets) {
        if (widget != nullptr) {
            widget->setEnabled(enabled);
        }
    }
}

void MainWindow::refreshCadProperties()
{
    if (m_cadWorkspaceView == nullptr || m_isSyncingCadInspector) {
        return;
    }

    m_isSyncingCadInspector = true;
    const auto summary = m_cadWorkspaceView->selectedEntitySummary();
    const bool hasSelection = summary.has_value();
    setCadEditorEnabled(hasSelection);

    if (!hasSelection) {
        m_cadSelectedType->setText("No selection");
        m_cadSelectedId->setText("-");
        m_cadLabelEdit->setText(QString());
        m_cadVisibleCheck->setChecked(false);
        m_cadPosX->setValue(0.0);
        m_cadPosY->setValue(0.0);
        m_cadPosZ->setValue(0.0);
        m_cadRadius->setValue(0.0);
        m_cadHeight->setValue(0.0);
        m_cadSizeX->setValue(0.0);
        m_cadSizeY->setValue(0.0);
        m_cadSizeZ->setValue(0.0);
        m_cadThickness->setValue(0.0);
        m_cadRadiusLabel->setVisible(false);
        m_cadRadius->setVisible(false);
        m_cadHeightLabel->setVisible(false);
        m_cadHeight->setVisible(false);
        m_cadSizeXLabel->setVisible(false);
        m_cadSizeX->setVisible(false);
        m_cadSizeYLabel->setVisible(false);
        m_cadSizeY->setVisible(false);
        m_cadSizeZLabel->setVisible(false);
        m_cadSizeZ->setVisible(false);
        m_cadThicknessLabel->setVisible(false);
        m_cadThickness->setVisible(false);
        m_isSyncingCadInspector = false;
        return;
    }

    const cad::battery::EntitySummary& entity = *summary;
    m_cadSelectedId->setText(QString::number(static_cast<qulonglong>(entity.id.value)));
    m_cadLabelEdit->setText(QString::fromStdString(entity.label));
    m_cadVisibleCheck->setChecked(entity.visible);

    const auto setRowVisible = [](QWidget* label, QWidget* editor, bool visible) {
        if (label != nullptr) {
            label->setVisible(visible);
        }
        if (editor != nullptr) {
            editor->setVisible(visible);
        }
    };

    setRowVisible(m_cadRadiusLabel, m_cadRadius, false);
    setRowVisible(m_cadHeightLabel, m_cadHeight, false);
    setRowVisible(m_cadSizeXLabel, m_cadSizeX, false);
    setRowVisible(m_cadSizeYLabel, m_cadSizeY, false);
    setRowVisible(m_cadSizeZLabel, m_cadSizeZ, false);
    setRowVisible(m_cadThicknessLabel, m_cadThickness, false);

    switch (entity.kind) {
    case cad::battery::EntityKind::Cell: {
        m_cadSelectedType->setText("Cell");
        const auto properties = m_cadWorkspaceView->selectedCellProperties();
        if (!properties.has_value()) {
            break;
        }
        m_cadPosX->setValue(properties->position.x);
        m_cadPosY->setValue(properties->position.y);
        m_cadPosZ->setValue(properties->position.z);
        m_cadRadius->setValue(properties->radius);
        m_cadHeight->setValue(properties->height);
        setRowVisible(m_cadRadiusLabel, m_cadRadius, true);
        setRowVisible(m_cadHeightLabel, m_cadHeight, true);
        break;
    }
    case cad::battery::EntityKind::Busbar: {
        m_cadSelectedType->setText("Busbar");
        const auto properties = m_cadWorkspaceView->selectedBusbarProperties();
        if (!properties.has_value()) {
            break;
        }
        m_cadPosX->setValue(properties->center.x);
        m_cadPosY->setValue(properties->center.y);
        m_cadPosZ->setValue(properties->center.z);
        m_cadSizeX->setValue(properties->size.x);
        m_cadSizeY->setValue(properties->size.y);
        m_cadSizeZ->setValue(properties->size.z);
        setRowVisible(m_cadSizeXLabel, m_cadSizeX, true);
        setRowVisible(m_cadSizeYLabel, m_cadSizeY, true);
        setRowVisible(m_cadSizeZLabel, m_cadSizeZ, true);
        break;
    }
    case cad::battery::EntityKind::CoolingPlate: {
        m_cadSelectedType->setText("Cooling Plate");
        const auto properties = m_cadWorkspaceView->selectedCoolingPlateProperties();
        if (!properties.has_value()) {
            break;
        }
        m_cadPosX->setValue(properties->center.x);
        m_cadPosY->setValue(properties->center.y);
        m_cadPosZ->setValue(properties->center.z);
        m_cadSizeX->setValue(properties->size.x);
        m_cadSizeY->setValue(properties->size.y);
        m_cadSizeZ->setValue(properties->size.z);
        setRowVisible(m_cadSizeXLabel, m_cadSizeX, true);
        setRowVisible(m_cadSizeYLabel, m_cadSizeY, true);
        setRowVisible(m_cadSizeZLabel, m_cadSizeZ, true);
        break;
    }
    case cad::battery::EntityKind::ModuleBoundary: {
        m_cadSelectedType->setText("Module Boundary");
        const auto properties = m_cadWorkspaceView->selectedModuleBoundaryProperties();
        if (!properties.has_value()) {
            break;
        }
        m_cadPosX->setValue(properties->center.x);
        m_cadPosY->setValue(properties->center.y);
        m_cadPosZ->setValue(properties->center.z);
        m_cadSizeX->setValue(properties->size.x);
        m_cadSizeY->setValue(properties->size.y);
        m_cadSizeZ->setValue(properties->size.z);
        setRowVisible(m_cadSizeXLabel, m_cadSizeX, true);
        setRowVisible(m_cadSizeYLabel, m_cadSizeY, true);
        setRowVisible(m_cadSizeZLabel, m_cadSizeZ, true);
        break;
    }
    case cad::battery::EntityKind::PackEnclosure: {
        m_cadSelectedType->setText("Pack Enclosure");
        const auto properties = m_cadWorkspaceView->selectedEnclosureProperties();
        if (!properties.has_value()) {
            break;
        }
        m_cadPosX->setValue(properties->center.x);
        m_cadPosY->setValue(properties->center.y);
        m_cadPosZ->setValue(properties->center.z);
        m_cadSizeX->setValue(properties->size.x);
        m_cadSizeY->setValue(properties->size.y);
        m_cadSizeZ->setValue(properties->size.z);
        m_cadThickness->setValue(properties->wall_thickness);
        setRowVisible(m_cadSizeXLabel, m_cadSizeX, true);
        setRowVisible(m_cadSizeYLabel, m_cadSizeY, true);
        setRowVisible(m_cadSizeZLabel, m_cadSizeZ, true);
        setRowVisible(m_cadThicknessLabel, m_cadThickness, true);
        break;
    }
    }
    m_isSyncingCadInspector = false;
}

void MainWindow::applyCadPropertyChanges()
{
    if (m_cadWorkspaceView == nullptr) {
        return;
    }

    const auto summary = m_cadWorkspaceView->selectedEntitySummary();
    if (!summary.has_value()) {
        return;
    }

    m_isSyncingCadInspector = true;

    switch (summary->kind) {
    case cad::battery::EntityKind::Cell: {
        const auto properties = m_cadWorkspaceView->selectedCellProperties();
        if (!properties.has_value()) {
            break;
        }
        cad::battery::CellPropertiesUpdate update;
        if (QString::fromStdString(summary->label) != m_cadLabelEdit->text()) {
            update.label = m_cadLabelEdit->text().toStdString();
        }
        if (summary->visible != m_cadVisibleCheck->isChecked()) {
            update.visible = m_cadVisibleCheck->isChecked();
        }
        const cad::math::Vec3 position{
            static_cast<float>(m_cadPosX->value()),
            static_cast<float>(m_cadPosY->value()),
            static_cast<float>(m_cadPosZ->value())
        };
        const float radius = static_cast<float>(m_cadRadius->value());
        const float height = static_cast<float>(m_cadHeight->value());
        if (position.x != properties->position.x || position.y != properties->position.y || position.z != properties->position.z) {
            update.position = position;
        }
        if (radius != properties->radius) {
            update.radius = radius;
        }
        if (height != properties->height) {
            update.height = height;
        }
        if (update.position.has_value() || update.radius.has_value() || update.height.has_value()
            || update.label.has_value() || update.visible.has_value()) {
            m_cadWorkspaceView->applySelectedCellUpdate(update);
        }
        break;
    }
    case cad::battery::EntityKind::Busbar: {
        const auto properties = m_cadWorkspaceView->selectedBusbarProperties();
        if (!properties.has_value()) {
            break;
        }
        cad::battery::BusbarPropertiesUpdate update;
        if (QString::fromStdString(summary->label) != m_cadLabelEdit->text()) {
            update.label = m_cadLabelEdit->text().toStdString();
        }
        if (summary->visible != m_cadVisibleCheck->isChecked()) {
            update.visible = m_cadVisibleCheck->isChecked();
        }
        const cad::math::Vec3 center{
            static_cast<float>(m_cadPosX->value()),
            static_cast<float>(m_cadPosY->value()),
            static_cast<float>(m_cadPosZ->value())
        };
        const cad::math::Vec3 size{
            static_cast<float>(m_cadSizeX->value()),
            static_cast<float>(m_cadSizeY->value()),
            static_cast<float>(m_cadSizeZ->value())
        };
        if (center.x != properties->center.x || center.y != properties->center.y || center.z != properties->center.z) {
            update.center = center;
        }
        if (size.x != properties->size.x || size.y != properties->size.y || size.z != properties->size.z) {
            update.size = size;
        }
        if (update.center.has_value() || update.size.has_value()
            || update.label.has_value() || update.visible.has_value()) {
            m_cadWorkspaceView->applySelectedBusbarUpdate(update);
        }
        break;
    }
    case cad::battery::EntityKind::CoolingPlate: {
        const auto properties = m_cadWorkspaceView->selectedCoolingPlateProperties();
        if (!properties.has_value()) {
            break;
        }
        cad::battery::CoolingPlatePropertiesUpdate update;
        if (QString::fromStdString(summary->label) != m_cadLabelEdit->text()) {
            update.label = m_cadLabelEdit->text().toStdString();
        }
        if (summary->visible != m_cadVisibleCheck->isChecked()) {
            update.visible = m_cadVisibleCheck->isChecked();
        }
        const cad::math::Vec3 center{
            static_cast<float>(m_cadPosX->value()),
            static_cast<float>(m_cadPosY->value()),
            static_cast<float>(m_cadPosZ->value())
        };
        const cad::math::Vec3 size{
            static_cast<float>(m_cadSizeX->value()),
            static_cast<float>(m_cadSizeY->value()),
            static_cast<float>(m_cadSizeZ->value())
        };
        if (center.x != properties->center.x || center.y != properties->center.y || center.z != properties->center.z) {
            update.center = center;
        }
        if (size.x != properties->size.x || size.y != properties->size.y || size.z != properties->size.z) {
            update.size = size;
        }
        if (update.center.has_value() || update.size.has_value()
            || update.label.has_value() || update.visible.has_value()) {
            m_cadWorkspaceView->applySelectedCoolingPlateUpdate(update);
        }
        break;
    }
    case cad::battery::EntityKind::ModuleBoundary: {
        const auto properties = m_cadWorkspaceView->selectedModuleBoundaryProperties();
        if (!properties.has_value()) {
            break;
        }
        cad::battery::ModuleBoundaryPropertiesUpdate update;
        if (QString::fromStdString(summary->label) != m_cadLabelEdit->text()) {
            update.label = m_cadLabelEdit->text().toStdString();
        }
        if (summary->visible != m_cadVisibleCheck->isChecked()) {
            update.visible = m_cadVisibleCheck->isChecked();
        }
        const cad::math::Vec3 center{
            static_cast<float>(m_cadPosX->value()),
            static_cast<float>(m_cadPosY->value()),
            static_cast<float>(m_cadPosZ->value())
        };
        const cad::math::Vec3 size{
            static_cast<float>(m_cadSizeX->value()),
            static_cast<float>(m_cadSizeY->value()),
            static_cast<float>(m_cadSizeZ->value())
        };
        if (center.x != properties->center.x || center.y != properties->center.y || center.z != properties->center.z) {
            update.center = center;
        }
        if (size.x != properties->size.x || size.y != properties->size.y || size.z != properties->size.z) {
            update.size = size;
        }
        if (update.center.has_value() || update.size.has_value()
            || update.label.has_value() || update.visible.has_value()) {
            m_cadWorkspaceView->applySelectedModuleBoundaryUpdate(update);
        }
        break;
    }
    case cad::battery::EntityKind::PackEnclosure: {
        const auto properties = m_cadWorkspaceView->selectedEnclosureProperties();
        if (!properties.has_value()) {
            break;
        }
        cad::battery::PackEnclosurePropertiesUpdate update;
        if (QString::fromStdString(summary->label) != m_cadLabelEdit->text()) {
            update.label = m_cadLabelEdit->text().toStdString();
        }
        if (summary->visible != m_cadVisibleCheck->isChecked()) {
            update.visible = m_cadVisibleCheck->isChecked();
        }
        const cad::math::Vec3 center{
            static_cast<float>(m_cadPosX->value()),
            static_cast<float>(m_cadPosY->value()),
            static_cast<float>(m_cadPosZ->value())
        };
        const cad::math::Vec3 size{
            static_cast<float>(m_cadSizeX->value()),
            static_cast<float>(m_cadSizeY->value()),
            static_cast<float>(m_cadSizeZ->value())
        };
        const float wallThickness = static_cast<float>(m_cadThickness->value());
        if (center.x != properties->center.x || center.y != properties->center.y || center.z != properties->center.z) {
            update.center = center;
        }
        if (size.x != properties->size.x || size.y != properties->size.y || size.z != properties->size.z) {
            update.size = size;
        }
        if (wallThickness != properties->wall_thickness) {
            update.wall_thickness = wallThickness;
        }
        if (update.center.has_value() || update.size.has_value() || update.wall_thickness.has_value()
            || update.label.has_value() || update.visible.has_value()) {
            m_cadWorkspaceView->applySelectedEnclosureUpdate(update);
        }
        break;
    }
    }

    m_isSyncingCadInspector = false;
    refreshCadProperties();
}

void MainWindow::undoCadEdit()
{
    m_isSyncingCadInspector = true;
    if (m_cadWorkspaceView != nullptr && m_cadWorkspaceView->undoLastEdit()) {
        refreshCadProperties();
    }
    m_isSyncingCadInspector = false;
    refreshCadProperties();
}

void MainWindow::redoCadEdit()
{
    m_isSyncingCadInspector = true;
    if (m_cadWorkspaceView != nullptr && m_cadWorkspaceView->redoLastEdit()) {
        refreshCadProperties();
    }
    m_isSyncingCadInspector = false;
    refreshCadProperties();
}

void MainWindow::resetCadPosition()
{
    m_isSyncingCadInspector = true;
    if (m_cadWorkspaceView != nullptr && m_cadWorkspaceView->resetSelectedPositionToGenerated()) {
        refreshCadProperties();
    }
    m_isSyncingCadInspector = false;
    refreshCadProperties();
}

void MainWindow::resetCadGeometry()
{
    m_isSyncingCadInspector = true;
    if (m_cadWorkspaceView != nullptr && m_cadWorkspaceView->resetSelectedGeometryToGenerated()) {
        refreshCadProperties();
    }
    m_isSyncingCadInspector = false;
    refreshCadProperties();
}

void MainWindow::resetCadLabel()
{
    m_isSyncingCadInspector = true;
    if (m_cadWorkspaceView != nullptr && m_cadWorkspaceView->resetSelectedLabelToGenerated()) {
        refreshCadProperties();
    }
    m_isSyncingCadInspector = false;
    refreshCadProperties();
}

charts::Series MainWindow::toChartSeries(const QJsonArray& timeSeries, const QString& metricKey, const QString& name, const QColor& color, bool dashed) const
{
    charts::Series series;
    series.name = name.toStdString();
    series.red = color.red();
    series.green = color.green();
    series.blue = color.blue();
    series.dashed = dashed;
    for (const QJsonValue& pointValue : timeSeries) {
        const QJsonObject point = pointValue.toObject();
        series.points.push_back({point.value("time_s").toDouble(), point.value(metricKey).toDouble()});
    }
    return series;
}

void MainWindow::populateChart(
    ChartWidget* graphWidget,
    const QJsonArray& timeSeries,
    const QString& metricKey,
    const QString& title,
    const QString& yTitle
)
{
    if (graphWidget == nullptr) {
        return;
    }
    graphWidget->showSingleSeries(
        title,
        yTitle,
        toChartSeries(timeSeries, metricKey, "Current Scenario", QColor(14, 165, 233))
    );
}

void MainWindow::populateComparisonChart(
    ChartWidget* graphWidget,
    const QJsonArray& baselineSeries,
    const QJsonArray& candidateSeries,
    const QString& metricKey,
    const QString& title,
    const QString& yTitle
)
{
    if (graphWidget == nullptr) {
        return;
    }
    graphWidget->showComparison(
        title,
        yTitle,
        toChartSeries(baselineSeries, metricKey, "Baseline", QColor(123, 135, 148), true),
        toChartSeries(candidateSeries, metricKey, "Candidate", QColor(14, 165, 233))
    );
}
