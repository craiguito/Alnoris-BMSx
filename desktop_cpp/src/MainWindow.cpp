#include "MainWindow.h"

#include "CadViewportWidget.h"
#include "ChartWidget.h"
#include "ProjectPersistence.h"
#include "TradeStudyReportExporter.h"
#include "TradeStudyResultsPanel.h"
#include "TradeStudySetupPanel.h"
#include "TradeStudyTestsPanel.h"
#include "TradeStudyWorkspacePanel.h"
#include "../cad/io/JsonCadDocumentIO.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <utility>

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

QString entityKindLabel(cad::battery::EntityKind kind)
{
    switch (kind) {
    case cad::battery::EntityKind::BatteryPack:
        return "Battery Pack";
    case cad::battery::EntityKind::CellGroup:
        return "Cell Group";
    case cad::battery::EntityKind::Cell:
        return "Cell";
    case cad::battery::EntityKind::Busbar:
        return "Busbar";
    case cad::battery::EntityKind::CoolingPlate:
        return "Cooling Channel";
    case cad::battery::EntityKind::ModuleBoundary:
        return "Battery Module";
    case cad::battery::EntityKind::PackEnclosure:
        return "Enclosure";
    }
    return "Entity";
}

struct CadCellDefaults
{
    cad::battery::CellFormFactor form_factor = cad::battery::CellFormFactor::Cylindrical;
    float radius_mm = 10.5f;
    float height_mm = 70.0f;
    float width_mm = 21.0f;
    float depth_mm = 21.0f;
    float x_spacing_mm = 23.0f;
    float z_spacing_mm = 23.0f;
};

CadCellDefaults fallbackCadCellDefaults(const QString& referencePreset)
{
    const QString normalized = referencePreset.trimmed().toLower();
    if (normalized.contains("26650")) {
        return {cad::battery::CellFormFactor::Cylindrical, 13.0f, 65.0f, 26.0f, 26.0f, 31.0f, 31.0f};
    }
    if (normalized.contains("21700")) {
        return {cad::battery::CellFormFactor::Cylindrical, 10.5f, 70.0f, 21.0f, 21.0f, 23.0f, 23.0f};
    }
    return {};
}

cad::battery::BatteryVisualizationOverlay buildSimulationOverlay(
    const cad::core::CadDocument& document,
    const desktop::SimulationResultModel& result,
    int pointIndex,
    cad::battery::BatteryVisualizationOverlay::Metric metric)
{
    cad::battery::BatteryVisualizationOverlay overlay;
    overlay.active_metric = metric;

    const desktop::SimulationTracePoint* point = result.pointAt(pointIndex);
    if (point == nullptr) {
        return overlay;
    }

    for (const cad::battery::CellEntity& cell : document.cells()) {
        int groupIndex = -1;
        if (!result.group_entity_ids.isEmpty()) {
            if (cell.parent_id.isValid()) {
                const QString parentGroupId = QString::number(static_cast<qulonglong>(cell.parent_id.value));
                groupIndex = result.group_entity_ids.indexOf(parentGroupId);
            }
            if (groupIndex < 0 && cell.series_index >= 0) {
                groupIndex = result.group_entity_ids.indexOf(QString("series-%1").arg(cell.series_index));
            }
        }
        if (groupIndex < 0) {
            groupIndex = cell.simulation_group_index >= 0 ? cell.simulation_group_index : cell.series_index;
        }
        if (groupIndex < 0) {
            continue;
        }
        if (groupIndex < static_cast<int>(point->group_core_temp_c.size())) {
            overlay.cell_core_temperature_c[cell.id] = point->group_core_temp_c[static_cast<std::size_t>(groupIndex)];
        }
        if (groupIndex < static_cast<int>(point->group_surface_temp_c.size())) {
            overlay.cell_surface_temperature_c[cell.id] = point->group_surface_temp_c[static_cast<std::size_t>(groupIndex)];
        }
        if (groupIndex < static_cast<int>(point->group_soc.size())) {
            overlay.cell_soc[cell.id] = point->group_soc[static_cast<std::size_t>(groupIndex)];
        }
        if (groupIndex < static_cast<int>(point->group_voltage_v.size())) {
            overlay.cell_voltage_v[cell.id] = point->group_voltage_v[static_cast<std::size_t>(groupIndex)];
        }
        if (groupIndex < static_cast<int>(point->group_diffusion_stress.size())) {
            overlay.cell_diffusion_stress[cell.id] = point->group_diffusion_stress[static_cast<std::size_t>(groupIndex)];
        }
        if (groupIndex < static_cast<int>(point->group_effective_resistance_ohm.size())) {
            overlay.cell_effective_resistance_ohm[cell.id] = point->group_effective_resistance_ohm[static_cast<std::size_t>(groupIndex)];
        }
    }

    return overlay;
}

std::optional<desktop::SimulationResultModel> parseOptionalResult(const QJsonObject& payload)
{
    if (payload.isEmpty()) {
        return std::nullopt;
    }
    const desktop::SimulationResultModel result = desktop::parseSimulationResultPayload(payload);
    if (!result.valid) {
        return std::nullopt;
    }
    return result;
}

} // namespace

