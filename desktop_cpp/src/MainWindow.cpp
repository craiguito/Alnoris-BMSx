#include "MainWindow.h"
#include "SimulationMappingBuilder.h"

#include <QComboBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCoreApplication>
#include <QFrame>
#include <QFile>
#include <QFileInfo>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QHBoxLayout>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <functional>
#include <limits>
#include <string>

namespace {

charts::Point makePoint(double x, double y)
{
    return charts::Point{x, y};
}

QColor overlayMetricAccent(int index)
{
    switch (index) {
    case 3:
        return QColor(168, 85, 247);
    case 4:
        return QColor(236, 72, 153);
    case 5:
        return QColor(251, 191, 36);
    case 2:
        return QColor(59, 130, 246);
    case 1:
        return QColor(34, 197, 94);
    case 0:
    default:
        return QColor(245, 113, 61);
    }
}

QString formatMaybeNumber(double value, int decimals = 3)
{
    return QString::number(value, 'f', decimals);
}

void appendDesktopStartupLog(const QString& line)
{
    const QString path = QDir(QCoreApplication::applicationDirPath()).filePath("alnoris_startup.log");
    QFile file(path);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODate) << " " << line << '\n';
}

cad::battery::CellFormFactor cellFormFactorFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == "prismatic") {
        return cad::battery::CellFormFactor::Prismatic;
    }
    if (normalized == "pouch") {
        return cad::battery::CellFormFactor::Pouch;
    }
    return cad::battery::CellFormFactor::Cylindrical;
}

cad::battery::BatteryVisualizationOverlay buildSimulationOverlay(
    const cad::core::CadDocument& document,
    const desktop::SimulationResultModel& result,
    int point_index,
    cad::battery::BatteryVisualizationOverlay::Metric metric
)
{
    cad::battery::BatteryVisualizationOverlay overlay;
    overlay.active_metric = metric;

    const desktop::SimulationTracePoint* point = result.pointAt(point_index);
    if (point == nullptr) {
        return overlay;
    }

    for (const cad::battery::CellEntity& cell : document.cells()) {
        int group_index = cell.simulation_group_index >= 0 ? cell.simulation_group_index : cell.series_index;
        if (group_index < 0 && !result.group_entity_ids.isEmpty()) {
            const QString entityId = QString::number(static_cast<qulonglong>(cell.id.value));
            group_index = result.group_entity_ids.indexOf(entityId);
        }
        if (group_index < 0) {
            continue;
        }
        if (group_index < static_cast<int>(point->group_core_temp_c.size())) {
            overlay.cell_core_temperature_c[cell.id] = point->group_core_temp_c[static_cast<std::size_t>(group_index)];
        }
        if (group_index < static_cast<int>(point->group_surface_temp_c.size())) {
            overlay.cell_surface_temperature_c[cell.id] = point->group_surface_temp_c[static_cast<std::size_t>(group_index)];
        }
        if (group_index < static_cast<int>(point->group_soc.size())) {
            overlay.cell_soc[cell.id] = point->group_soc[static_cast<std::size_t>(group_index)];
        }
        if (group_index < static_cast<int>(point->group_voltage_v.size())) {
            overlay.cell_voltage_v[cell.id] = point->group_voltage_v[static_cast<std::size_t>(group_index)];
        }
        if (group_index < static_cast<int>(point->group_diffusion_stress.size())) {
            overlay.cell_diffusion_stress[cell.id] = point->group_diffusion_stress[static_cast<std::size_t>(group_index)];
        }
        if (group_index < static_cast<int>(point->group_effective_resistance_ohm.size())) {
            overlay.cell_effective_resistance_ohm[cell.id] = point->group_effective_resistance_ohm[static_cast<std::size_t>(group_index)];
        }
    }

    return overlay;
}

} // namespace

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

    m_systemPresetCategoryCombo = new QComboBox(central);
    connect(m_systemPresetCategoryCombo, &QComboBox::currentIndexChanged, this, &MainWindow::handleSystemPresetCategoryChanged);
    m_systemPresetCombo = new QComboBox(central);
    m_systemPresetDescription = new QLabel("Loading battery system presets...", central);
    m_systemPresetDescription->setWordWrap(true);
    m_systemPresetDescription->setStyleSheet("font-size:12px; color:#93a6ba;");
    connect(m_systemPresetCombo, &QComboBox::currentIndexChanged, this, &MainWindow::handleSystemPresetCategoryChanged);
    m_applySystemPresetButton = new QPushButton("Load Preset", central);
    connect(m_applySystemPresetButton, &QPushButton::clicked, this, &MainWindow::applySelectedSystemPreset);

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
    rightTabs->addTab(createVirtualTestsPanel(), "Virtual Tests");

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
    clearSimulationVisualization();

    QTimer::singleShot(0, this, [this]() {
        appendDesktopStartupLog("main window deferred initialization begin");
        loadSystemPresetCatalog();
        appendDesktopStartupLog("system preset catalog loaded");
        loadVirtualTestCatalog();
        appendDesktopStartupLog("virtual test catalog loaded");
        if (m_systemPresetCombo != nullptr && m_systemPresetCombo->count() > 0) {
            applySelectedSystemPreset();
            appendDesktopStartupLog("default system preset applied");
        } else {
            updateCadWorkspace();
            appendDesktopStartupLog("fallback CAD workspace refreshed");
        }
    });
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
    const desktop::SimulationResultModel baseline = desktop::parseSimulationResultPayload(m_baselineResult);
    if (baseline.valid) {
        m_outputText->setPlainText(formatSummaryLines(baseline) + "\n\n" + formatTraceLines(baseline));
    }
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
    root.insert("system_preset", m_activeSystemPreset);
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
    m_activeSystemPreset = root.value("system_preset").toObject();
    m_baselineConfig = root.value("baseline_config").toObject();
    m_baselineResult = root.value("baseline_result").toObject();
    m_referencePreset->setCurrentIndex(root.value("reference_preset_index").toInt(0));
    if (m_systemPresetCombo != nullptr && !m_activeSystemPreset.isEmpty()) {
        const QString presetId = m_activeSystemPreset.value("preset_id").toString();
        for (int index = 0; index < m_systemPresetCombo->count(); ++index) {
            if (m_systemPresetCombo->itemData(index).toString() == presetId) {
                m_systemPresetCombo->setCurrentIndex(index);
                break;
            }
        }
    }
    updateCadWorkspace();

    if (!m_baselineResult.isEmpty()) {
        m_summaryLabel->setText(QString("Loaded project from %1 with a saved baseline.").arg(path));
        const desktop::SimulationResultModel baseline = desktop::parseSimulationResultPayload(m_baselineResult);
        if (baseline.valid) {
            m_outputText->setPlainText(formatSummaryLines(baseline) + "\n\n" + formatTraceLines(baseline));
        }
    } else {
        m_summaryLabel->setText(QString("Loaded project from %1.").arg(path));
    }
}

void MainWindow::applyReferencePreset(int index)
{
    if (index > 0) {
        m_activeSystemPreset = {};
    }
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
    toolbar->addAction("Export Result", this, &MainWindow::exportActiveResultJson);
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

    auto* presetGroup = new QGroupBox("Battery System Preset", content);
    auto* presetLayout = new QFormLayout(presetGroup);
    presetLayout->setContentsMargins(14, 16, 14, 14);
    presetLayout->setHorizontalSpacing(10);
    presetLayout->setVerticalSpacing(8);
    presetLayout->addRow("Category", m_systemPresetCategoryCombo);
    presetLayout->addRow("Preset", m_systemPresetCombo);
    presetLayout->addRow("", m_applySystemPresetButton);
    presetLayout->addRow("Description", m_systemPresetDescription);
    layout->addWidget(presetGroup);

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
    QJsonObject config = m_activeSystemPreset.value("simulation_defaults").toObject();
    config.insert("cell_nominal_voltage", m_cellNominalVoltage->value());
    config.insert("cell_full_voltage", m_cellFullVoltage->value());
    config.insert("cell_empty_voltage", m_cellEmptyVoltage->value());
    config.insert("cell_cutoff_voltage", m_cellCutoffVoltage->value());
    config.insert("cell_capacity_ah", m_cellCapacity->value());
    config.insert("cells_in_series", static_cast<int>(m_cellsInSeries->value()));
    config.insert("cells_in_parallel", static_cast<int>(m_cellsInParallel->value()));
    config.insert("internal_resistance_ohm_per_cell", m_internalResistance->value());
    config.insert("ambient_temp_c", m_ambientTemp->value());
    config.insert("discharge_current_a", m_dischargeCurrent->value());
    config.insert("duration_s", static_cast<int>(m_duration->value()));
    config.insert("time_step_s", static_cast<int>(m_timeStep->value()));
    config.insert("initial_soc", m_initialSoc->value());
    config.insert("pack_mass_kg", m_packMass->value());
    config.insert("pack_heat_capacity_j_per_kgk", m_packHeatCapacity->value());
    config.insert("cooling_coeff_w_per_k", m_coolingCoeff->value());

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
    m_lastExportPayload = payload;
    m_activeResult = desktop::parseSimulationResultPayload(payload);
    if (!m_activeResult->valid) {
        QMessageBox::warning(this, "Simulation Result Error", m_activeResult->error);
        clearSimulationVisualization();
        return;
    }

    m_activeResultPointIndex = m_activeResult->pointCount() - 1;
    m_selectedResultGroupIndex = m_activeResult->summary.weakest_group_index >= 0
        ? m_activeResult->summary.weakest_group_index
        : (m_activeResult->summary.hottest_group_index >= 0 ? m_activeResult->summary.hottest_group_index : 0);
    refreshSimulationViews();
}