MainWindow::MainWindow(QString projectRoot, QWidget* parent)
    : QMainWindow(parent)
    , m_client(std::move(projectRoot))
{
    setWindowTitle("Alnoris Trade Study Cockpit");
    resize(1560, 940);

    m_cadRefreshTimer = new QTimer(this);
    m_cadRefreshTimer->setSingleShot(true);
    m_cadRefreshTimer->setInterval(250);
    connect(m_cadRefreshTimer, &QTimer::timeout, this, &MainWindow::updateCadWorkspace);
    connect(&m_client, &SimulationClient::requestFinished, this, &MainWindow::handleBackendRequestFinished);

    m_setupPanel = new TradeStudySetupPanel(this);
    m_workspacePanel = new TradeStudyWorkspacePanel(this);
    m_testsPanel = new TradeStudyTestsPanel(this);
    m_resultsPanel = new TradeStudyResultsPanel(this);
    m_workspacePanel->workspaceView->setEditorInteractionsEnabled(false);

    connect(m_setupPanel->referencePreset, &QComboBox::currentIndexChanged, this, &MainWindow::applyReferencePreset);
    connect(m_setupPanel->referencePreset, &QComboBox::currentTextChanged, this, [this](const QString&) { scheduleCadWorkspaceUpdate(); });
    connect(m_setupPanel->systemPresetCombo, &QComboBox::currentIndexChanged, this, [this](int) { refreshSelectedSystemPresetDescription(); });
    connect(m_setupPanel->applySystemPresetButton, &QPushButton::clicked, this, &MainWindow::applySelectedSystemPreset);
    connect(m_setupPanel->updateLayoutButton, &QPushButton::clicked, this, &MainWindow::generateLayout);
    connect(m_setupPanel->runButton, &QPushButton::clicked, this, &MainWindow::runSimulation);
    connect(m_setupPanel->captureBaselineButton, &QPushButton::clicked, this, &MainWindow::captureBaseline);
    connect(m_setupPanel->compareBaselineButton, &QPushButton::clicked, this, &MainWindow::compareAgainstBaseline);
    connect(m_setupPanel->exportReportButton, &QPushButton::clicked, this, &MainWindow::exportTradeStudyReport);
    connect(m_setupPanel->saveProjectButton, &QPushButton::clicked, this, &MainWindow::saveProject);
    connect(m_setupPanel->loadProjectButton, &QPushButton::clicked, this, &MainWindow::loadProject);

    const auto bindCadRefresh = [this](QDoubleSpinBox* spinBox) {
        connect(
            spinBox,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            [this](double) { scheduleCadWorkspaceUpdate(); });
    };
    for (QDoubleSpinBox* spinBox : {
             m_setupPanel->cellNominalVoltage,
             m_setupPanel->cellFullVoltage,
             m_setupPanel->cellEmptyVoltage,
             m_setupPanel->cellCutoffVoltage,
             m_setupPanel->cellCapacity,
             m_setupPanel->cellsInSeries,
             m_setupPanel->cellsInParallel,
             m_setupPanel->internalResistance,
             m_setupPanel->ambientTemp,
             m_setupPanel->dischargeCurrent,
             m_setupPanel->duration,
             m_setupPanel->timeStep,
             m_setupPanel->initialSoc,
             m_setupPanel->packMass,
             m_setupPanel->packHeatCapacity,
             m_setupPanel->coolingCoeff,
         }) {
        bindCadRefresh(spinBox);
    }

    connect(m_workspacePanel->workspaceView, &CadViewportWidget::selectionChanged, this, &MainWindow::refreshWorkspaceSummary);
    connect(m_resultsPanel->overlayMetricCombo, &QComboBox::currentIndexChanged, this, &MainWindow::handleOverlayMetricChanged);
    connect(m_resultsPanel->resultTimeSlider, &QSlider::valueChanged, this, &MainWindow::handleResultScrubChanged);
    connect(m_resultsPanel->groupTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::handleGroupSelectionChanged);
    connect(m_testsPanel->virtualTestCombo, &QComboBox::currentIndexChanged, this, &MainWindow::handleVirtualTestSelectionChanged);
    connect(m_testsPanel->vetTestButton, &QPushButton::clicked, this, &MainWindow::vetSelectedVirtualTest);
    connect(m_testsPanel->runTestButton, &QPushButton::clicked, this, &MainWindow::runSelectedVirtualTest);

    createMainToolbar();
    statusBar()->showMessage("Ready");

    auto* fileMenu = menuBar()->addMenu("File");
    auto* loadAction = fileMenu->addAction("Load Project", this, &MainWindow::loadProject);
    loadAction->setShortcut(QKeySequence::Open);
    auto* saveAction = fileMenu->addAction("Save Project", this, &MainWindow::saveProject);
    saveAction->setShortcut(QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction("Export Report", this, &MainWindow::exportTradeStudyReport);
    fileMenu->addAction("Export Canonical JSON", this, &MainWindow::exportActiveResultJson);
    fileMenu->addSeparator();
    auto* exitAction = fileMenu->addAction("Exit", this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);

    auto* workflowMenu = menuBar()->addMenu("Trade Study");
    auto* layoutAction = workflowMenu->addAction("Generate / Update Layout", this, &MainWindow::generateLayout);
    layoutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    auto* simulationRunAction = workflowMenu->addAction("Run Candidate Study", this, &MainWindow::runSimulation);
    simulationRunAction->setShortcut(QKeySequence(Qt::Key_F5));
    auto* simulationCaptureAction = workflowMenu->addAction("Capture Baseline", this, &MainWindow::captureBaseline);
    simulationCaptureAction->setShortcut(QKeySequence(Qt::Key_F6));
    workflowMenu->addAction("Compare to Baseline", this, &MainWindow::compareAgainstBaseline);
    workflowMenu->addAction("Run Selected Flagship Test", this, &MainWindow::runSelectedVirtualTest);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(10);

    auto* mainSplitter = new QSplitter(Qt::Horizontal, central);
    mainSplitter->setChildrenCollapsible(false);
    auto* rightTabs = new QTabWidget(mainSplitter);
    rightTabs->setMinimumWidth(360);
    rightTabs->addTab(m_testsPanel, "Flagship Tests");
    rightTabs->addTab(m_resultsPanel, "Results");

    mainSplitter->addWidget(m_setupPanel);
    mainSplitter->addWidget(m_workspacePanel);
    mainSplitter->addWidget(rightTabs);
    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 7);
    mainSplitter->setStretchFactor(2, 3);
    rootLayout->addWidget(mainSplitter, 1);

    setCentralWidget(central);

    applyTheme();
    updateCadWorkspace();
    refreshWorkspaceSummary();
    clearSimulationVisualization();

    QTimer::singleShot(0, this, [this]() {
        appendDesktopStartupLog("main window deferred initialization begin");
        loadSystemPresetCatalog();
        appendDesktopStartupLog("system preset catalog loaded");
        loadVirtualTestCatalog();
        appendDesktopStartupLog("virtual test catalog loaded");
        if (m_setupPanel->systemPresetCombo != nullptr && m_setupPanel->systemPresetCombo->count() > 0) {
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
    if (m_client.isBusy()) {
        statusBar()->showMessage("A simulator request is already running.", 4000);
        return;
    }

    flushPendingCadWorkspaceUpdate();
    m_reportContext = packStudyContext("Candidate Pack Study");
    m_pendingBackendAction = PendingBackendAction::RunSimulation;
    if (!m_client.runSimulationAsync(buildSimulationConfig())) {
        m_pendingBackendAction = PendingBackendAction::None;
        QMessageBox::critical(this, "Simulation Error", "Could not launch the simulation backend.");
        return;
    }

    setBackendBusy(true, "Running candidate study...");
}

void MainWindow::captureBaseline()
{
    if (m_client.isBusy()) {
        statusBar()->showMessage("A simulator request is already running.", 4000);
        return;
    }

    flushPendingCadWorkspaceUpdate();
    m_reportContext = packStudyContext("Baseline Reference");
    m_baselineConfig = buildSimulationConfig();
    m_pendingBackendAction = PendingBackendAction::CaptureBaseline;
    if (!m_client.runSimulationAsync(m_baselineConfig)) {
        m_pendingBackendAction = PendingBackendAction::None;
        QMessageBox::critical(this, "Simulation Error", "Could not launch the simulation backend.");
        return;
    }

    setBackendBusy(true, "Capturing baseline...");
}

void MainWindow::compareAgainstBaseline()
{
    if (m_client.isBusy()) {
        statusBar()->showMessage("A simulator request is already running.", 4000);
        return;
    }
    if (m_baselineConfig.isEmpty() || m_baselineResult.isEmpty()) {
        QMessageBox::information(this, "No Baseline", "Capture a baseline simulation before comparing.");
        return;
    }

    flushPendingCadWorkspaceUpdate();
    m_reportContext = packStudyContext("Candidate vs Baseline Trade Study");
    m_pendingBackendAction = PendingBackendAction::CompareAgainstBaseline;
    if (!m_client.runSimulationAsync(buildSimulationConfig())) {
        m_pendingBackendAction = PendingBackendAction::None;
        QMessageBox::critical(this, "Simulation Error", "Could not launch the simulation backend.");
        return;
    }

    setBackendBusy(true, "Comparing against baseline...");
}

void MainWindow::saveProject()
{
    if (m_client.isBusy()) {
        QMessageBox::information(this, "Busy", "Wait for the current simulator request to finish before saving.");
        return;
    }

    flushPendingCadWorkspaceUpdate();
    const QString path = QFileDialog::getSaveFileName(
        this,
        "Save Project",
        QString(),
        "Alnoris Project (*.json)");
    if (path.isEmpty()) {
        return;
    }

    trade_study::ProjectState state;
    state.referencePresetIndex = m_setupPanel->referencePreset->currentIndex();
    state.comparisonActive = m_activeComparisonSummary.has_value();
    state.systemPreset = m_activeSystemPreset;
    state.simulationConfig = buildSimulationConfig();
    state.baselineConfig = m_baselineConfig;
    state.baselineResult = m_baselineResult;
    state.activeResult = m_activeResultPayload;
    state.reportContext = trade_study::reportContextToJson(m_reportContext);
    if (m_workspacePanel->workspaceView != nullptr) {
        state.cadDocument = cad::io::serializeCadDocument(m_workspacePanel->workspaceView->document());
    }

    QString error;
    if (!trade_study::saveProjectState(path, state, &error)) {
        QMessageBox::critical(this, "Save Failed", error);
        return;
    }

    m_resultsPanel->summaryLabel->setText(QString("Saved project to %1").arg(path));
}

void MainWindow::loadProject()
{
    if (m_client.isBusy()) {
        QMessageBox::information(this, "Busy", "Wait for the current simulator request to finish before loading a project.");
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this,
        "Load Project",
        QString(),
        "Alnoris Project (*.json)");
    if (path.isEmpty()) {
        return;
    }

    QString error;
    const std::optional<trade_study::ProjectState> state = trade_study::loadProjectState(path, &error);
    if (!state.has_value()) {
        QMessageBox::critical(this, "Load Failed", error);
        return;
    }

    if (m_cadRefreshTimer != nullptr) {
        m_cadRefreshTimer->stop();
    }

    applySimulationConfig(state->simulationConfig);
    m_activeSystemPreset = state->systemPreset;
    m_baselineConfig = state->baselineConfig;
    m_baselineResult = state->baselineResult;
    m_activeResultPayload = state->activeResult;
    m_reportContext = trade_study::reportContextFromJson(state->reportContext);
    m_setupPanel->referencePreset->setCurrentIndex(state->referencePresetIndex);

    if (m_setupPanel->systemPresetCombo != nullptr && !m_activeSystemPreset.isEmpty()) {
        const QString presetId = m_activeSystemPreset.value("preset_id").toString();
        for (int index = 0; index < m_setupPanel->systemPresetCombo->count(); ++index) {
            if (m_setupPanel->systemPresetCombo->itemData(index).toString() == presetId) {
                m_setupPanel->systemPresetCombo->setCurrentIndex(index);
                break;
            }
        }
    }
    refreshSelectedSystemPresetDescription();

    bool loadedCadDocument = false;
    QString cadDocumentError;
    if (m_workspacePanel->workspaceView != nullptr && !state->cadDocument.isEmpty()) {
        cad::core::CadDocument cadDocument;
        QJsonObject root;
        root.insert("cad_document", state->cadDocument);
        if (cad::io::tryLoadCadDocumentFromProject(root, cadDocument, &cadDocumentError)) {
            cad::battery::BatteryCadConfig cadConfig = buildCadWorkspaceConfig();
            if (cadDocument.metadata().layout_config.cells_in_series > 0 || cadDocument.metadata().layout_config.cells_in_parallel > 0) {
                cadConfig.layout = cadDocument.metadata().layout_config;
            }
            m_workspacePanel->workspaceView->loadDocument(std::move(cadDocument), cadConfig);
            loadedCadDocument = true;
        }
    }
    if (!loadedCadDocument) {
        updateCadWorkspace();
        if (!cadDocumentError.isEmpty()) {
            QMessageBox::warning(
                this,
                "CAD Document Warning",
                QString("The saved CAD document could not be loaded, so the workspace was rebuilt from simulation config.\n\n%1")
                    .arg(cadDocumentError));
        }
    }

    refreshWorkspaceSummary();

    if (!m_activeResultPayload.isEmpty()) {
        if (state->comparisonActive && !m_baselineResult.isEmpty()) {
            renderComparison(m_baselineResult, m_activeResultPayload);
        } else {
            renderResult(m_activeResultPayload);
        }
        m_resultsPanel->summaryLabel->setText(QString("Loaded project from %1.").arg(path));
        return;
    }

    clearSimulationVisualization();
    if (!m_baselineResult.isEmpty()) {
        if (const auto baseline = parseOptionalResult(m_baselineResult); baseline.has_value()) {
            const auto summary = trade_study::buildComparisonSummary(
                *baseline,
                std::nullopt,
                trade_study::buildPackSimulationReportContext(currentArchetypeId(), currentArchetypeName(), "Baseline Reference"));
            m_workspacePanel->reportNotes->setPlainText(trade_study::formatComparisonSummaryText(summary));
        }
        m_resultsPanel->summaryLabel->setText(QString("Loaded project from %1 with a saved baseline.").arg(path));
    } else {
        m_resultsPanel->summaryLabel->setText(QString("Loaded project from %1.").arg(path));
    }
}

void MainWindow::applyReferencePreset(int index)
{
    if (index > 0) {
        m_activeSystemPreset = {};
    }

    switch (index) {
    case 1:
        m_setupPanel->cellNominalVoltage->setValue(3.6);
        m_setupPanel->cellFullVoltage->setValue(4.2);
        m_setupPanel->cellEmptyVoltage->setValue(3.0);
        m_setupPanel->cellCutoffVoltage->setValue(3.0);
        m_setupPanel->cellCapacity->setValue(3.35);
        m_setupPanel->internalResistance->setValue(0.035);
        m_setupPanel->dischargeCurrent->setValue(1.675);
        m_setupPanel->packMass->setValue(0.048);
        m_setupPanel->packHeatCapacity->setValue(900.0);
        m_setupPanel->coolingCoeff->setValue(0.35);
        break;
    case 2:
        m_setupPanel->cellNominalVoltage->setValue(3.6);
        m_setupPanel->cellFullVoltage->setValue(4.2);
        m_setupPanel->cellEmptyVoltage->setValue(2.5);
        m_setupPanel->cellCutoffVoltage->setValue(2.5);
        m_setupPanel->cellCapacity->setValue(3.0);
        m_setupPanel->internalResistance->setValue(0.02);
        m_setupPanel->dischargeCurrent->setValue(3.0);
        m_setupPanel->packMass->setValue(0.046);
        m_setupPanel->packHeatCapacity->setValue(900.0);
        m_setupPanel->coolingCoeff->setValue(0.45);
        break;
    case 3:
        m_setupPanel->cellNominalVoltage->setValue(3.3);
        m_setupPanel->cellFullVoltage->setValue(3.6);
        m_setupPanel->cellEmptyVoltage->setValue(2.0);
        m_setupPanel->cellCutoffVoltage->setValue(2.0);
        m_setupPanel->cellCapacity->setValue(2.5);
        m_setupPanel->internalResistance->setValue(0.006);
        m_setupPanel->dischargeCurrent->setValue(7.5);
        m_setupPanel->packMass->setValue(0.076);
        m_setupPanel->packHeatCapacity->setValue(950.0);
        m_setupPanel->coolingCoeff->setValue(0.55);
        break;
    default:
        break;
    }
}

void MainWindow::generateLayout()
{
    flushPendingCadWorkspaceUpdate();
    m_resultsPanel->summaryLabel->setText("Generated layout and thermal zoning are refreshed. The workspace is ready for a flagship test or baseline run.");
}

void MainWindow::exportTradeStudyReport()
{
    if (!m_activeResult.has_value() || !m_activeResult->valid) {
        QMessageBox::information(this, "No Result", "Run a candidate study or flagship test before exporting a trade-study report.");
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this,
        "Export Trade-Study Report",
        QString(),
        "Markdown Report (*.md)");
    if (path.isEmpty()) {
        return;
    }

    const std::optional<desktop::SimulationResultModel> baseline = parseOptionalResult(m_baselineResult);
    const trade_study::ComparisonSummary summary = m_activeComparisonSummary.has_value()
        ? *m_activeComparisonSummary
        : trade_study::buildComparisonSummary(*m_activeResult, baseline, m_reportContext);

    QString error;
    if (!trade_study::writeMarkdownReport(path, summary, &error)) {
        QMessageBox::critical(this, "Export Failed", error);
        return;
    }

    m_resultsPanel->summaryLabel->setText(QString("Exported trade-study report to %1").arg(path));
}

void MainWindow::createMainToolbar()
{
    auto* toolbar = addToolBar("Primary");
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addAction("Load Project", this, &MainWindow::loadProject);
    toolbar->addAction("Save Project", this, &MainWindow::saveProject);
    toolbar->addSeparator();
    toolbar->addAction("Update Layout", this, &MainWindow::generateLayout);
    toolbar->addAction("Run Candidate", this, &MainWindow::runSimulation);
    toolbar->addAction("Capture Baseline", this, &MainWindow::captureBaseline);
    toolbar->addAction("Compare to Baseline", this, &MainWindow::compareAgainstBaseline);
    toolbar->addAction("Export Report", this, &MainWindow::exportTradeStudyReport);
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
        "QScrollArea { border:none; background:transparent; }")
            .arg(appBg, text, fieldBg, border, accent));

    if (m_workspacePanel != nullptr && m_workspacePanel->workspaceView != nullptr) {
        m_workspacePanel->workspaceView->setBackgroundColor(m_theme.cadBackground);
    }

    applyChartTheme(m_resultsPanel != nullptr ? m_resultsPanel->voltageChartView : nullptr);
    applyChartTheme(m_resultsPanel != nullptr ? m_resultsPanel->powerChartView : nullptr);
    applyChartTheme(m_resultsPanel != nullptr ? m_resultsPanel->temperatureChartView : nullptr);
    applyChartTheme(m_resultsPanel != nullptr ? m_resultsPanel->socEnvelopeChartView : nullptr);
}

void MainWindow::applyChartTheme(ChartWidget* graphWidget)
{
    if (graphWidget == nullptr) {
        return;
    }

    graphWidget->setThemeColors(m_theme.chartBackground, m_theme.textColor);
}

QJsonObject MainWindow::buildSimulationConfig() const
{
    QJsonObject config = m_activeSystemPreset.value("simulation_defaults").toObject();
    config.insert("cell_nominal_voltage", m_setupPanel->cellNominalVoltage->value());
    config.insert("cell_full_voltage", m_setupPanel->cellFullVoltage->value());
    config.insert("cell_empty_voltage", m_setupPanel->cellEmptyVoltage->value());
    config.insert("cell_cutoff_voltage", m_setupPanel->cellCutoffVoltage->value());
    config.insert("cell_capacity_ah", m_setupPanel->cellCapacity->value());
    config.insert("cells_in_series", static_cast<int>(m_setupPanel->cellsInSeries->value()));
    config.insert("cells_in_parallel", static_cast<int>(m_setupPanel->cellsInParallel->value()));
    config.insert("internal_resistance_ohm_per_cell", m_setupPanel->internalResistance->value());
    config.insert("ambient_temp_c", m_setupPanel->ambientTemp->value());
    config.insert("discharge_current_a", m_setupPanel->dischargeCurrent->value());
    config.insert("duration_s", static_cast<int>(m_setupPanel->duration->value()));
    config.insert("time_step_s", static_cast<int>(m_setupPanel->timeStep->value()));
    config.insert("initial_soc", m_setupPanel->initialSoc->value());
    config.insert("pack_mass_kg", m_setupPanel->packMass->value());
    config.insert("pack_heat_capacity_j_per_kgk", m_setupPanel->packHeatCapacity->value());
    config.insert("cooling_coeff_w_per_k", m_setupPanel->coolingCoeff->value());

    const SimulationMappingBuilder::MappingResult mapping = buildSimulationMapping();
    config.insert("group_count", mapping.groupCount);
    config.insert("thermal_zones", mapping.thermalZones);
    config.insert("group_zone_assignments", mapping.groupZoneAssignments);
    config.insert("group_labels", mapping.groupLabels);
    config.insert("group_entity_ids", mapping.groupEntityIds);

    return config;
}

SimulationMappingBuilder::MappingResult MainWindow::buildSimulationMapping() const
{
    if (m_workspacePanel != nullptr && m_workspacePanel->workspaceView != nullptr) {
        return SimulationMappingBuilder::build(
            m_workspacePanel->workspaceView->document(),
            m_setupPanel->ambientTemp->value(),
            m_setupPanel->coolingCoeff->value());
    }

    SimulationMappingBuilder::MappingResult mapping;
    mapping.groupCount = std::max(1, static_cast<int>(m_setupPanel->cellsInSeries->value()));
    mapping.thermalZones.append(QJsonObject{
        {"zone_id", 0},
        {"name", "Default Zone"},
        {"ambient_temp_c", m_setupPanel->ambientTemp->value()},
        {"cooling_coeff_w_per_k", m_setupPanel->coolingCoeff->value()},
    });
    return mapping;
}

void MainWindow::applySimulationConfig(const QJsonObject& config)
{
    if (config.isEmpty()) {
        return;
    }

    m_setupPanel->cellNominalVoltage->setValue(config.value("cell_nominal_voltage").toDouble(m_setupPanel->cellNominalVoltage->value()));
    m_setupPanel->cellFullVoltage->setValue(config.value("cell_full_voltage").toDouble(m_setupPanel->cellFullVoltage->value()));
    m_setupPanel->cellEmptyVoltage->setValue(config.value("cell_empty_voltage").toDouble(m_setupPanel->cellEmptyVoltage->value()));
    m_setupPanel->cellCutoffVoltage->setValue(config.value("cell_cutoff_voltage").toDouble(m_setupPanel->cellCutoffVoltage->value()));
    m_setupPanel->cellCapacity->setValue(config.value("cell_capacity_ah").toDouble(m_setupPanel->cellCapacity->value()));
    m_setupPanel->cellsInSeries->setValue(config.value("cells_in_series").toInt(static_cast<int>(m_setupPanel->cellsInSeries->value())));
    m_setupPanel->cellsInParallel->setValue(config.value("cells_in_parallel").toInt(static_cast<int>(m_setupPanel->cellsInParallel->value())));
    m_setupPanel->internalResistance->setValue(config.value("internal_resistance_ohm_per_cell").toDouble(m_setupPanel->internalResistance->value()));
    m_setupPanel->ambientTemp->setValue(config.value("ambient_temp_c").toDouble(m_setupPanel->ambientTemp->value()));
    m_setupPanel->dischargeCurrent->setValue(config.value("discharge_current_a").toDouble(m_setupPanel->dischargeCurrent->value()));
    m_setupPanel->duration->setValue(config.value("duration_s").toInt(static_cast<int>(m_setupPanel->duration->value())));
    m_setupPanel->timeStep->setValue(config.value("time_step_s").toInt(static_cast<int>(m_setupPanel->timeStep->value())));
    m_setupPanel->initialSoc->setValue(config.value("initial_soc").toDouble(m_setupPanel->initialSoc->value()));
    m_setupPanel->packMass->setValue(config.value("pack_mass_kg").toDouble(m_setupPanel->packMass->value()));
    m_setupPanel->packHeatCapacity->setValue(config.value("pack_heat_capacity_j_per_kgk").toDouble(m_setupPanel->packHeatCapacity->value()));
    m_setupPanel->coolingCoeff->setValue(config.value("cooling_coeff_w_per_k").toDouble(m_setupPanel->coolingCoeff->value()));
}

cad::battery::BatteryCadConfig MainWindow::buildCadWorkspaceConfig() const
{
    cad::battery::BatteryCadConfig cadConfig;
    const QJsonObject cadDefaults = m_activeSystemPreset.value("cad_defaults").toObject();
    const CadCellDefaults fallbackDefaults = fallbackCadCellDefaults(
        m_setupPanel != nullptr ? m_setupPanel->referencePreset->currentText() : QString());
    cadConfig.layout.preset_name = !cadDefaults.isEmpty()
        ? currentArchetypeName().toStdString()
        : (m_setupPanel != nullptr ? m_setupPanel->referencePreset->currentText().toStdString() : std::string("Custom"));
    cadConfig.layout.cells_in_series = static_cast<int>(m_setupPanel->cellsInSeries->value());
    cadConfig.layout.cells_in_parallel = static_cast<int>(m_setupPanel->cellsInParallel->value());
    cadConfig.layout.module_count = cadDefaults.value("module_count").toInt(1);
    cadConfig.layout.cell_form_factor = cadDefaults.isEmpty()
        ? fallbackDefaults.form_factor
        : cellFormFactorFromString(cadDefaults.value("cell_form_factor").toString("cylindrical"));
    cadConfig.electrical.cell_nominal_voltage = m_setupPanel->cellNominalVoltage->value();
    cadConfig.electrical.cell_capacity_ah = m_setupPanel->cellCapacity->value();
    cadConfig.thermal.ambient_temp_c = m_setupPanel->ambientTemp->value();
    cadConfig.electrical.internal_resistance_ohm = m_setupPanel->internalResistance->value();
    cadConfig.electrical.discharge_current_a = m_setupPanel->dischargeCurrent->value();
    cadConfig.thermal.pack_mass_kg = m_setupPanel->packMass->value();
    cadConfig.thermal.cooling_coeff_w_per_k = m_setupPanel->coolingCoeff->value();
    cadConfig.electrical.initial_soc = m_setupPanel->initialSoc->value();
    cadConfig.metrics.pack_voltage = m_setupPanel->cellNominalVoltage->value() * m_setupPanel->cellsInSeries->value();
    cadConfig.metrics.pack_capacity_ah = m_setupPanel->cellCapacity->value() * m_setupPanel->cellsInParallel->value();
    cadConfig.layout.cell_radius = static_cast<float>(cadDefaults.value("cell_radius_mm").toDouble(fallbackDefaults.radius_mm));
    cadConfig.layout.cell_height = static_cast<float>(cadDefaults.value("cell_height_mm").toDouble(fallbackDefaults.height_mm));
    cadConfig.layout.cell_width = static_cast<float>(cadDefaults.value("cell_width_mm").toDouble(fallbackDefaults.width_mm));
    cadConfig.layout.cell_depth = static_cast<float>(cadDefaults.value("cell_depth_mm").toDouble(fallbackDefaults.depth_mm));
    cadConfig.layout.top_cap_outer_diameter = static_cast<float>(cadDefaults.value("top_cap_outer_diameter_mm").toDouble(cadConfig.layout.top_cap_outer_diameter));
    cadConfig.layout.top_cap_inner_diameter = static_cast<float>(cadDefaults.value("top_cap_inner_diameter_mm").toDouble(cadConfig.layout.top_cap_inner_diameter));
    cadConfig.layout.top_cap_shoulder_height = static_cast<float>(cadDefaults.value("top_cap_shoulder_height_mm").toDouble(cadConfig.layout.top_cap_shoulder_height));
    cadConfig.layout.positive_terminal_diameter = static_cast<float>(cadDefaults.value("positive_terminal_diameter_mm").toDouble(cadConfig.layout.positive_terminal_diameter));
    cadConfig.layout.positive_terminal_height = static_cast<float>(cadDefaults.value("positive_terminal_height_mm").toDouble(cadConfig.layout.positive_terminal_height));
    cadConfig.layout.insulating_ring_outer_diameter = static_cast<float>(cadDefaults.value("insulating_ring_outer_diameter_mm").toDouble(cadConfig.layout.insulating_ring_outer_diameter));
    cadConfig.layout.insulating_ring_inner_diameter = static_cast<float>(cadDefaults.value("insulating_ring_inner_diameter_mm").toDouble(cadConfig.layout.insulating_ring_inner_diameter));
    cadConfig.layout.insulating_ring_height = static_cast<float>(cadDefaults.value("insulating_ring_height_mm").toDouble(cadConfig.layout.insulating_ring_height));
    cadConfig.layout.x_spacing = static_cast<float>(cadDefaults.value("x_spacing_mm").toDouble(fallbackDefaults.x_spacing_mm));
    cadConfig.layout.z_spacing = static_cast<float>(cadDefaults.value("z_spacing_mm").toDouble(fallbackDefaults.z_spacing_mm));
    cadConfig.layout.module_gap_x = static_cast<float>(cadDefaults.value("module_gap_x_mm").toDouble(cadConfig.layout.module_gap_x));
    cadConfig.layout.busbar_thickness = static_cast<float>(cadDefaults.value("busbar_thickness_mm").toDouble(cadConfig.layout.busbar_thickness));
    cadConfig.layout.busbar_width = static_cast<float>(cadDefaults.value("busbar_width_mm").toDouble(cadConfig.layout.busbar_width));
    cadConfig.layout.busbar_terminal_clearance = static_cast<float>(cadDefaults.value("busbar_terminal_clearance_mm").toDouble(cadConfig.layout.busbar_terminal_clearance));
    cadConfig.layout.busbar_support_offset = static_cast<float>(cadDefaults.value("busbar_support_offset_mm").toDouble(cadConfig.layout.busbar_support_offset));
    cadConfig.layout.busbar_overlap_width = static_cast<float>(cadDefaults.value("busbar_overlap_width_mm").toDouble(cadConfig.layout.busbar_overlap_width));
    cadConfig.layout.cooling_channel_thickness = static_cast<float>(cadDefaults.value("cooling_channel_thickness_mm").toDouble(cadConfig.layout.cooling_channel_thickness));
    cadConfig.layout.cooling_channel_depth = static_cast<float>(cadDefaults.value("cooling_channel_depth_mm").toDouble(cadConfig.layout.cooling_channel_depth));
    cadConfig.layout.cooling_plate_margin_x = static_cast<float>(cadDefaults.value("cooling_plate_margin_x_mm").toDouble(cadConfig.layout.cooling_plate_margin_x));
    cadConfig.layout.cooling_plate_margin_z = static_cast<float>(cadDefaults.value("cooling_plate_margin_z_mm").toDouble(cadConfig.layout.cooling_plate_margin_z));
    cadConfig.layout.cooling_plate_offset_below_tray = static_cast<float>(cadDefaults.value("cooling_plate_offset_below_tray_mm").toDouble(cadConfig.layout.cooling_plate_offset_below_tray));
    cadConfig.layout.enclosure_wall_thickness = static_cast<float>(cadDefaults.value("enclosure_wall_thickness_mm").toDouble(cadConfig.layout.enclosure_wall_thickness));
    cadConfig.layout.enclosure_floor_thickness = static_cast<float>(cadDefaults.value("enclosure_floor_thickness_mm").toDouble(cadConfig.layout.enclosure_floor_thickness));
    cadConfig.layout.enclosure_floor_offset = static_cast<float>(cadDefaults.value("enclosure_floor_offset_mm").toDouble(cadConfig.layout.enclosure_floor_offset));
    cadConfig.layout.enclosure_clearance_x = static_cast<float>(cadDefaults.value("enclosure_clearance_x_mm").toDouble(cadConfig.layout.enclosure_clearance_x));
    cadConfig.layout.enclosure_clearance_z = static_cast<float>(cadDefaults.value("enclosure_clearance_z_mm").toDouble(cadConfig.layout.enclosure_clearance_z));
    cadConfig.layout.module_tray_base_thickness = static_cast<float>(cadDefaults.value("tray_base_thickness_mm").toDouble(cadConfig.layout.module_tray_base_thickness));
    cadConfig.layout.module_tray_wall_thickness = static_cast<float>(cadDefaults.value("tray_wall_thickness_mm").toDouble(cadConfig.layout.module_tray_wall_thickness));
    cadConfig.layout.module_tray_wall_height = static_cast<float>(cadDefaults.value("tray_wall_height_mm").toDouble(cadConfig.layout.module_tray_wall_height));
    cadConfig.layout.cell_seating_offset = static_cast<float>(cadDefaults.value("cell_seating_offset_mm").toDouble(cadConfig.layout.cell_seating_offset));
    cadConfig.layout.module_tray_margin_x = static_cast<float>(cadDefaults.value("tray_margin_x_mm").toDouble(cadConfig.layout.module_tray_margin_x));
    cadConfig.layout.module_tray_margin_z = static_cast<float>(cadDefaults.value("tray_margin_z_mm").toDouble(cadConfig.layout.module_tray_margin_z));
    return cadConfig;
}

void MainWindow::updateCadWorkspace()
{
    if (m_cadRefreshTimer != nullptr) {
        m_cadRefreshTimer->stop();
    }
    if (m_workspacePanel->workspaceView == nullptr) {
        return;
    }

    m_workspacePanel->workspaceView->setPackConfig(buildCadWorkspaceConfig());
    refreshCadOverlay();
    refreshWorkspaceSummary();
}

void MainWindow::scheduleCadWorkspaceUpdate()
{
    if (m_cadRefreshTimer == nullptr) {
        updateCadWorkspace();
        return;
    }
    m_cadRefreshTimer->start();
}

void MainWindow::flushPendingCadWorkspaceUpdate()
{
    if (m_cadRefreshTimer != nullptr && m_cadRefreshTimer->isActive()) {
        m_cadRefreshTimer->stop();
        updateCadWorkspace();
    }
}

void MainWindow::refreshWorkspaceSummary()
{
    if (m_workspacePanel == nullptr || m_workspacePanel->workspaceView == nullptr) {
        return;
    }

    const cad::battery::BatteryCadConfig cadConfig = buildCadWorkspaceConfig();
    const SimulationMappingBuilder::MappingResult mapping = buildSimulationMapping();
    m_workspacePanel->layoutSummaryLabel->setText(QString(
        "Archetype: %1\nLayout: %2s%3p | Nominal pack: %4 V | Pack capacity: %5 Ah")
            .arg(currentArchetypeName().isEmpty() ? QString("Custom") : currentArchetypeName())
            .arg(cadConfig.layout.cells_in_series)
            .arg(cadConfig.layout.cells_in_parallel)
            .arg(formatMaybeNumber(cadConfig.metrics.pack_voltage, 2))
            .arg(formatMaybeNumber(cadConfig.metrics.pack_capacity_ah, 2)));

    QStringList zoneNames;
    for (int index = 0; index < mapping.thermalZones.size() && index < 4; ++index) {
        zoneNames << mapping.thermalZones.at(index).toObject().value("name").toString();
    }
    if (mapping.thermalZones.size() > zoneNames.size()) {
        zoneNames << QString("+%1 more").arg(mapping.thermalZones.size() - zoneNames.size());
    }
    m_workspacePanel->thermalZoneSummaryLabel->setText(QString(
        "Thermal zoning: %1 simulation groups mapped across %2 zones.\nZones: %3")
            .arg(mapping.groupCount)
            .arg(mapping.thermalZones.size())
            .arg(zoneNames.isEmpty() ? QString("Default Zone") : zoneNames.join(", ")));

    const auto selected = m_workspacePanel->workspaceView->selectedEntitySummary();
    if (!selected.has_value()) {
        m_workspacePanel->workspaceSelectionLabel->setText(
            "Workspace focus: preview-only. Click a group, module, or cooling channel to inspect the generated mapping.");
        return;
    }

    int selectedGroupIndex = selected->simulation_group_index;
    if (m_activeResult.has_value() && m_activeResult->valid) {
        if (selectedGroupIndex < 0) {
            const QString entityId = QString::number(static_cast<qulonglong>(selected->id.value));
            selectedGroupIndex = m_activeResult->group_entity_ids.indexOf(entityId);
        }
        if (selectedGroupIndex < 0 && selected->parent_id.isValid()) {
            const QString parentId = QString::number(static_cast<qulonglong>(selected->parent_id.value));
            selectedGroupIndex = m_activeResult->group_entity_ids.indexOf(parentId);
        }
        if (selectedGroupIndex >= 0 && selectedGroupIndex < m_activeResult->groupCount() && selectedGroupIndex != m_selectedResultGroupIndex) {
            m_selectedResultGroupIndex = selectedGroupIndex;
            refreshGroupTable();
            refreshGroupDetailPanel();
        }
    }

    QStringList details;
    details << QString("Selected: %1").arg(entityKindLabel(selected->kind));
    if (!selected->label.empty()) {
        details << QString("Label: %1").arg(QString::fromStdString(selected->label));
    }
    details << QString("Entity ID: %1").arg(static_cast<qulonglong>(selected->id.value));
    if (selectedGroupIndex >= 0 && m_activeResult.has_value() && m_activeResult->valid && selectedGroupIndex < m_activeResult->groupCount()) {
        details << QString("Simulation group: %1").arg(m_activeResult->groupLabel(selectedGroupIndex));
    }
    m_workspacePanel->workspaceSelectionLabel->setText(details.join(" | "));
}

void MainWindow::setBackendBusy(bool busy, const QString& statusText)
{
    if (m_backendBusy == busy) {
        if (!statusText.isEmpty()) {
            statusBar()->showMessage(statusText);
        }
        return;
    }

    m_backendBusy = busy;
    if (QWidget* widget = centralWidget()) {
        widget->setEnabled(!busy);
    }
    if (menuBar() != nullptr) {
        menuBar()->setEnabled(!busy);
    }
    for (QToolBar* toolbar : findChildren<QToolBar*>()) {
        toolbar->setEnabled(!busy);
    }

    if (busy) {
        QApplication::setOverrideCursor(Qt::BusyCursor);
        statusBar()->showMessage(statusText);
    } else {
        if (QApplication::overrideCursor() != nullptr) {
            QApplication::restoreOverrideCursor();
        }
        statusBar()->showMessage(statusText.isEmpty() ? "Ready" : statusText, 4000);
    }
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

void MainWindow::renderResult(const QJsonObject& payload)
{
    m_activeResultPayload = payload;
    m_activeComparisonSummary.reset();
    m_activeResult = desktop::parseSimulationResultPayload(payload);
    if (!m_activeResult->valid) {
        QMessageBox::warning(this, "Simulation Result Error", m_activeResult->error);
        clearSimulationVisualization();
        return;
    }

    if (m_reportContext.workflowName.isEmpty()) {
        m_reportContext = packStudyContext("Candidate Pack Study");
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

    m_activeResultPayload = candidatePayload;
    m_activeResult = candidate;
    m_activeComparisonSummary = trade_study::buildComparisonSummary(candidate, baseline, m_reportContext);
    m_activeResultPointIndex = candidate.pointCount() - 1;
    if (m_selectedResultGroupIndex < 0) {
        m_selectedResultGroupIndex = candidate.summary.weakest_group_index >= 0 ? candidate.summary.weakest_group_index : 0;
    }
    refreshSimulationViews();
}

void MainWindow::refreshSimulationViews()
{
    if (!m_activeResult.has_value() || !m_activeResult->valid) {
        clearSimulationVisualization();
        return;
    }

    refreshResultScrubber();
    refreshCharts();
    refreshGroupTable();
    refreshGroupDetailPanel();
    refreshCadOverlay();
    refreshWorkspaceSummary();

    if (m_activeComparisonSummary.has_value()) {
        m_resultsPanel->summaryLabel->setText(
            "Candidate-vs-baseline deltas are active. The workspace stays synced to the candidate while charts and report preview stay focused on the trade-off.");
        m_workspacePanel->reportNotes->setPlainText(trade_study::formatComparisonSummaryText(*m_activeComparisonSummary));
        return;
    }

    const trade_study::ComparisonSummary summary = trade_study::buildComparisonSummary(*m_activeResult, parseOptionalResult(m_baselineResult), m_reportContext);
    m_resultsPanel->summaryLabel->setText(
        "Candidate results are live. Scrub the workspace to inspect thermal zoning, then compare against baseline when you need a recommendation-ready delta view.");
    m_workspacePanel->reportNotes->setPlainText(trade_study::formatComparisonSummaryText(summary));
}

void MainWindow::refreshCharts()
{
    if (!m_activeResult.has_value() || !m_activeResult->valid) {
        return;
    }

    const auto markerTime = m_activeResult->pointAt(m_activeResultPointIndex) != nullptr
        ? std::optional<double>(m_activeResult->pointAt(m_activeResultPointIndex)->time_s)
        : std::nullopt;

    const std::optional<desktop::SimulationResultModel> baseline = m_activeComparisonSummary.has_value()
        ? parseOptionalResult(m_baselineResult)
        : std::nullopt;

    if (baseline.has_value()) {
        m_resultsPanel->voltageChartView->showComparison(
            "Pack Voltage Comparison",
            "Voltage (V)",
            makeSeries(pointSeriesForMetric(*baseline, "pack_voltage_v"), "Baseline", QColor(123, 135, 148), true),
            makeSeries(pointSeriesForMetric(*m_activeResult, "pack_voltage_v"), "Candidate", QColor(14, 165, 233)));
        m_resultsPanel->powerChartView->showComparison(
            "Pack Power Comparison",
            "Power (W)",
            makeSeries(pointSeriesForMetric(*baseline, "pack_power_w"), "Baseline", QColor(123, 135, 148), true),
            makeSeries(pointSeriesForMetric(*m_activeResult, "pack_power_w"), "Candidate", QColor(245, 158, 11)));
        m_resultsPanel->temperatureChartView->showComparison(
            "Core Temperature Comparison",
            "Temperature (C)",
            makeSeries(pointSeriesForMetric(*baseline, "group_core_temp_max_c"), "Baseline", QColor(123, 135, 148), true),
            makeSeries(pointSeriesForMetric(*m_activeResult, "group_core_temp_max_c"), "Candidate", QColor(245, 113, 61)));
        m_resultsPanel->socEnvelopeChartView->showComparison(
            "SOC Spread Comparison",
            "Spread",
            makeSeries(pointSeriesForMetric(*baseline, "soc_spread"), "Baseline", QColor(123, 135, 148), true),
            makeSeries(pointSeriesForMetric(*m_activeResult, "soc_spread"), "Candidate", QColor(34, 197, 94)));
    } else {
        m_resultsPanel->voltageChartView->showSingleSeries(
            "Pack Voltage vs Time",
            "Voltage (V)",
            makeSeries(pointSeriesForMetric(*m_activeResult, "pack_voltage_v"), "Current Scenario", QColor(14, 165, 233)));
        m_resultsPanel->powerChartView->showSingleSeries(
            "Pack Power vs Time",
            "Power (W)",
            makeSeries(pointSeriesForMetric(*m_activeResult, "pack_power_w"), "Current Scenario", QColor(245, 158, 11)));
        m_resultsPanel->temperatureChartView->showComparison(
            "Thermal Peak vs Time",
            "Temperature (C)",
            makeSeries(pointSeriesForMetric(*m_activeResult, "pack_temp_max_c"), "Pack Max", QColor(56, 189, 248), true),
            makeSeries(pointSeriesForMetric(*m_activeResult, "group_core_temp_max_c"), "Core Max", QColor(245, 113, 61)));
        m_resultsPanel->socEnvelopeChartView->showSingleSeries(
            "SOC Spread vs Time",
            "Spread",
            makeSeries(pointSeriesForMetric(*m_activeResult, "soc_spread"), "SOC Spread", QColor(34, 197, 94)));
    }

    for (ChartWidget* chart : {
             m_resultsPanel->voltageChartView,
             m_resultsPanel->powerChartView,
             m_resultsPanel->temperatureChartView,
             m_resultsPanel->socEnvelopeChartView,
         }) {
        if (chart != nullptr) {
            chart->setMarkerTime(markerTime);
        }
    }
}

void MainWindow::refreshGroupTable()
{
    if (m_resultsPanel == nullptr || m_resultsPanel->groupTable == nullptr) {
        return;
    }

    m_isSyncingGroupPanel = true;
    m_resultsPanel->groupTable->clearContents();

    if (!m_activeResult.has_value() || !m_activeResult->valid) {
        m_resultsPanel->groupTable->setRowCount(0);
        m_isSyncingGroupPanel = false;
        return;
    }

    const desktop::SimulationTracePoint* point = m_activeResult->pointAt(m_activeResultPointIndex);
    if (point == nullptr) {
        m_resultsPanel->groupTable->setRowCount(0);
        m_isSyncingGroupPanel = false;
        return;
    }

    const int groupCount = m_activeResult->groupCount();
    m_resultsPanel->groupTable->setRowCount(groupCount);
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

        m_resultsPanel->groupTable->setItem(row, 0, groupItem);
        m_resultsPanel->groupTable->setItem(row, 1, socItem);
        m_resultsPanel->groupTable->setItem(row, 2, voltageItem);
        m_resultsPanel->groupTable->setItem(row, 3, coreTempItem);
        m_resultsPanel->groupTable->setItem(row, 4, surfaceTempItem);
    }

    const int clampedGroup = std::clamp(m_selectedResultGroupIndex, 0, std::max(0, groupCount - 1));
    m_selectedResultGroupIndex = clampedGroup;
    if (groupCount > 0) {
        m_resultsPanel->groupTable->selectRow(clampedGroup);
        m_resultsPanel->resultSelectionLabel->setText(QString("Selected group: %1").arg(m_activeResult->groupLabel(clampedGroup)));
    } else {
        m_resultsPanel->resultSelectionLabel->setText("Selected group: --");
    }
    m_isSyncingGroupPanel = false;
}

void MainWindow::refreshGroupDetailPanel()
{
    if (m_resultsPanel == nullptr || m_resultsPanel->groupDetailLabel == nullptr) {
        return;
    }
    if (!m_activeResult.has_value() || !m_activeResult->valid || m_selectedResultGroupIndex < 0) {
        m_resultsPanel->groupDetailLabel->setText(
            "Select a group from the table or click the workspace preview to review the hottest or weakest area in the current candidate.");
        return;
    }

    const desktop::SimulationTracePoint* point = m_activeResult->pointAt(m_activeResultPointIndex);
    if (point == nullptr) {
        m_resultsPanel->groupDetailLabel->setText(
            "Select a group from the table or click the workspace preview to review the hottest or weakest area in the current candidate.");
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

    m_resultsPanel->groupDetailLabel->setText(QString(
        "%1\n"
        "Entity: %2 | Zone: %3 | Flags: %4\n"
        "SOC: %5 | Voltage: %6 V\n"
        "Core: %7 C | Surface: %8 C")
            .arg(m_activeResult->groupLabel(index))
            .arg(m_activeResult->groupEntityId(index).isEmpty() ? "--" : m_activeResult->groupEntityId(index))
            .arg(index < static_cast<int>(point->group_zone_ids.size()) ? QString::number(point->group_zone_ids[static_cast<std::size_t>(index)]) : "--")
            .arg(flags.isEmpty() ? "none" : flags.join(", "))
            .arg(valueAt(point->group_soc))
            .arg(valueAt(point->group_voltage_v))
            .arg(valueAt(point->group_core_temp_c))
            .arg(valueAt(point->group_surface_temp_c)));
}

void MainWindow::refreshCadOverlay()
{
    if (m_workspacePanel == nullptr || m_workspacePanel->workspaceView == nullptr) {
        return;
    }

    if (!m_activeResult.has_value() || !m_activeResult->valid) {
        m_workspacePanel->workspaceView->clearSimulationOverlay();
        if (m_resultsPanel != nullptr && m_resultsPanel->resultOverlayLegendLabel != nullptr) {
            m_resultsPanel->resultOverlayLegendLabel->setText("Overlay range: --");
        }
        return;
    }

    cad::battery::BatteryVisualizationOverlay::Metric metric = cad::battery::BatteryVisualizationOverlay::Metric::CoreTemperature;
    if (m_resultsPanel != nullptr && m_resultsPanel->overlayMetricCombo != nullptr) {
        switch (m_resultsPanel->overlayMetricCombo->currentIndex()) {
        case 1:
            metric = cad::battery::BatteryVisualizationOverlay::Metric::Soc;
            break;
        case 2:
            metric = cad::battery::BatteryVisualizationOverlay::Metric::Voltage;
            break;
        case 3:
            metric = cad::battery::BatteryVisualizationOverlay::Metric::SurfaceTemperature;
            break;
        case 0:
        default:
            metric = cad::battery::BatteryVisualizationOverlay::Metric::CoreTemperature;
            break;
        }
    }

    if (const desktop::SimulationTracePoint* point = m_activeResult->pointAt(m_activeResultPointIndex);
        point != nullptr && m_resultsPanel != nullptr && m_resultsPanel->resultOverlayLegendLabel != nullptr) {
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
        case cad::battery::BatteryVisualizationOverlay::Metric::CoreTemperature:
        default:
            values = point->group_core_temp_c;
            break;
        }
        if (!values.empty()) {
            const auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
            m_resultsPanel->resultOverlayLegendLabel->setText(
                QString("Overlay range: %1 to %2").arg(formatMaybeNumber(*minIt, 3), formatMaybeNumber(*maxIt, 3)));
        } else {
            m_resultsPanel->resultOverlayLegendLabel->setText("Overlay range: --");
        }
    }

    m_workspacePanel->workspaceView->setSimulationOverlay(
        buildSimulationOverlay(m_workspacePanel->workspaceView->document(), *m_activeResult, m_activeResultPointIndex, metric));
}

void MainWindow::refreshResultScrubber()
{
    if (m_resultsPanel == nullptr || m_resultsPanel->resultTimeSlider == nullptr || m_resultsPanel->resultTimeLabel == nullptr) {
        return;
    }

    QSignalBlocker blocker(m_resultsPanel->resultTimeSlider);
    if (!m_activeResult.has_value() || !m_activeResult->valid || !m_activeResult->hasPoints()) {
        m_resultsPanel->resultTimeSlider->setEnabled(false);
        m_resultsPanel->resultTimeSlider->setRange(0, 0);
        m_resultsPanel->resultTimeSlider->setValue(0);
        m_resultsPanel->resultTimeLabel->setText("Time: --");
        return;
    }

    m_activeResultPointIndex = m_activeResult->clampedPointIndex(
        m_activeResultPointIndex < 0 ? m_activeResult->pointCount() - 1 : m_activeResultPointIndex);
    m_resultsPanel->resultTimeSlider->setEnabled(true);
    m_resultsPanel->resultTimeSlider->setRange(0, m_activeResult->pointCount() - 1);
    m_resultsPanel->resultTimeSlider->setValue(m_activeResultPointIndex);

    if (const auto* point = m_activeResult->pointAt(m_activeResultPointIndex); point != nullptr) {
        m_resultsPanel->resultTimeLabel->setText(QString("Time: %1 s").arg(point->time_s, 0, 'f', 0));
    }
}

std::vector<charts::Point> MainWindow::pointSeriesForMetric(const desktop::SimulationResultModel& result, const QString& metricKey) const
{
    std::vector<charts::Point> points;
    points.reserve(result.points.size());
    for (const desktop::SimulationTracePoint& point : result.points) {
        double value = 0.0;
        if (metricKey == "pack_voltage_v") {
            value = point.pack_voltage_v;
        } else if (metricKey == "pack_power_w") {
            value = point.pack_power_w;
        } else if (metricKey == "soc_spread") {
            value = point.soc_max - point.soc_min;
        } else if (metricKey == "pack_temp_max_c") {
            value = point.pack_temp_max_c;
        } else if (metricKey == "group_core_temp_max_c" && !point.group_core_temp_c.empty()) {
            value = *std::max_element(point.group_core_temp_c.begin(), point.group_core_temp_c.end());
        }
        points.push_back(makePoint(point.time_s, value));
    }
    return points;
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
    m_activeComparisonSummary.reset();
    m_activeResultPayload = {};
    m_activeResultPointIndex = -1;
    m_selectedResultGroupIndex = -1;
    if (m_resultsPanel != nullptr) {
        m_resultsPanel->summaryLabel->setText(
            "Run a candidate study to populate the primary trade-study readout, then compare it against a saved baseline.");
        clearChart(m_resultsPanel->voltageChartView, "Pack Voltage vs Time", "Voltage (V)");
        clearChart(m_resultsPanel->powerChartView, "Pack Power vs Time", "Power (W)");
        clearChart(m_resultsPanel->temperatureChartView, "Thermal Peak vs Time", "Temperature (C)");
        clearChart(m_resultsPanel->socEnvelopeChartView, "SOC Spread vs Time", "Spread");
        m_resultsPanel->groupTable->setRowCount(0);
        m_resultsPanel->resultTimeSlider->setEnabled(false);
        m_resultsPanel->resultTimeSlider->setRange(0, 0);
        m_resultsPanel->resultTimeSlider->setValue(0);
        m_resultsPanel->resultTimeLabel->setText("Time: --");
        m_resultsPanel->resultSelectionLabel->setText("Selected group: --");
        m_resultsPanel->resultOverlayLegendLabel->setText("Overlay range: --");
        m_resultsPanel->groupDetailLabel->setText(
            "Select a group from the table or click the workspace preview to review the hottest or weakest area in the current candidate.");
    }
    if (m_testsPanel != nullptr && m_testsPanel->virtualTestComparisonTable != nullptr) {
        m_testsPanel->virtualTestComparisonTable->setRowCount(0);
    }
    if (m_workspacePanel != nullptr) {
        if (m_workspacePanel->workspaceView != nullptr) {
            m_workspacePanel->workspaceView->clearSimulationOverlay();
        }
        if (m_workspacePanel->reportNotes != nullptr) {
            m_workspacePanel->reportNotes->clear();
        }
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
    refreshCadOverlay();
    refreshCharts();
}

void MainWindow::handleOverlayMetricChanged(int index)
{
    if (m_resultsPanel != nullptr && m_resultsPanel->overlayMetricCombo != nullptr) {
        m_resultsPanel->overlayMetricCombo->setStyleSheet(QString("border:1px solid %1;").arg(overlayMetricAccent(index).name()));
    }
    refreshCadOverlay();
}

void MainWindow::handleGroupSelectionChanged()
{
    if (m_isSyncingGroupPanel || m_resultsPanel == nullptr || m_resultsPanel->groupTable == nullptr) {
        return;
    }

    const QList<QTableWidgetItem*> items = m_resultsPanel->groupTable->selectedItems();
    if (items.empty()) {
        return;
    }

    m_selectedResultGroupIndex = items.front()->row();
    if (m_activeResult.has_value()) {
        m_resultsPanel->resultSelectionLabel->setText(QString("Selected group: %1").arg(m_activeResult->groupLabel(m_selectedResultGroupIndex)));
    }
    refreshGroupDetailPanel();
}

void MainWindow::loadSystemPresetCatalog()
{
    if (m_setupPanel == nullptr || m_setupPanel->systemPresetCombo == nullptr) {
        return;
    }

    const SimulationClient::Result result = m_client.listSystemPresets();
    if (!result.ok) {
        m_setupPanel->systemPresetDescription->setText(QString("Failed to load trade-study archetypes.\n%1").arg(result.error));
        return;
    }

    m_systemPresetCatalog = result.payload.value("presets").toArray();
    {
        const QSignalBlocker presetBlocker(m_setupPanel->systemPresetCombo);
        m_setupPanel->systemPresetCombo->clear();
        for (const QJsonValue& value : m_systemPresetCatalog) {
            const QJsonObject preset = value.toObject();
            m_setupPanel->systemPresetCombo->addItem(preset.value("display_name").toString(), preset.value("preset_id").toString());
        }

        const QString defaultPresetId = result.payload.value("default_preset_id").toString();
        for (int index = 0; index < m_setupPanel->systemPresetCombo->count(); ++index) {
            if (m_setupPanel->systemPresetCombo->itemData(index).toString() == defaultPresetId) {
                m_setupPanel->systemPresetCombo->setCurrentIndex(index);
                break;
            }
        }
    }
    refreshSelectedSystemPresetDescription();
}

void MainWindow::refreshSelectedSystemPresetDescription()
{
    if (m_setupPanel == nullptr || m_setupPanel->systemPresetDescription == nullptr || m_setupPanel->systemPresetCombo == nullptr) {
        return;
    }

    const QString presetId = m_setupPanel->systemPresetCombo->currentData().toString();
    appendDesktopStartupLog(QString("system preset selection update | preset_id=%1").arg(presetId));
    for (const QJsonValue& value : m_systemPresetCatalog) {
        const QJsonObject preset = value.toObject();
        if (preset.value("preset_id").toString() != presetId) {
            continue;
        }
        QStringList recommended;
        for (const QJsonValue& item : preset.value("recommended_virtual_tests").toArray()) {
            QString label = item.toString();
            label.replace('_', ' ');
            if (!label.isEmpty()) {
                label[0] = label[0].toUpper();
            }
            recommended << label;
        }
        m_setupPanel->systemPresetDescription->setText(QString("%1\nChemistry: %2 | Cell: %3\nFlagship tests: %4")
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
    applySimulationConfig(preset.value("simulation_defaults").toObject());
    if (m_setupPanel->referencePreset != nullptr) {
        m_setupPanel->referencePreset->setCurrentIndex(0);
    }
    m_reportContext = packStudyContext("Candidate Pack Study");
    updateCadWorkspace();
    clearSimulationVisualization();

    const QJsonArray recommendedTests = preset.value("recommended_virtual_tests").toArray();
    if (m_testsPanel != nullptr && m_testsPanel->virtualTestCombo != nullptr && !recommendedTests.isEmpty()) {
        const QString firstTestId = recommendedTests.first().toString();
        for (int index = 0; index < m_testsPanel->virtualTestCombo->count(); ++index) {
            if (m_testsPanel->virtualTestCombo->itemData(index).toString() == firstTestId) {
                m_testsPanel->virtualTestCombo->setCurrentIndex(index);
                break;
            }
        }
    }

    if (m_resultsPanel != nullptr && m_resultsPanel->summaryLabel != nullptr) {
        m_resultsPanel->summaryLabel->setText(QString("Loaded trade-study archetype: %1").arg(preset.value("display_name").toString()));
    }
    appendDesktopStartupLog("apply system preset complete");
}

void MainWindow::applySelectedSystemPreset()
{
    if (m_setupPanel == nullptr || m_setupPanel->systemPresetCombo == nullptr) {
        return;
    }

    const QString presetId = m_setupPanel->systemPresetCombo->currentData().toString();
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
    if (m_testsPanel == nullptr || m_testsPanel->virtualTestCombo == nullptr) {
        return;
    }

    const SimulationClient::Result result = m_client.listVirtualTests();
    if (!result.ok) {
        setVirtualTestStatus(QString("Failed to load flagship test catalog.\n%1").arg(result.error), QColor(220, 78, 78));
        return;
    }

    m_virtualTestCatalog = result.payload.value("tests").toArray();
    m_testsPanel->virtualTestCombo->clear();
    for (const QJsonValue& value : m_virtualTestCatalog) {
        const QJsonObject test = value.toObject();
        m_testsPanel->virtualTestCombo->addItem(test.value("display_name").toString(), test.value("test_id").toString());
    }
    rebuildVirtualTestForm();
}

void MainWindow::rebuildVirtualTestForm()
{
    if (m_testsPanel == nullptr || m_testsPanel->virtualTestFormLayout == nullptr || m_testsPanel->virtualTestCombo == nullptr) {
        return;
    }

    while (m_testsPanel->virtualTestFormLayout->rowCount() > 0) {
        m_testsPanel->virtualTestFormLayout->removeRow(0);
    }
    m_virtualTestFields.clear();

    const int index = m_testsPanel->virtualTestCombo->currentIndex();
    if (index < 0 || index >= m_virtualTestCatalog.size()) {
        m_testsPanel->virtualTestDescription->setText("No flagship trade-study tests available.");
        return;
    }

    const QJsonObject test = m_virtualTestCatalog.at(index).toObject();
    QString description = test.value("description").toString();
    description += QString("\nCategory: %1 | Difficulty: %2")
                       .arg(test.value("category").toString(), test.value("difficulty").toString());
    m_testsPanel->virtualTestDescription->setText(description);

    const QJsonArray parameters = test.value("parameters").toArray();
    for (const QJsonValue& value : parameters) {
        const QJsonObject parameter = value.toObject();
        const QString type = parameter.value("param_type").toString();
        const QString label = parameter.value("label").toString();
        const QString unit = parameter.value("unit").toString();
        const QString fullLabel = unit.isEmpty() ? label : QString("%1 (%2)").arg(label, unit);
        QWidget* editor = nullptr;

        if (type == "bool") {
            auto* check = new QCheckBox(m_testsPanel);
            check->setChecked(parameter.value("default_value").toBool());
            editor = check;
        } else if (type == "enum") {
            auto* combo = new QComboBox(m_testsPanel);
            for (const QJsonValue& item : parameter.value("allowed_values").toArray()) {
                combo->addItem(item.toString());
            }
            combo->setCurrentText(parameter.value("default_value").toString());
            editor = combo;
        } else {
            auto* line = new QLineEdit(m_testsPanel);
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
            m_testsPanel->virtualTestFormLayout->addRow(fullLabel, editor);
            m_virtualTestFields.push_back({
                parameter.value("key").toString(),
                type,
                parameter.value("required").toBool(true),
                editor,
            });
        }
    }

    setVirtualTestStatus(
        "Choose parameters, vet the selected flagship workflow against the active pack, then run it into the shared results and report preview.",
        QColor(90, 144, 203));
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
        {"test_id", m_testsPanel != nullptr && m_testsPanel->virtualTestCombo != nullptr ? m_testsPanel->virtualTestCombo->currentData().toString() : QString()},
        {"base_config", buildSimulationConfig()},
        {"parameters", parameters},
    };
}

void MainWindow::setVirtualTestStatus(const QString& text, const QColor& accent)
{
    if (m_testsPanel == nullptr || m_testsPanel->virtualTestStatus == nullptr) {
        return;
    }
    m_testsPanel->virtualTestStatus->setPlainText(text);
    m_testsPanel->virtualTestStatus->setStyleSheet(QString("border:1px solid %1;").arg(accent.name()));
}

void MainWindow::renderVirtualTestResult(const QJsonObject& payload)
{
    m_reportContext = trade_study::buildVirtualTestReportContext(currentArchetypeId(), currentArchetypeName(), payload);

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

    if (m_testsPanel != nullptr && m_testsPanel->virtualTestComparisonTable != nullptr) {
        m_testsPanel->virtualTestComparisonTable->setRowCount(0);
        int row = 0;
        const QJsonArray subResults = payload.value("sub_results").toArray();
        for (const QJsonValue& value : subResults) {
            const QJsonObject sub = value.toObject();
            const QString scenarioName = sub.value("name").toString();
            const QJsonObject scenarioSummary = sub.value("summary_metrics").toObject();
            for (auto it = scenarioSummary.begin(); it != scenarioSummary.end(); ++it) {
                m_testsPanel->virtualTestComparisonTable->insertRow(row);
                m_testsPanel->virtualTestComparisonTable->setItem(row, 0, new QTableWidgetItem(scenarioName));
                m_testsPanel->virtualTestComparisonTable->setItem(row, 1, new QTableWidgetItem(it.key()));
                m_testsPanel->virtualTestComparisonTable->setItem(row, 2, new QTableWidgetItem(it.value().toVariant().toString()));
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

void MainWindow::handleBackendRequestFinished(bool ok, const QString& error, const QJsonObject& payload)
{
    const PendingBackendAction action = m_pendingBackendAction;
    m_pendingBackendAction = PendingBackendAction::None;
    setBackendBusy(false);

    if (!ok) {
        m_runVirtualTestAfterVetting = false;
        m_pendingVirtualTestPayload = {};
        switch (action) {
        case PendingBackendAction::VetVirtualTest:
        case PendingBackendAction::RunVirtualTest:
            setVirtualTestStatus(
                QString("%1 failed.\n%2").arg(action == PendingBackendAction::RunVirtualTest ? "Virtual test" : "Test vetting", error),
                QColor(220, 78, 78));
            break;
        case PendingBackendAction::CaptureBaseline:
        case PendingBackendAction::CompareAgainstBaseline:
        case PendingBackendAction::RunSimulation:
        case PendingBackendAction::None:
        default:
            QMessageBox::critical(this, "Simulation Error", error);
            break;
        }
        return;
    }

    switch (action) {
    case PendingBackendAction::RunSimulation:
        renderResult(payload);
        statusBar()->showMessage("Candidate study complete.", 4000);
        break;
    case PendingBackendAction::CaptureBaseline: {
        m_baselineResult = payload;
        m_resultsPanel->summaryLabel->setText("Baseline captured. Update the candidate layout or flagship test inputs, then compare against baseline.");
        if (const auto baseline = parseOptionalResult(m_baselineResult); baseline.has_value()) {
            const auto summary = trade_study::buildComparisonSummary(
                *baseline,
                std::nullopt,
                trade_study::buildPackSimulationReportContext(currentArchetypeId(), currentArchetypeName(), "Baseline Reference"));
            m_workspacePanel->reportNotes->setPlainText(trade_study::formatComparisonSummaryText(summary));
        }
        statusBar()->showMessage("Baseline captured.", 4000);
        break;
    }
    case PendingBackendAction::CompareAgainstBaseline:
        renderComparison(m_baselineResult, payload);
        statusBar()->showMessage("Baseline comparison complete.", 4000);
        break;
    case PendingBackendAction::VetVirtualTest: {
        const QJsonObject vetting = payload.value("vetting_result").toObject();
        QStringList lines;
        const bool isValid = vetting.value("is_valid").toBool();
        lines << QString("Vetting: %1").arg(isValid ? "ready to run" : "blocked");
        const QJsonArray errors = vetting.value("errors").toArray();
        if (!errors.isEmpty()) {
            lines << "Errors:";
            for (const QJsonValue& entry : errors) {
                lines << QString("- %1").arg(entry.toString());
            }
        }
        const QJsonArray warnings = vetting.value("warnings").toArray();
        if (!warnings.isEmpty()) {
            lines << "Warnings:";
            for (const QJsonValue& entry : warnings) {
                lines << QString("- %1").arg(entry.toString());
            }
        }
        setVirtualTestStatus(
            lines.join('\n'),
            isValid ? (warnings.isEmpty() ? QColor(58, 165, 99) : QColor(214, 179, 67)) : QColor(220, 78, 78));

        if (m_runVirtualTestAfterVetting && isValid && !m_pendingVirtualTestPayload.isEmpty()) {
            m_runVirtualTestAfterVetting = false;
            m_pendingBackendAction = PendingBackendAction::RunVirtualTest;
            if (!m_client.runVirtualTestAsync(m_pendingVirtualTestPayload)) {
                m_pendingBackendAction = PendingBackendAction::None;
                m_pendingVirtualTestPayload = {};
                setVirtualTestStatus("Virtual test could not be started because another backend request is active.", QColor(220, 78, 78));
                return;
            }
            setBackendBusy(true, "Running virtual test...");
            return;
        }

        m_runVirtualTestAfterVetting = false;
        m_pendingVirtualTestPayload = {};
        statusBar()->showMessage("Virtual test vetting complete.", 4000);
        break;
    }
    case PendingBackendAction::RunVirtualTest:
        m_runVirtualTestAfterVetting = false;
        m_pendingVirtualTestPayload = {};
        renderVirtualTestResult(payload);
        statusBar()->showMessage("Virtual test complete.", 4000);
        break;
    case PendingBackendAction::None:
    default:
        break;
    }
}

void MainWindow::vetSelectedVirtualTest()
{
    if (m_client.isBusy()) {
        setVirtualTestStatus("A simulator request is already running.", QColor(214, 179, 67));
        return;
    }
    flushPendingCadWorkspaceUpdate();
    m_runVirtualTestAfterVetting = false;
    m_pendingVirtualTestPayload = buildVirtualTestPayload();
    m_pendingBackendAction = PendingBackendAction::VetVirtualTest;
    if (!m_client.vetVirtualTestAsync(m_pendingVirtualTestPayload)) {
        m_pendingBackendAction = PendingBackendAction::None;
        m_pendingVirtualTestPayload = {};
        setVirtualTestStatus("Test vetting could not be started.", QColor(220, 78, 78));
        return;
    }
    setBackendBusy(true, "Vetting virtual test...");
}

void MainWindow::runSelectedVirtualTest()
{
    if (m_client.isBusy()) {
        setVirtualTestStatus("A simulator request is already running.", QColor(214, 179, 67));
        return;
    }
    flushPendingCadWorkspaceUpdate();
    m_pendingVirtualTestPayload = buildVirtualTestPayload();
    m_runVirtualTestAfterVetting = true;
    m_pendingBackendAction = PendingBackendAction::VetVirtualTest;
    if (!m_client.vetVirtualTestAsync(m_pendingVirtualTestPayload)) {
        m_pendingBackendAction = PendingBackendAction::None;
        m_pendingVirtualTestPayload = {};
        m_runVirtualTestAfterVetting = false;
        setVirtualTestStatus("Virtual test could not be started.", QColor(220, 78, 78));
        return;
    }
    setBackendBusy(true, "Vetting virtual test...");
}

void MainWindow::exportActiveResultJson()
{
    if (m_activeResultPayload.isEmpty()) {
        QMessageBox::information(this, "No Result", "Run a simulation or flagship test before exporting the canonical result payload.");
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this,
        "Export Canonical Result JSON",
        QString(),
        "JSON Files (*.json)");
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, "Export Failed", "Could not write the selected export file.");
        return;
    }

    file.write(QJsonDocument(m_activeResultPayload).toJson(QJsonDocument::Indented));
    m_resultsPanel->summaryLabel->setText(QString("Exported canonical result payload to %1").arg(path));
}

QString MainWindow::currentArchetypeId() const
{
    if (!m_activeSystemPreset.isEmpty()) {
        return m_activeSystemPreset.value("preset_id").toString();
    }
    if (m_setupPanel != nullptr && m_setupPanel->systemPresetCombo != nullptr) {
        return m_setupPanel->systemPresetCombo->currentData().toString();
    }
    return {};
}

QString MainWindow::currentArchetypeName() const
{
    if (!m_activeSystemPreset.isEmpty()) {
        return m_activeSystemPreset.value("display_name").toString();
    }
    if (m_setupPanel != nullptr && m_setupPanel->systemPresetCombo != nullptr) {
        return m_setupPanel->systemPresetCombo->currentText();
    }
    return {};
}

trade_study::ReportContext MainWindow::packStudyContext(const QString& workflowName) const
{
    return trade_study::buildPackSimulationReportContext(currentArchetypeId(), currentArchetypeName(), workflowName);
}