void MainWindow::renderComparison(const QJsonObject& baselinePayload, const QJsonObject& candidatePayload)
{
    const desktop::SimulationResultModel baseline = desktop::parseSimulationResultPayload(baselinePayload);
    const desktop::SimulationResultModel candidate = desktop::parseSimulationResultPayload(candidatePayload);
    if (!baseline.valid || !candidate.valid) {
        QMessageBox::warning(this, "Comparison Error", "One of the simulation payloads could not be parsed.");
        return;
    }

    m_activeResult = candidate;
    m_lastExportPayload = candidatePayload;
    m_activeResultPointIndex = candidate.pointCount() - 1;
    if (m_selectedResultGroupIndex < 0) {
        m_selectedResultGroupIndex = candidate.summary.weakest_group_index >= 0 ? candidate.summary.weakest_group_index : 0;
    }
    refreshSimulationViews();

    const double runtimeDelta = candidate.summary.runtime_s - baseline.summary.runtime_s;
    const double energyDelta = candidate.summary.delivered_energy_wh - baseline.summary.delivered_energy_wh;
    const double tempDelta = candidate.summary.max_group_temp_c - baseline.summary.max_group_temp_c;
    const double voltageDelta = candidate.summary.min_group_voltage_v - baseline.summary.min_group_voltage_v;

    m_summaryLabel->setText("Scenario comparison view. Charts overlay candidate against baseline while the CAD viewport stays synced to the candidate run.");
    m_voltageChartView->showComparison(
        "Pack Voltage Comparison",
        "Voltage (V)",
        makeSeries(pointSeriesForMetric(baseline, "pack_voltage_v"), "Baseline", QColor(123, 135, 148), true),
        makeSeries(pointSeriesForMetric(candidate, "pack_voltage_v"), "Candidate", QColor(14, 165, 233))
    );
    m_currentChartView->showComparison(
        "Pack Current Comparison",
        "Current (A)",
        makeSeries(pointSeriesForMetric(baseline, "current_a"), "Baseline", QColor(123, 135, 148), true),
        makeSeries(pointSeriesForMetric(candidate, "current_a"), "Candidate", QColor(14, 165, 233))
    );
    m_powerChartView->showComparison(
        "Pack Power Comparison",
        "Power (W)",
        makeSeries(pointSeriesForMetric(baseline, "pack_power_w"), "Baseline", QColor(123, 135, 148), true),
        makeSeries(pointSeriesForMetric(candidate, "pack_power_w"), "Candidate", QColor(14, 165, 233))
    );
    m_temperatureChartView->showComparison(
        "Max Temperature Comparison",
        "Temperature (C)",
        makeSeries(pointSeriesForMetric(baseline, "pack_temp_max_c"), "Baseline", QColor(123, 135, 148), true),
        makeSeries(pointSeriesForMetric(candidate, "pack_temp_max_c"), "Candidate", QColor(14, 165, 233))
    );
    m_socChartView->showComparison(
        "Average SOC Comparison",
        "SOC",
        makeSeries(pointSeriesForMetric(baseline, "soc_avg"), "Baseline", QColor(123, 135, 148), true),
        makeSeries(pointSeriesForMetric(candidate, "soc_avg"), "Candidate", QColor(14, 165, 233))
    );
    m_socEnvelopeChartView->showComparison(
        "Minimum SOC Comparison",
        "SOC",
        makeSeries(pointSeriesForMetric(baseline, "soc_min"), "Baseline", QColor(123, 135, 148), true),
        makeSeries(pointSeriesForMetric(candidate, "soc_min"), "Candidate", QColor(14, 165, 233))
    );

    const std::optional<double> markerTime = candidate.pointAt(m_activeResultPointIndex) != nullptr
        ? std::optional<double>(candidate.pointAt(m_activeResultPointIndex)->time_s)
        : std::nullopt;
    for (ChartWidget* chart : {m_voltageChartView, m_currentChartView, m_powerChartView, m_temperatureChartView, m_socChartView, m_socEnvelopeChartView}) {
        if (chart != nullptr) {
            chart->setMarkerTime(markerTime);
        }
    }

    QStringList lines;
    lines << "Baseline Summary";
    lines << formatSummaryLines(baseline);
    lines << "";
    lines << "Candidate Summary";
    lines << formatSummaryLines(candidate);
    lines << "";
    lines << "Comparison Deltas";
    lines << QString("Runtime delta: %1 s").arg(runtimeDelta, 0, 'f', 1);
    lines << QString("Delivered energy delta: %1 Wh").arg(energyDelta, 0, 'f', 2);
    lines << QString("Max group temperature delta: %1 C").arg(tempDelta, 0, 'f', 2);
    lines << QString("Minimum group voltage delta: %1 V").arg(voltageDelta, 0, 'f', 2);
    lines << "";
    lines << "Candidate Trace";
    lines << formatTraceLines(candidate);
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

QString MainWindow::formatSummaryLines(const desktop::SimulationResultModel& result) const
{
    QStringList lines;
    lines << QString("Pack nominal voltage: %1 V").arg(result.pack_nominal_voltage_v, 0, 'f', 2);
    lines << QString("Pack capacity: %1 Ah").arg(result.pack_capacity_ah, 0, 'f', 2);
    lines << QString("Theoretical energy: %1 Wh").arg(result.theoretical_energy_wh, 0, 'f', 2);
    lines << QString("Delivered energy: %1 Wh").arg(result.summary.delivered_energy_wh, 0, 'f', 2);
    lines << QString("Delivered capacity: %1 Ah").arg(result.summary.delivered_capacity_ah, 0, 'f', 3);
    lines << QString("Max core temperature: %1 C").arg(result.summary.max_core_temp_c, 0, 'f', 2);
    lines << QString("Max surface temperature: %1 C").arg(result.summary.max_surface_temp_c, 0, 'f', 2);
    lines << QString("Minimum group voltage: %1 V").arg(result.summary.min_group_voltage_v, 0, 'f', 3);
    lines << QString("Final average SOC: %1").arg(result.summary.final_soc_avg, 0, 'f', 3);
    lines << QString("SOC spread: %1").arg(result.summary.soc_spread, 0, 'f', 4);
    lines << QString("Max diffusion stress: %1").arg(result.summary.max_diffusion_stress, 0, 'f', 4);
    lines << QString("Chemistry: %1").arg(result.summary.chemistry_name.isEmpty() ? "generic_liion" : result.summary.chemistry_name);
    lines << QString("Capacity retention: %1").arg(result.summary.capacity_retention, 0, 'f', 5);
    lines << QString("Resistance growth: %1").arg(result.summary.resistance_growth, 0, 'f', 5);
    lines << QString("Runtime: %1 s").arg(result.summary.runtime_s, 0, 'f', 0);
    lines << QString("Termination: %1").arg(result.summary.termination_reason);
    if (!result.summary.enabled_nonlinear_features.isEmpty()) {
        lines << QString("Enabled nonlinear features: %1").arg(result.summary.enabled_nonlinear_features.join(", "));
    }
    const QString warningText = formatWarningLines(result.summary.warnings);
    if (!warningText.isEmpty()) {
        lines << "Warnings:";
        lines << warningText;
    }
    return lines.join('\n');
}

QString MainWindow::formatTraceLines(const desktop::SimulationResultModel& result) const
{
    QStringList lines;
    for (const desktop::SimulationTracePoint& point : result.points) {
        lines << QString("t=%1s  I=%2A  soc_avg=%3  V=%4  P=%5W  Tmax=%6C")
                     .arg(point.time_s, 5, 'f', 0)
                     .arg(point.current_a, 0, 'f', 3)
                     .arg(point.soc_avg, 0, 'f', 3)
                     .arg(point.pack_voltage_v, 0, 'f', 3)
                     .arg(point.pack_power_w, 0, 'f', 3)
                     .arg(point.pack_temp_max_c, 0, 'f', 3);
    }
    return lines.join('\n');
}

QString MainWindow::formatWarningLines(const std::vector<desktop::SimulationWarningModel>& warnings) const
{
    QStringList lines;
    for (const desktop::SimulationWarningModel& warning : warnings) {
        lines << QString("- [%1] %2").arg(warning.severity, warning.message);
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

    auto* playbackGroup = new QGroupBox("Playback & Overlay", group);
    auto* playbackLayout = new QGridLayout(playbackGroup);
    playbackLayout->setContentsMargins(14, 16, 14, 14);
    playbackLayout->setHorizontalSpacing(10);
    playbackLayout->setVerticalSpacing(8);
    m_overlayMetricCombo = new QComboBox(playbackGroup);
    m_overlayMetricCombo->addItems({
        "Core Temperature Overlay",
        "SOC Overlay",
        "Voltage Overlay",
        "Surface Temperature Overlay",
        "Diffusion Stress Overlay",
        "Effective Resistance Overlay"
    });
    connect(m_overlayMetricCombo, &QComboBox::currentIndexChanged, this, &MainWindow::handleOverlayMetricChanged);
    m_resultTimeSlider = new QSlider(Qt::Horizontal, playbackGroup);
    m_resultTimeSlider->setEnabled(false);
    connect(m_resultTimeSlider, &QSlider::valueChanged, this, &MainWindow::handleResultScrubChanged);
    m_resultTimeLabel = new QLabel("Time: --", playbackGroup);
    m_resultSelectionLabel = new QLabel("Selected group: --", playbackGroup);
    m_resultOverlayLegendLabel = new QLabel("Overlay range: --", playbackGroup);
    m_resultSelectionLabel->setStyleSheet("color:#93a6ba;");
    m_resultOverlayLegendLabel->setStyleSheet("color:#93a6ba;");
    playbackLayout->addWidget(new QLabel("Overlay metric", playbackGroup), 0, 0);
    playbackLayout->addWidget(m_overlayMetricCombo, 0, 1);
    playbackLayout->addWidget(new QLabel("Scrub timestep", playbackGroup), 1, 0);
    playbackLayout->addWidget(m_resultTimeSlider, 1, 1);
    playbackLayout->addWidget(m_resultTimeLabel, 2, 0);
    playbackLayout->addWidget(m_resultSelectionLabel, 2, 1);
    playbackLayout->addWidget(m_resultOverlayLegendLabel, 3, 0, 1, 2);
    layout->addWidget(playbackGroup);

    auto* chartTabs = new QTabWidget(group);
    auto* electricalTab = new QWidget(chartTabs);
    auto* electricalLayout = new QVBoxLayout(electricalTab);
    electricalLayout->setContentsMargins(8, 8, 8, 8);
    electricalLayout->setSpacing(10);
    m_topChartTabs = new QTabWidget(electricalTab);
    m_voltageChartView = createGraphWidget();
    m_currentChartView = createGraphWidget();
    m_powerChartView = createGraphWidget();
    m_topChartTabs->addTab(m_voltageChartView, "Voltage");
    m_topChartTabs->addTab(m_currentChartView, "Current");
    m_topChartTabs->addTab(m_powerChartView, "Power");
    electricalLayout->addWidget(m_topChartTabs, 1);

    auto* thermalTab = new QWidget(chartTabs);
    auto* thermalLayout = new QVBoxLayout(thermalTab);
    thermalLayout->setContentsMargins(8, 8, 8, 8);
    thermalLayout->setSpacing(10);
    m_bottomChartTabs = new QTabWidget(thermalTab);
    m_temperatureChartView = createGraphWidget();
    m_coreTemperatureChartView = createGraphWidget();
    m_surfaceTemperatureChartView = createGraphWidget();
    m_socChartView = createGraphWidget();
    m_socEnvelopeChartView = createGraphWidget();
    m_bottomChartTabs->addTab(m_temperatureChartView, "Temp Summary");
    m_bottomChartTabs->addTab(m_coreTemperatureChartView, "Core Temp");
    m_bottomChartTabs->addTab(m_surfaceTemperatureChartView, "Surface Temp");
    m_bottomChartTabs->addTab(m_socChartView, "Avg SOC");
    m_bottomChartTabs->addTab(m_socEnvelopeChartView, "SOC Envelope");
    thermalLayout->addWidget(m_bottomChartTabs, 1);

    chartTabs->addTab(electricalTab, "Electrical");
    chartTabs->addTab(thermalTab, "Thermal / State");
    layout->addWidget(chartTabs, 1);

    auto* groupInfoGroup = new QGroupBox("Group Inspection", group);
    auto* groupInfoLayout = new QVBoxLayout(groupInfoGroup);
    groupInfoLayout->setContentsMargins(14, 16, 14, 14);
    groupInfoLayout->setSpacing(10);
    m_groupTable = new QTableWidget(0, 5, groupInfoGroup);
    m_groupTable->setHorizontalHeaderLabels({"Group", "SOC", "Voltage (V)", "Core (C)", "Surface (C)"});
    m_groupTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_groupTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_groupTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_groupTable->verticalHeader()->setVisible(false);
    m_groupTable->horizontalHeader()->setStretchLastSection(true);
    m_groupTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    connect(m_groupTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::handleGroupSelectionChanged);
    groupInfoLayout->addWidget(m_groupTable);
    m_groupDetailLabel = new QLabel("Select a group from the table or CAD view to inspect nonlinear state values.", groupInfoGroup);
    m_groupDetailLabel->setWordWrap(true);
    m_groupDetailLabel->setStyleSheet("color:#93a6ba;");
    groupInfoLayout->addWidget(m_groupDetailLabel);

    auto* groupChartTabs = new QTabWidget(groupInfoGroup);
    m_groupVoltageChartView = createGraphWidget();
    m_groupCoreTemperatureChartView = createGraphWidget();
    m_groupSurfaceTemperatureChartView = createGraphWidget();
    m_groupDiffusionStressChartView = createGraphWidget();
    m_groupHysteresisChartView = createGraphWidget();
    m_groupSocChartView = createGraphWidget();
    groupChartTabs->addTab(m_groupVoltageChartView, "Selected Voltage");
    groupChartTabs->addTab(m_groupCoreTemperatureChartView, "Core Temp");
    groupChartTabs->addTab(m_groupSurfaceTemperatureChartView, "Surface Temp");
    groupChartTabs->addTab(m_groupDiffusionStressChartView, "Stress");
    groupChartTabs->addTab(m_groupHysteresisChartView, "Hysteresis");
    groupChartTabs->addTab(m_groupSocChartView, "Selected SOC");
    groupInfoLayout->addWidget(groupChartTabs, 1);
    layout->addWidget(groupInfoGroup, 1);

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

QGroupBox* MainWindow::createVirtualTestsPanel()
{
    auto* group = new QGroupBox("", this);
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* header = new QLabel("Virtual Testbench", group);
    header->setStyleSheet("font-size:16px; font-weight:700; color:#f3f7fb;");
    auto* subheader = new QLabel("Select an engineering test workflow, vet it against the current pack configuration, then run it into the existing desktop result views.", group);
    subheader->setWordWrap(true);
    subheader->setStyleSheet("font-size:12px; color:#93a6ba;");
    layout->addWidget(header);
    layout->addWidget(subheader);

    auto* selectionGroup = new QGroupBox("Test Selection", group);
    auto* selectionLayout = new QVBoxLayout(selectionGroup);
    selectionLayout->setContentsMargins(14, 16, 14, 14);
    selectionLayout->setSpacing(8);
    m_virtualTestCombo = new QComboBox(selectionGroup);
    connect(m_virtualTestCombo, &QComboBox::currentIndexChanged, this, &MainWindow::handleVirtualTestSelectionChanged);
    m_virtualTestDescription = new QLabel("Loading test catalog...", selectionGroup);
    m_virtualTestDescription->setWordWrap(true);
    m_virtualTestDescription->setStyleSheet("color:#93a6ba;");
    selectionLayout->addWidget(m_virtualTestCombo);
    selectionLayout->addWidget(m_virtualTestDescription);
    layout->addWidget(selectionGroup);

    auto* parametersGroup = new QGroupBox("Parameters", group);
    m_virtualTestFormLayout = new QFormLayout(parametersGroup);
    m_virtualTestFormLayout->setContentsMargins(14, 16, 14, 14);
    m_virtualTestFormLayout->setHorizontalSpacing(10);
    m_virtualTestFormLayout->setVerticalSpacing(8);
    layout->addWidget(parametersGroup);

    auto* actionsGroup = new QGroupBox("Actions", group);
    auto* actionsLayout = new QVBoxLayout(actionsGroup);
    actionsLayout->setContentsMargins(14, 16, 14, 14);
    actionsLayout->setSpacing(8);
    auto* actionButtons = new QHBoxLayout();
    m_vetTestButton = new QPushButton("Vet Test", actionsGroup);
    m_runTestButton = new QPushButton("Run Test", actionsGroup);
    connect(m_vetTestButton, &QPushButton::clicked, this, &MainWindow::vetSelectedVirtualTest);
    connect(m_runTestButton, &QPushButton::clicked, this, &MainWindow::runSelectedVirtualTest);
    actionButtons->addWidget(m_vetTestButton);
    actionButtons->addWidget(m_runTestButton);
    m_virtualTestStatus = new QPlainTextEdit(actionsGroup);
    m_virtualTestStatus->setReadOnly(true);
    m_virtualTestStatus->setMinimumHeight(140);
    actionsLayout->addLayout(actionButtons);
    actionsLayout->addWidget(m_virtualTestStatus);
    m_virtualTestComparisonTable = new QTableWidget(0, 3, actionsGroup);
    m_virtualTestComparisonTable->setHorizontalHeaderLabels({"Scenario", "Metric", "Value"});
    m_virtualTestComparisonTable->verticalHeader()->setVisible(false);
    m_virtualTestComparisonTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_virtualTestComparisonTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_virtualTestComparisonTable->horizontalHeader()->setStretchLastSection(true);
    m_virtualTestComparisonTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    actionsLayout->addWidget(m_virtualTestComparisonTable, 1);
    layout->addWidget(actionsGroup, 1);

    return group;
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
    applyChartTheme(m_currentChartView);
    applyChartTheme(m_temperatureChartView);
    applyChartTheme(m_coreTemperatureChartView);
    applyChartTheme(m_surfaceTemperatureChartView);
    applyChartTheme(m_socChartView);
    applyChartTheme(m_socEnvelopeChartView);
    applyChartTheme(m_powerChartView);
    applyChartTheme(m_groupVoltageChartView);
    applyChartTheme(m_groupCoreTemperatureChartView);
    applyChartTheme(m_groupSurfaceTemperatureChartView);
    applyChartTheme(m_groupDiffusionStressChartView);
    applyChartTheme(m_groupHysteresisChartView);
    applyChartTheme(m_groupSocChartView);
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
    const QJsonObject cadDefaults = m_activeSystemPreset.value("cad_defaults").toObject();
    cadConfig.layout.preset_name = !cadDefaults.isEmpty()
        ? m_activeSystemPreset.value("display_name").toString().toStdString()
        : (m_referencePreset != nullptr ? m_referencePreset->currentText().toStdString() : std::string("Custom"));
    cadConfig.layout.cells_in_series = m_cellsInSeries != nullptr ? static_cast<int>(m_cellsInSeries->value()) : 0;
    cadConfig.layout.cells_in_parallel = m_cellsInParallel != nullptr ? static_cast<int>(m_cellsInParallel->value()) : 0;
    cadConfig.layout.module_count = cadDefaults.value("module_count").toInt(1);
    cadConfig.layout.cell_form_factor = cellFormFactorFromString(cadDefaults.value("cell_form_factor").toString("cylindrical"));
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
    cadConfig.layout.cell_radius = static_cast<float>(cadDefaults.value("cell_radius_mm").toDouble(cadConfig.layout.cell_radius));
    cadConfig.layout.cell_height = static_cast<float>(cadDefaults.value("cell_height_mm").toDouble(cadConfig.layout.cell_height));
    cadConfig.layout.cell_width = static_cast<float>(cadDefaults.value("cell_width_mm").toDouble(cadConfig.layout.cell_width));
    cadConfig.layout.cell_depth = static_cast<float>(cadDefaults.value("cell_depth_mm").toDouble(cadConfig.layout.cell_depth));
    cadConfig.layout.x_spacing = static_cast<float>(cadDefaults.value("x_spacing_mm").toDouble(cadConfig.layout.x_spacing));
    cadConfig.layout.z_spacing = static_cast<float>(cadDefaults.value("z_spacing_mm").toDouble(cadConfig.layout.z_spacing));
    cadConfig.layout.module_gap_x = static_cast<float>(cadDefaults.value("module_gap_x_mm").toDouble(cadConfig.layout.module_gap_x));
    cadConfig.layout.busbar_thickness = static_cast<float>(cadDefaults.value("busbar_thickness_mm").toDouble(cadConfig.layout.busbar_thickness));
    cadConfig.layout.cooling_channel_thickness = static_cast<float>(cadDefaults.value("cooling_channel_thickness_mm").toDouble(cadConfig.layout.cooling_channel_thickness));
    cadConfig.layout.enclosure_wall_thickness = static_cast<float>(cadDefaults.value("enclosure_wall_thickness_mm").toDouble(cadConfig.layout.enclosure_wall_thickness));
    m_cadWorkspaceView->setPackConfig(cadConfig);
    refreshCadOverlay();
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
    case cad::battery::EntityKind::BatteryPack:
        m_cadSelectedType->setText("Battery Pack");
        m_cadPosX->setValue(0.0);
        m_cadPosY->setValue(0.0);
        m_cadPosZ->setValue(0.0);
        break;
    case cad::battery::EntityKind::CellGroup:
        m_cadSelectedType->setText("Cell Group");
        m_cadPosX->setValue(0.0);
        m_cadPosY->setValue(0.0);
        m_cadPosZ->setValue(0.0);
        break;
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
        m_cadSelectedType->setText("Cooling Channel");
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
        m_cadSelectedType->setText("Battery Module");
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
        m_cadSelectedType->setText("Enclosure");
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
    case cad::battery::EntityKind::BatteryPack:
    case cad::battery::EntityKind::CellGroup: {
        if (QString::fromStdString(summary->label) != m_cadLabelEdit->text()) {
            m_cadWorkspaceView->applyRenameToSelected(m_cadLabelEdit->text());
        }
        if (summary->visible != m_cadVisibleCheck->isChecked()) {
            m_cadWorkspaceView->applySelectedVisibility(m_cadVisibleCheck->isChecked());
        }
        break;
    }
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

charts::Series MainWindow::makeSeries(const std::vector<charts::Point>& points, const QString& name, const QColor& color, bool dashed) const
{
    charts::Series series;
    series.name = name.toStdString();
    series.points = points;
    series.red = color.red();
    series.green = color.green();
    series.blue = color.blue();
    series.dashed = dashed;
    return series;
}

std::vector<charts::Point> MainWindow::pointSeriesForMetric(const desktop::SimulationResultModel& result, const QString& metricKey) const
{
    std::vector<charts::Point> points;
    points.reserve(result.points.size());
    for (const desktop::SimulationTracePoint& point : result.points) {
        double value = 0.0;
        if (metricKey == "pack_voltage_v") {
            value = point.pack_voltage_v;
        } else if (metricKey == "current_a") {
            value = point.current_a;
        } else if (metricKey == "pack_power_w") {
            value = point.pack_power_w;
        } else if (metricKey == "soc_avg") {
            value = point.soc_avg;
        } else if (metricKey == "soc_min") {
            value = point.soc_min;
        } else if (metricKey == "soc_max") {
            value = point.soc_max;
        } else if (metricKey == "pack_temp_avg_c") {
            value = point.pack_temp_avg_c;
        } else if (metricKey == "pack_temp_max_c") {
            value = point.pack_temp_max_c;
        } else if (metricKey == "pack_heat_w") {
            value = point.pack_heat_w;
        } else if (metricKey == "group_core_temp_max_c" && !point.group_core_temp_c.empty()) {
            value = *std::max_element(point.group_core_temp_c.begin(), point.group_core_temp_c.end());
        } else if (metricKey == "group_surface_temp_max_c" && !point.group_surface_temp_c.empty()) {
            value = *std::max_element(point.group_surface_temp_c.begin(), point.group_surface_temp_c.end());
        }
        points.push_back(makePoint(point.time_s, value));
    }
    return points;
}

std::vector<charts::Point> MainWindow::pointSeriesForGroupMetric(const desktop::SimulationResultModel& result, int groupIndex, const QString& metricKey) const
{
    std::vector<charts::Point> points;
    if (groupIndex < 0) {
        return points;
    }
    points.reserve(result.points.size());
    for (const desktop::SimulationTracePoint& point : result.points) {
        double value = 0.0;
        if (metricKey == "group_soc" && groupIndex < static_cast<int>(point.group_soc.size())) {
            value = point.group_soc[static_cast<std::size_t>(groupIndex)];
        } else if (metricKey == "group_core_temp" && groupIndex < static_cast<int>(point.group_core_temp_c.size())) {
            value = point.group_core_temp_c[static_cast<std::size_t>(groupIndex)];
        } else if (metricKey == "group_surface_temp" && groupIndex < static_cast<int>(point.group_surface_temp_c.size())) {
            value = point.group_surface_temp_c[static_cast<std::size_t>(groupIndex)];
        } else if (metricKey == "group_voltage" && groupIndex < static_cast<int>(point.group_voltage_v.size())) {
            value = point.group_voltage_v[static_cast<std::size_t>(groupIndex)];
        } else if (metricKey == "group_diffusion_stress" && groupIndex < static_cast<int>(point.group_diffusion_stress.size())) {
            value = point.group_diffusion_stress[static_cast<std::size_t>(groupIndex)];
        } else if (metricKey == "group_hysteresis_v" && groupIndex < static_cast<int>(point.group_hysteresis_v.size())) {
            value = point.group_hysteresis_v[static_cast<std::size_t>(groupIndex)];
        }
        points.push_back(makePoint(point.time_s, value));
    }
    return points;
}

void MainWindow::refreshSimulationViews()
{
    if (!m_activeResult.has_value() || !m_activeResult->valid) {
        clearSimulationVisualization();
        return;
    }

    m_summaryLabel->setText("Simulation results are live in charts, group inspection, and the CAD workspace. Scrub time or change overlay metric to inspect behavior.");
    refreshResultScrubber();
    refreshCharts();
    refreshGroupTable();
    refreshGroupDetailPanel();
    refreshSelectedGroupCharts();
    refreshCadOverlay();
    m_outputText->setPlainText(formatSummaryLines(*m_activeResult) + "\n\n" + formatTraceLines(*m_activeResult));
}

void MainWindow::refreshCharts()
{
    if (!m_activeResult.has_value() || !m_activeResult->valid) {
        return;
    }

    const auto markerTime = m_activeResult->pointAt(m_activeResultPointIndex) != nullptr
        ? std::optional<double>(m_activeResult->pointAt(m_activeResultPointIndex)->time_s)
        : std::nullopt;

    m_voltageChartView->showSingleSeries(
        "Pack Voltage vs Time",
        "Voltage (V)",
        makeSeries(pointSeriesForMetric(*m_activeResult, "pack_voltage_v"), "Current Scenario", QColor(14, 165, 233))
    );
    m_currentChartView->showSingleSeries(
        "Current vs Time",
        "Current (A)",
        makeSeries(pointSeriesForMetric(*m_activeResult, "current_a"), "Current Scenario", QColor(59, 130, 246))
    );
    m_powerChartView->showSingleSeries(
        "Pack Power vs Time",
        "Power (W)",
        makeSeries(pointSeriesForMetric(*m_activeResult, "pack_power_w"), "Current Scenario", QColor(245, 158, 11))
    );
    m_socChartView->showSingleSeries(
        "Average SOC vs Time",
        "SOC",
        makeSeries(pointSeriesForMetric(*m_activeResult, "soc_avg"), "Current Scenario", QColor(34, 197, 94))
    );
    m_socEnvelopeChartView->showComparison(
        "SOC Envelope",
        "SOC",
        makeSeries(pointSeriesForMetric(*m_activeResult, "soc_min"), "Minimum", QColor(239, 68, 68), true),
        makeSeries(pointSeriesForMetric(*m_activeResult, "soc_max"), "Maximum", QColor(34, 197, 94))
    );
    m_temperatureChartView->showComparison(
        "Temperature vs Time",
        "Temperature (C)",
        makeSeries(pointSeriesForMetric(*m_activeResult, "pack_temp_avg_c"), "Average", QColor(56, 189, 248), true),
        makeSeries(pointSeriesForMetric(*m_activeResult, "pack_temp_max_c"), "Maximum", QColor(245, 113, 61))
    );
    m_coreTemperatureChartView->showSingleSeries(
        "Max Core Temperature vs Time",
        "Temperature (C)",
        makeSeries(pointSeriesForMetric(*m_activeResult, "group_core_temp_max_c"), "Core Max", QColor(220, 90, 61))
    );
    m_surfaceTemperatureChartView->showSingleSeries(
        "Max Surface Temperature vs Time",
        "Temperature (C)",
        makeSeries(pointSeriesForMetric(*m_activeResult, "group_surface_temp_max_c"), "Surface Max", QColor(56, 189, 248))
    );

    for (ChartWidget* chart : {m_voltageChartView, m_currentChartView, m_powerChartView, m_socChartView, m_socEnvelopeChartView, m_temperatureChartView, m_coreTemperatureChartView, m_surfaceTemperatureChartView}) {
        if (chart != nullptr) {
            chart->setMarkerTime(markerTime);
        }
    }
}

void MainWindow::refreshGroupTable()
{
    if (m_groupTable == nullptr) {
        return;
    }

    m_isSyncingGroupPanel = true;
    m_groupTable->clearContents();

    if (!m_activeResult.has_value() || !m_activeResult->valid) {
        m_groupTable->setRowCount(0);
        m_isSyncingGroupPanel = false;
        return;
    }

    const desktop::SimulationTracePoint* point = m_activeResult->pointAt(m_activeResultPointIndex);
    if (point == nullptr) {
        m_groupTable->setRowCount(0);
        m_isSyncingGroupPanel = false;
        return;
    }

    const int groupCount = m_activeResult->groupCount();
    m_groupTable->setRowCount(groupCount);
    for (int row = 0; row < groupCount; ++row) {
        QString label = m_activeResult->groupLabel(row);
        if (row == m_activeResult->summary.weakest_group_index) {
            label += "  [weakest]";
        }
        if (row == m_activeResult->summary.hottest_group_index) {
            label += "  [hottest]";
        }

        auto* groupItem = new QTableWidgetItem(label);
        auto* socItem = new QTableWidgetItem(row < static_cast<int>(point->group_soc.size()) ? formatMaybeNumber(point->group_soc[static_cast<std::size_t>(row)], 3) : "--");
        auto* voltageItem = new QTableWidgetItem(row < static_cast<int>(point->group_voltage_v.size()) ? formatMaybeNumber(point->group_voltage_v[static_cast<std::size_t>(row)], 3) : "--");
        auto* coreTempItem = new QTableWidgetItem(row < static_cast<int>(point->group_core_temp_c.size()) ? formatMaybeNumber(point->group_core_temp_c[static_cast<std::size_t>(row)], 2) : "--");
        auto* surfaceTempItem = new QTableWidgetItem(row < static_cast<int>(point->group_surface_temp_c.size()) ? formatMaybeNumber(point->group_surface_temp_c[static_cast<std::size_t>(row)], 2) : "--");

        if (row == m_activeResult->summary.weakest_group_index) {
            const QColor highlight(116, 52, 52, 110);
            groupItem->setBackground(highlight);
            socItem->setBackground(highlight);
            voltageItem->setBackground(highlight);
            coreTempItem->setBackground(highlight);
            surfaceTempItem->setBackground(highlight);
        } else if (row == m_activeResult->summary.hottest_group_index) {
            const QColor highlight(92, 65, 28, 110);
            groupItem->setBackground(highlight);
            socItem->setBackground(highlight);
            voltageItem->setBackground(highlight);
            coreTempItem->setBackground(highlight);
            surfaceTempItem->setBackground(highlight);
        }

        m_groupTable->setItem(row, 0, groupItem);
        m_groupTable->setItem(row, 1, socItem);
        m_groupTable->setItem(row, 2, voltageItem);
        m_groupTable->setItem(row, 3, coreTempItem);
        m_groupTable->setItem(row, 4, surfaceTempItem);
    }

    const int clampedGroup = std::clamp(m_selectedResultGroupIndex, 0, std::max(0, groupCount - 1));
    m_selectedResultGroupIndex = clampedGroup;
    if (groupCount > 0) {
        m_groupTable->selectRow(clampedGroup);
        m_resultSelectionLabel->setText(QString("Selected group: %1").arg(m_activeResult->groupLabel(clampedGroup)));
    } else {
        m_resultSelectionLabel->setText("Selected group: --");
    }
    m_isSyncingGroupPanel = false;
}

void MainWindow::refreshGroupDetailPanel()
{
    if (m_groupDetailLabel == nullptr) {
        return;
    }
    if (!m_activeResult.has_value() || !m_activeResult->valid || m_selectedResultGroupIndex < 0) {
        m_groupDetailLabel->setText("Select a group from the table or CAD view to inspect nonlinear state values.");
        return;
    }

    const desktop::SimulationTracePoint* point = m_activeResult->pointAt(m_activeResultPointIndex);
    if (point == nullptr) {
        m_groupDetailLabel->setText("Select a group from the table or CAD view to inspect nonlinear state values.");
        return;
    }

    const int index = m_selectedResultGroupIndex;
    const auto valueAt = [index](const auto& values) -> QString {
        return index < static_cast<int>(values.size())
            ? formatMaybeNumber(values[static_cast<std::size_t>(index)], 4)
            : QString("--");
    };

    QStringList flags;
    if (index == m_activeResult->summary.weakest_group_index) {
        flags << "weakest";
    }
    if (index == m_activeResult->summary.hottest_group_index) {
        flags << "hottest";
    }
    if (point->group_diffusion_stress.size() > static_cast<std::size_t>(index)) {
        const auto maxIt = std::max_element(point->group_diffusion_stress.begin(), point->group_diffusion_stress.end());
        if (maxIt != point->group_diffusion_stress.end() && std::distance(point->group_diffusion_stress.begin(), maxIt) == index) {
            flags << "highest stress";
        }
    }

    m_groupDetailLabel->setText(QString(
        "%1\n"
        "Entity: %2 | Zone: %3 | Flags: %4\n"
        "SOC: %5 | Voltage: %6 V\n"
        "Core: %7 C | Surface: %8 C\n"
        "Stress: %9 | Hysteresis: %10 V | Resistance: %11 ohm")
        .arg(m_activeResult->groupLabel(index))
        .arg(m_activeResult->groupEntityId(index).isEmpty() ? "--" : m_activeResult->groupEntityId(index))
        .arg(index < static_cast<int>(point->group_zone_ids.size()) ? QString::number(point->group_zone_ids[static_cast<std::size_t>(index)]) : "--")
        .arg(flags.isEmpty() ? "none" : flags.join(", "))
        .arg(valueAt(point->group_soc))
        .arg(valueAt(point->group_voltage_v))
        .arg(valueAt(point->group_core_temp_c))
        .arg(valueAt(point->group_surface_temp_c))
        .arg(valueAt(point->group_diffusion_stress))
        .arg(valueAt(point->group_hysteresis_v))
        .arg(valueAt(point->group_effective_resistance_ohm)));
}

void MainWindow::refreshCadOverlay()
{
    if (m_cadWorkspaceView == nullptr) {
        return;
    }

    if (!m_activeResult.has_value() || !m_activeResult->valid) {
        m_cadWorkspaceView->clearSimulationOverlay();
        if (m_resultOverlayLegendLabel != nullptr) {
            m_resultOverlayLegendLabel->setText("Overlay range: --");
        }
        return;
    }

    cad::battery::BatteryVisualizationOverlay::Metric metric = cad::battery::BatteryVisualizationOverlay::Metric::CoreTemperature;
    if (m_overlayMetricCombo != nullptr) {
        switch (m_overlayMetricCombo->currentIndex()) {
        case 1:
            metric = cad::battery::BatteryVisualizationOverlay::Metric::Soc;
            break;
        case 2:
            metric = cad::battery::BatteryVisualizationOverlay::Metric::Voltage;
            break;
        case 3:
            metric = cad::battery::BatteryVisualizationOverlay::Metric::SurfaceTemperature;
            break;
        case 4:
            metric = cad::battery::BatteryVisualizationOverlay::Metric::DiffusionStress;
            break;
        case 5:
            metric = cad::battery::BatteryVisualizationOverlay::Metric::EffectiveResistance;
            break;
        case 0:
        default:
            metric = cad::battery::BatteryVisualizationOverlay::Metric::CoreTemperature;
            break;
        }
    }

    if (const desktop::SimulationTracePoint* point = m_activeResult->pointAt(m_activeResultPointIndex); point != nullptr && m_resultOverlayLegendLabel != nullptr) {
        std::vector<double> values;
        switch (metric) {
        case cad::battery::BatteryVisualizationOverlay::Metric::Soc:
            values = point->group_soc;
            break;
        case cad::battery::BatteryVisualizationOverlay::Metric::Voltage:
            values = point->group_voltage_v;
            break;
        case cad::battery::BatteryVisualizationOverlay::Metric::SurfaceTemperature:
            values = point->group_surface_temp_c;
            break;
        case cad::battery::BatteryVisualizationOverlay::Metric::DiffusionStress:
            values = point->group_diffusion_stress;
            break;
        case cad::battery::BatteryVisualizationOverlay::Metric::EffectiveResistance:
            values = point->group_effective_resistance_ohm;
            break;
        case cad::battery::BatteryVisualizationOverlay::Metric::CoreTemperature:
        default:
            values = point->group_core_temp_c;
            break;
        }
        if (!values.empty()) {
            const auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
            m_resultOverlayLegendLabel->setText(QString("Overlay range: %1 to %2").arg(formatMaybeNumber(*minIt, 3), formatMaybeNumber(*maxIt, 3)));
        } else {
            m_resultOverlayLegendLabel->setText("Overlay range: --");
        }
    }

    m_cadWorkspaceView->setSimulationOverlay(
        buildSimulationOverlay(m_cadWorkspaceView->document(), *m_activeResult, m_activeResultPointIndex, metric)
    );
}

void MainWindow::refreshResultScrubber()
{
    if (m_resultTimeSlider == nullptr || m_resultTimeLabel == nullptr) {
        return;
    }

    QSignalBlocker blocker(m_resultTimeSlider);
    if (!m_activeResult.has_value() || !m_activeResult->valid || !m_activeResult->hasPoints()) {
        m_resultTimeSlider->setEnabled(false);
        m_resultTimeSlider->setRange(0, 0);
        m_resultTimeSlider->setValue(0);
        m_resultTimeLabel->setText("Time: --");
        return;
    }

    m_activeResultPointIndex = m_activeResult->clampedPointIndex(m_activeResultPointIndex < 0 ? m_activeResult->pointCount() - 1 : m_activeResultPointIndex);
    m_resultTimeSlider->setEnabled(true);
    m_resultTimeSlider->setRange(0, m_activeResult->pointCount() - 1);
    m_resultTimeSlider->setValue(m_activeResultPointIndex);

    if (const auto* point = m_activeResult->pointAt(m_activeResultPointIndex); point != nullptr) {
        m_resultTimeLabel->setText(QString("Time: %1 s").arg(point->time_s, 0, 'f', 0));
    }
}

void MainWindow::refreshSelectedGroupCharts()
{
    const auto clearGroupChart = [](ChartWidget* chart, const QString& title, const QString& yAxis) {
        if (chart != nullptr) {
            chart->showSingleSeries(title, yAxis, charts::Series{});
            chart->setMarkerTime(std::nullopt);
        }
    };

    if (!m_activeResult.has_value() || !m_activeResult->valid || m_selectedResultGroupIndex < 0) {
        clearGroupChart(m_groupVoltageChartView, "Selected Group Voltage", "Voltage (V)");
        clearGroupChart(m_groupCoreTemperatureChartView, "Selected Group Core Temperature", "Temperature (C)");
        clearGroupChart(m_groupSurfaceTemperatureChartView, "Selected Group Surface Temperature", "Temperature (C)");
        clearGroupChart(m_groupDiffusionStressChartView, "Selected Group Diffusion Stress", "Stress");
        clearGroupChart(m_groupHysteresisChartView, "Selected Group Hysteresis", "Voltage (V)");
        clearGroupChart(m_groupSocChartView, "Selected Group SOC", "SOC");
        return;
    }

    const QString groupName = m_activeResult->groupLabel(m_selectedResultGroupIndex);
    const auto markerTime = m_activeResult->pointAt(m_activeResultPointIndex) != nullptr
        ? std::optional<double>(m_activeResult->pointAt(m_activeResultPointIndex)->time_s)
        : std::nullopt;

    m_groupVoltageChartView->showSingleSeries(
        QString("%1 Voltage").arg(groupName),
        "Voltage (V)",
        makeSeries(pointSeriesForGroupMetric(*m_activeResult, m_selectedResultGroupIndex, "group_voltage"), groupName, QColor(59, 130, 246))
    );
    m_groupCoreTemperatureChartView->showSingleSeries(
        QString("%1 Core Temperature").arg(groupName),
        "Temperature (C)",
        makeSeries(pointSeriesForGroupMetric(*m_activeResult, m_selectedResultGroupIndex, "group_core_temp"), groupName, QColor(245, 113, 61))
    );
    m_groupSurfaceTemperatureChartView->showSingleSeries(
        QString("%1 Surface Temperature").arg(groupName),
        "Temperature (C)",
        makeSeries(pointSeriesForGroupMetric(*m_activeResult, m_selectedResultGroupIndex, "group_surface_temp"), groupName, QColor(56, 189, 248))
    );
    m_groupDiffusionStressChartView->showSingleSeries(
        QString("%1 Diffusion Stress").arg(groupName),
        "Stress",
        makeSeries(pointSeriesForGroupMetric(*m_activeResult, m_selectedResultGroupIndex, "group_diffusion_stress"), groupName, QColor(168, 85, 247))
    );
    m_groupHysteresisChartView->showSingleSeries(
        QString("%1 Hysteresis").arg(groupName),
        "Voltage (V)",
        makeSeries(pointSeriesForGroupMetric(*m_activeResult, m_selectedResultGroupIndex, "group_hysteresis_v"), groupName, QColor(236, 72, 153))
    );
    m_groupSocChartView->showSingleSeries(
        QString("%1 SOC").arg(groupName),
        "SOC",
        makeSeries(pointSeriesForGroupMetric(*m_activeResult, m_selectedResultGroupIndex, "group_soc"), groupName, QColor(34, 197, 94))
    );
    m_groupVoltageChartView->setMarkerTime(markerTime);
    m_groupCoreTemperatureChartView->setMarkerTime(markerTime);
    m_groupSurfaceTemperatureChartView->setMarkerTime(markerTime);
    m_groupDiffusionStressChartView->setMarkerTime(markerTime);
    m_groupHysteresisChartView->setMarkerTime(markerTime);
    m_groupSocChartView->setMarkerTime(markerTime);
}

void MainWindow::clearSimulationVisualization()
{
    const auto clearChart = [](ChartWidget* chart, const QString& title, const QString& yAxis) {
        if (chart != nullptr) {
            chart->showSingleSeries(title, yAxis, charts::Series{});
            chart->setMarkerTime(std::nullopt);
        }
    };

    m_activeResult.reset();
    m_lastExportPayload = {};
    m_activeResultPointIndex = -1;
    m_selectedResultGroupIndex = -1;
    m_summaryLabel->setText("Run a simulation to view the desktop-first results. Capture a baseline when you want to compare design changes.");
    clearChart(m_voltageChartView, "Pack Voltage vs Time", "Voltage (V)");
    clearChart(m_currentChartView, "Current vs Time", "Current (A)");
    clearChart(m_powerChartView, "Pack Power vs Time", "Power (W)");
    clearChart(m_socChartView, "Average SOC vs Time", "SOC");
    clearChart(m_socEnvelopeChartView, "SOC Envelope", "SOC");
    clearChart(m_temperatureChartView, "Temperature vs Time", "Temperature (C)");
    clearChart(m_coreTemperatureChartView, "Max Core Temperature vs Time", "Temperature (C)");
    clearChart(m_surfaceTemperatureChartView, "Max Surface Temperature vs Time", "Temperature (C)");
    clearChart(m_groupVoltageChartView, "Selected Group Voltage", "Voltage (V)");
    clearChart(m_groupCoreTemperatureChartView, "Selected Group Core Temperature", "Temperature (C)");
    clearChart(m_groupSurfaceTemperatureChartView, "Selected Group Surface Temperature", "Temperature (C)");
    clearChart(m_groupDiffusionStressChartView, "Selected Group Diffusion Stress", "Stress");
    clearChart(m_groupHysteresisChartView, "Selected Group Hysteresis", "Voltage (V)");
    clearChart(m_groupSocChartView, "Selected Group SOC", "SOC");
    if (m_groupTable != nullptr) {
        m_groupTable->setRowCount(0);
    }
    if (m_resultTimeSlider != nullptr) {
        m_resultTimeSlider->setEnabled(false);
        m_resultTimeSlider->setRange(0, 0);
        m_resultTimeSlider->setValue(0);
    }
    if (m_resultTimeLabel != nullptr) {
        m_resultTimeLabel->setText("Time: --");
    }
    if (m_resultSelectionLabel != nullptr) {
        m_resultSelectionLabel->setText("Selected group: --");
    }
    if (m_resultOverlayLegendLabel != nullptr) {
        m_resultOverlayLegendLabel->setText("Overlay range: --");
    }
    if (m_groupDetailLabel != nullptr) {
        m_groupDetailLabel->setText("Select a group from the table or CAD view to inspect nonlinear state values.");
    }
    if (m_virtualTestComparisonTable != nullptr) {
        m_virtualTestComparisonTable->setRowCount(0);
    }
    if (m_cadWorkspaceView != nullptr) {
        m_cadWorkspaceView->clearSimulationOverlay();
    }
    if (m_outputText != nullptr) {
        m_outputText->clear();
    }
}

void MainWindow::handleResultScrubChanged(int value)
{
    if (!m_activeResult.has_value() || !m_activeResult->valid) {
        return;
    }
    m_activeResultPointIndex = m_activeResult->clampedPointIndex(value);
    refreshResultScrubber();
    refreshGroupTable();
    refreshGroupDetailPanel();
    refreshSelectedGroupCharts();
    refreshCadOverlay();
    refreshCharts();
}

void MainWindow::handleOverlayMetricChanged(int index)
{
    if (m_overlayMetricCombo != nullptr) {
        m_overlayMetricCombo->setStyleSheet(QString("border:1px solid %1;").arg(overlayMetricAccent(index).name()));
    }
    refreshCadOverlay();
}

void MainWindow::handleGroupSelectionChanged()
{
    if (m_isSyncingGroupPanel || m_groupTable == nullptr) {
        return;
    }
    const auto items = m_groupTable->selectionModel()->selectedRows();
    if (items.isEmpty()) {
        return;
    }
    m_selectedResultGroupIndex = items.front().row();
    if (m_activeResult.has_value()) {
        m_resultSelectionLabel->setText(QString("Selected group: %1").arg(m_activeResult->groupLabel(m_selectedResultGroupIndex)));
    }
    refreshGroupDetailPanel();
    refreshSelectedGroupCharts();
}

void MainWindow::loadSystemPresetCatalog()
{
    if (m_systemPresetCategoryCombo == nullptr || m_systemPresetCombo == nullptr) {
        return;
    }

    const SimulationClient::Result result = m_client.listSystemPresets();
    if (!result.ok) {
        if (m_systemPresetDescription != nullptr) {
            m_systemPresetDescription->setText(QString("Failed to load battery system presets.\n%1").arg(result.error));
        }
        return;
    }

    m_systemPresetCatalog = result.payload.value("presets").toArray();
    {
        const QSignalBlocker categoryBlocker(m_systemPresetCategoryCombo);
        m_systemPresetCategoryCombo->clear();
        m_systemPresetCategoryCombo->addItem("All Categories");
        for (const QJsonValue& value : result.payload.value("categories").toArray()) {
            m_systemPresetCategoryCombo->addItem(value.toString());
        }
    }
    rebuildSystemPresetOptions();

    const QString defaultPresetId = result.payload.value("default_preset_id").toString();
    {
        const QSignalBlocker presetBlocker(m_systemPresetCombo);
        for (int index = 0; index < m_systemPresetCombo->count(); ++index) {
            if (m_systemPresetCombo->itemData(index).toString() == defaultPresetId) {
                m_systemPresetCombo->setCurrentIndex(index);
                break;
            }
        }
    }
    handleSystemPresetCategoryChanged(m_systemPresetCombo->currentIndex());
}

void MainWindow::rebuildSystemPresetOptions()
{
    if (m_systemPresetCombo == nullptr) {
        return;
    }

    const QString selectedCategory = m_systemPresetCategoryCombo != nullptr ? m_systemPresetCategoryCombo->currentText() : QString();
    {
        const QSignalBlocker presetBlocker(m_systemPresetCombo);
        m_systemPresetCombo->clear();
        for (const QJsonValue& value : m_systemPresetCatalog) {
            const QJsonObject preset = value.toObject();
            if (!selectedCategory.isEmpty() && selectedCategory != "All Categories" && preset.value("category").toString() != selectedCategory) {
                continue;
            }
            m_systemPresetCombo->addItem(preset.value("display_name").toString(), preset.value("preset_id").toString());
        }
    }

    handleSystemPresetCategoryChanged(m_systemPresetCombo->currentIndex());
}

void MainWindow::handleSystemPresetCategoryChanged(int)
{
    if (sender() == m_systemPresetCategoryCombo) {
        appendDesktopStartupLog("system preset category changed");
        rebuildSystemPresetOptions();
        return;
    }

    if (m_systemPresetDescription == nullptr || m_systemPresetCombo == nullptr) {
        return;
    }

    const QString presetId = m_systemPresetCombo->currentData().toString();
    appendDesktopStartupLog(QString("system preset selection update | preset_id=%1").arg(presetId));
    for (const QJsonValue& value : m_systemPresetCatalog) {
        const QJsonObject preset = value.toObject();
        if (preset.value("preset_id").toString() != presetId) {
            continue;
        }
        const QStringList recommended = [&]() {
            QStringList output;
            for (const QJsonValue& item : preset.value("recommended_virtual_tests").toArray()) {
                output << item.toString();
            }
            return output;
        }();
        m_systemPresetDescription->setText(QString("%1\nChemistry: %2 | Cell: %3\nRecommended tests: %4")
            .arg(preset.value("description").toString())
            .arg(preset.value("chemistry_display_name").toString())
            .arg(preset.value("cell_preset_name").toString())
            .arg(recommended.isEmpty() ? "None" : recommended.join(", ")));
        break;
    }
}

void MainWindow::applySystemPreset(const QJsonObject& preset)
{
    if (preset.isEmpty()) {
        return;
    }

    appendDesktopStartupLog(QString("apply system preset begin | preset_id=%1").arg(preset.value("preset_id").toString()));
    m_activeSystemPreset = preset;
    const QJsonObject simulation = preset.value("simulation_defaults").toObject();
    applySimulationConfig(simulation);
    if (m_referencePreset != nullptr) {
        m_referencePreset->setCurrentIndex(0);
    }
    updateCadWorkspace();

    const QJsonArray recommendedTests = preset.value("recommended_virtual_tests").toArray();
    if (m_virtualTestCombo != nullptr && !recommendedTests.isEmpty()) {
        const QString firstTestId = recommendedTests.first().toString();
        for (int index = 0; index < m_virtualTestCombo->count(); ++index) {
            if (m_virtualTestCombo->itemData(index).toString() == firstTestId) {
                m_virtualTestCombo->setCurrentIndex(index);
                break;
            }
        }
    }

    if (m_summaryLabel != nullptr) {
        m_summaryLabel->setText(QString("Loaded battery system preset: %1").arg(preset.value("display_name").toString()));
    }
    appendDesktopStartupLog("apply system preset complete");
}

void MainWindow::applySelectedSystemPreset()
{
    if (m_systemPresetCombo == nullptr) {
        return;
    }

    const QString presetId = m_systemPresetCombo->currentData().toString();
    for (const QJsonValue& value : m_systemPresetCatalog) {
        const QJsonObject preset = value.toObject();
        if (preset.value("preset_id").toString() == presetId) {
            applySystemPreset(preset);
            return;
        }
    }
}

void MainWindow::loadVirtualTestCatalog()
{
    if (m_virtualTestCombo == nullptr) {
        return;
    }

    const SimulationClient::Result result = m_client.listVirtualTests();
    if (!result.ok) {
        setVirtualTestStatus(QString("Failed to load virtual test catalog.\n%1").arg(result.error), QColor(220, 78, 78));
        return;
    }

    m_virtualTestCatalog = result.payload.value("tests").toArray();
    m_virtualTestCombo->clear();
    for (const QJsonValue& value : m_virtualTestCatalog) {
        const QJsonObject test = value.toObject();
        m_virtualTestCombo->addItem(test.value("display_name").toString(), test.value("test_id").toString());
    }
    rebuildVirtualTestForm();
}

void MainWindow::rebuildVirtualTestForm()
{
    if (m_virtualTestFormLayout == nullptr || m_virtualTestCombo == nullptr) {
        return;
    }

    while (m_virtualTestFormLayout->rowCount() > 0) {
        m_virtualTestFormLayout->removeRow(0);
    }
    m_virtualTestFields.clear();

    const int index = m_virtualTestCombo->currentIndex();
    if (index < 0 || index >= m_virtualTestCatalog.size()) {
        if (m_virtualTestDescription != nullptr) {
            m_virtualTestDescription->setText("No virtual tests available.");
        }
        return;
    }

    const QJsonObject test = m_virtualTestCatalog.at(index).toObject();
    QString description = test.value("description").toString();
    description += QString("\nCategory: %1 | Difficulty: %2")
                       .arg(test.value("category").toString(), test.value("difficulty").toString());
    if (m_virtualTestDescription != nullptr) {
        m_virtualTestDescription->setText(description);
    }

    const QJsonArray parameters = test.value("parameters").toArray();
    for (const QJsonValue& value : parameters) {
        const QJsonObject parameter = value.toObject();
        const QString type = parameter.value("param_type").toString();
        const QString label = parameter.value("label").toString();
        const QString unit = parameter.value("unit").toString();
        const QString fullLabel = unit.isEmpty() ? label : QString("%1 (%2)").arg(label, unit);
        QWidget* editor = nullptr;
        if (type == "bool") {
            auto* check = new QCheckBox(this);
            check->setChecked(parameter.value("default_value").toBool());
            editor = check;
        } else if (type == "enum") {
            auto* combo = new QComboBox(this);
            for (const QJsonValue& item : parameter.value("allowed_values").toArray()) {
                combo->addItem(item.toString());
            }
            combo->setCurrentText(parameter.value("default_value").toString());
            editor = combo;
        } else {
            auto* line = new QLineEdit(this);
            const QJsonValue defaultValue = parameter.value("default_value");
            if (defaultValue.isArray()) {
                QStringList items;
                for (const QJsonValue& item : defaultValue.toArray()) {
                    items << QString::number(item.toDouble());
                }
                line->setText(items.join(", "));
            } else if (defaultValue.isObject()) {
                line->setText(QString::fromUtf8(QJsonDocument(defaultValue.toObject()).toJson(QJsonDocument::Compact)));
            } else if (defaultValue.isBool()) {
                line->setText(defaultValue.toBool() ? "true" : "false");
            } else if (defaultValue.isString()) {
                line->setText(defaultValue.toString());
            } else {
                line->setText(QString::number(defaultValue.toDouble()));
            }
            line->setPlaceholderText(parameter.value("tooltip").toString());
            editor = line;
        }
        if (editor != nullptr) {
            editor->setToolTip(parameter.value("tooltip").toString());
            m_virtualTestFormLayout->addRow(fullLabel, editor);
            m_virtualTestFields.push_back({
                parameter.value("key").toString(),
                type,
                parameter.value("required").toBool(true),
                editor,
            });
        }
    }

    setVirtualTestStatus("Choose parameters, then vet the selected workflow against the current pack and simulation setup.", QColor(90, 144, 203));
}

QJsonObject MainWindow::buildVirtualTestPayload() const
{
    QJsonObject parameters;
    for (const VirtualTestField& field : m_virtualTestFields) {
        if (field.editor == nullptr) {
            continue;
        }
        if (field.type == "bool") {
            const auto* check = qobject_cast<QCheckBox*>(field.editor);
            parameters.insert(field.key, check != nullptr && check->isChecked());
            continue;
        }
        if (field.type == "enum") {
            const auto* combo = qobject_cast<QComboBox*>(field.editor);
            parameters.insert(field.key, combo != nullptr ? combo->currentText() : QString());
            continue;
        }

        const auto* line = qobject_cast<QLineEdit*>(field.editor);
        const QString text = line != nullptr ? line->text().trimmed() : QString();
        if (field.type == "int") {
            parameters.insert(field.key, text.toInt());
        } else if (field.type == "float") {
            parameters.insert(field.key, text.toDouble());
        } else if (field.type == "list_float") {
            QJsonArray array;
            for (const QString& item : text.split(',', Qt::SkipEmptyParts)) {
                array.append(item.trimmed().toDouble());
            }
            parameters.insert(field.key, array);
        } else if (field.type == "json") {
            QJsonParseError parseError;
            const QJsonDocument json = QJsonDocument::fromJson(text.toUtf8(), &parseError);
            if (parseError.error == QJsonParseError::NoError) {
                if (json.isArray()) {
                    parameters.insert(field.key, json.array());
                } else if (json.isObject()) {
                    parameters.insert(field.key, json.object());
                }
            } else {
                parameters.insert(field.key, text);
            }
        } else {
            parameters.insert(field.key, text);
        }
    }

    return QJsonObject{
        {"test_id", m_virtualTestCombo != nullptr ? m_virtualTestCombo->currentData().toString() : QString()},
        {"base_config", buildSimulationConfig()},
        {"parameters", parameters},
    };
}

void MainWindow::setVirtualTestStatus(const QString& text, const QColor& accent)
{
    if (m_virtualTestStatus == nullptr) {
        return;
    }
    m_virtualTestStatus->setPlainText(text);
    m_virtualTestStatus->setStyleSheet(QString("border:1px solid %1;").arg(accent.name()));
}

void MainWindow::renderVirtualTestResult(const QJsonObject& payload)
{
    m_lastExportPayload = payload;
    const QString testName = payload.value("test_name").toString();
    const QJsonObject summary = payload.value("summary_metrics").toObject();
    const QJsonObject passFail = payload.value("pass_fail_indicators").toObject();
    const QJsonObject vetting = payload.value("vetting_result").toObject();
    const QJsonObject parametersUsed = payload.value("parameters_used").toObject();
    const QJsonArray runtimeWarnings = payload.value("warnings").toArray();

    QStringList lines;
    lines << QString("Test: %1").arg(testName);
    lines << QString("Vetting: %1").arg(vetting.value("is_valid").toBool() ? "ready" : "blocked");
    lines << "Parameters used:";
    for (auto it = parametersUsed.begin(); it != parametersUsed.end(); ++it) {
        lines << QString("- %1: %2").arg(it.key(), it.value().toVariant().toString());
    }
    const QJsonArray warnings = vetting.value("warnings").toArray();
    if (!warnings.isEmpty()) {
        lines << "Vetting warnings:";
        for (const QJsonValue& warning : warnings) {
            lines << QString("- %1").arg(warning.toString());
        }
    }
    if (!runtimeWarnings.isEmpty()) {
        lines << "Runtime warnings:";
        for (const QJsonValue& warning : runtimeWarnings) {
            lines << QString("- %1").arg(warning.toString());
        }
    }
    lines << "";
    lines << "Summary Metrics:";
    for (auto it = summary.begin(); it != summary.end(); ++it) {
        lines << QString("- %1: %2").arg(it.key(), it.value().toVariant().toString());
    }
    if (!passFail.isEmpty()) {
        lines << "";
        lines << "Checks:";
        for (auto it = passFail.begin(); it != passFail.end(); ++it) {
            lines << QString("- %1: %2").arg(it.key(), it.value().toBool() ? "pass" : "flagged");
        }
    }
    setVirtualTestStatus(lines.join('\n'), QColor(58, 165, 99));

    if (m_virtualTestComparisonTable != nullptr) {
        m_virtualTestComparisonTable->setRowCount(0);
        int row = 0;
        const QJsonArray subResults = payload.value("sub_results").toArray();
        for (const QJsonValue& value : subResults) {
            const QJsonObject sub = value.toObject();
            const QString scenarioName = sub.value("name").toString();
            const QJsonObject scenarioSummary = sub.value("summary_metrics").toObject();
            for (auto it = scenarioSummary.begin(); it != scenarioSummary.end(); ++it) {
                m_virtualTestComparisonTable->insertRow(row);
                m_virtualTestComparisonTable->setItem(row, 0, new QTableWidgetItem(scenarioName));
                m_virtualTestComparisonTable->setItem(row, 1, new QTableWidgetItem(it.key()));
                m_virtualTestComparisonTable->setItem(row, 2, new QTableWidgetItem(it.value().toVariant().toString()));
                ++row;
            }
        }
    }

    const QJsonObject primaryResult = payload.value("primary_result").toObject();
    if (!primaryResult.isEmpty()) {
        renderResult(primaryResult);
        return;
    }

    const QJsonArray subResults = payload.value("sub_results").toArray();
    for (const QJsonValue& value : subResults) {
        const QJsonObject sub = value.toObject();
        const QJsonObject simulationResult = sub.value("simulation_result").toObject();
        if (!simulationResult.isEmpty()) {
            renderResult(simulationResult);
            return;
        }
    }
}

void MainWindow::handleVirtualTestSelectionChanged(int index)
{
    Q_UNUSED(index);
    rebuildVirtualTestForm();
}

void MainWindow::vetSelectedVirtualTest()
{
    const SimulationClient::Result result = m_client.vetVirtualTest(buildVirtualTestPayload());
    if (!result.ok) {
        setVirtualTestStatus(QString("Test vetting failed.\n%1").arg(result.error), QColor(220, 78, 78));
        return;
    }

    const QJsonObject vetting = result.payload.value("vetting_result").toObject();
    QStringList lines;
    const bool isValid = vetting.value("is_valid").toBool();
    lines << QString("Vetting: %1").arg(isValid ? "ready to run" : "blocked");
    const QJsonArray errors = vetting.value("errors").toArray();
    if (!errors.isEmpty()) {
        lines << "Errors:";
        for (const QJsonValue& error : errors) {
            lines << QString("- %1").arg(error.toString());
        }
    }
    const QJsonArray warnings = vetting.value("warnings").toArray();
    if (!warnings.isEmpty()) {
        lines << "Warnings:";
        for (const QJsonValue& warning : warnings) {
            lines << QString("- %1").arg(warning.toString());
        }
    }
    setVirtualTestStatus(lines.join('\n'), isValid ? (warnings.isEmpty() ? QColor(58, 165, 99) : QColor(214, 179, 67)) : QColor(220, 78, 78));
}

void MainWindow::runSelectedVirtualTest()
{
    const QJsonObject payload = buildVirtualTestPayload();
    const SimulationClient::Result vet = m_client.vetVirtualTest(payload);
    if (!vet.ok) {
        setVirtualTestStatus(QString("Test vetting failed.\n%1").arg(vet.error), QColor(220, 78, 78));
        return;
    }
    if (!vet.payload.value("vetting_result").toObject().value("is_valid").toBool()) {
        vetSelectedVirtualTest();
        return;
    }

    const SimulationClient::Result result = m_client.runVirtualTest(payload);
    if (!result.ok) {
        setVirtualTestStatus(QString("Virtual test failed.\n%1").arg(result.error), QColor(220, 78, 78));
        return;
    }
    renderVirtualTestResult(result.payload);
}

void MainWindow::exportActiveResultJson()
{
    if (m_lastExportPayload.isEmpty()) {
        QMessageBox::information(this, "No Result", "Run a simulation or virtual test before exporting results.");
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this,
        "Export Result JSON",
        QString(),
        "JSON Files (*.json)"
    );
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, "Export Failed", "Could not write the selected export file.");
        return;
    }

    file.write(QJsonDocument(m_lastExportPayload).toJson(QJsonDocument::Indented));
    m_summaryLabel->setText(QString("Exported canonical result payload to %1").arg(path));
}
