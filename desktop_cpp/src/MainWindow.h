#pragma once

#include "CadViewportWidget.h"
#include "ChartWidget.h"
#include "SimulationClient.h"
#include "SimulationResultModel.h"

#include <QColor>
#include <QJsonArray>
#include <QMainWindow>
#include <QJsonObject>
#include <QVariantList>

#include <optional>
#include <vector>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QFrame;
class QFormLayout;
class QGroupBox;
class QPlainTextEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QTableWidget;
class QTabWidget;
class QToolBar;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QString projectRoot, QWidget* parent = nullptr);

private slots:
    void runSimulation();
    void captureBaseline();
    void compareAgainstBaseline();
    void saveProject();
    void loadProject();
    void applyReferencePreset(int index);
    void openCustomizationDialog();
    void refreshCadProperties();
    void applyCadPropertyChanges();
    void undoCadEdit();
    void redoCadEdit();
    void resetCadPosition();
    void resetCadGeometry();
    void resetCadLabel();
    void handleResultScrubChanged(int value);
    void handleOverlayMetricChanged(int index);
    void handleGroupSelectionChanged();
    void handleVirtualTestSelectionChanged(int index);
    void vetSelectedVirtualTest();
    void runSelectedVirtualTest();
    void exportActiveResultJson();
    void handleSystemPresetCategoryChanged(int index);
    void applySelectedSystemPreset();

private:
    struct ThemeSettings
    {
        QColor appBackground = QColor(34, 34, 34);
        QColor textColor = QColor(243, 247, 251);
        QColor cadBackground = QColor(198, 205, 214);
        QColor chartBackground = QColor(198, 205, 214);
    };

    struct VirtualTestField
    {
        QString key;
        QString type;
        bool required = true;
        QWidget* editor = nullptr;
    };

    QDoubleSpinBox* createDoubleSpin(double value, double min, double max, int decimals);
    QJsonObject buildSimulationConfig() const;
    cad::battery::BatteryCadConfig buildCadWorkspaceConfig() const;
    void renderResult(const QJsonObject& payload);
    void renderComparison(const QJsonObject& baselinePayload, const QJsonObject& candidatePayload);
    void applySimulationConfig(const QJsonObject& config);
    QString formatSummaryLines(const desktop::SimulationResultModel& result) const;
    QString formatTraceLines(const desktop::SimulationResultModel& result) const;
    void createMainToolbar();
    ChartWidget* createGraphWidget();
    QWidget* createInputsSidebar();
    QFrame* createWorkspacePanel();
    QGroupBox* createCadPropertiesPanel();
    QGroupBox* createResultsPanel();
    QGroupBox* createVirtualTestsPanel();
    QGroupBox* createOutputPanel();
    void applyTheme();
    void applyChartTheme(ChartWidget* graphWidget);
    void updateCadWorkspace();
    void setCadEditorEnabled(bool enabled);
    charts::Series makeSeries(const std::vector<charts::Point>& points, const QString& name, const QColor& color, bool dashed = false) const;
    void refreshSimulationViews();
    void refreshCharts();
    void refreshGroupTable();
    void refreshGroupDetailPanel();
    void refreshCadOverlay();
    void refreshResultScrubber();
    void refreshSelectedGroupCharts();
    std::vector<charts::Point> pointSeriesForMetric(const desktop::SimulationResultModel& result, const QString& metricKey) const;
    std::vector<charts::Point> pointSeriesForGroupMetric(const desktop::SimulationResultModel& result, int groupIndex, const QString& metricKey) const;
    void clearSimulationVisualization();
    void loadSystemPresetCatalog();
    void rebuildSystemPresetOptions();
    void refreshSelectedSystemPresetDescription();
    void applySystemPreset(const QJsonObject& preset);
    void loadVirtualTestCatalog();
    void rebuildVirtualTestForm();
    QJsonObject buildVirtualTestPayload() const;
    void setVirtualTestStatus(const QString& text, const QColor& accent);
    void renderVirtualTestResult(const QJsonObject& payload);
    QString formatWarningLines(const std::vector<desktop::SimulationWarningModel>& warnings) const;

    SimulationClient m_client;
    QComboBox* m_systemPresetCategoryCombo = nullptr;
    QComboBox* m_systemPresetCombo = nullptr;
    QLabel* m_systemPresetDescription = nullptr;
    QPushButton* m_applySystemPresetButton = nullptr;
    QComboBox* m_referencePreset = nullptr;
    QDoubleSpinBox* m_cellNominalVoltage = nullptr;
    QDoubleSpinBox* m_cellFullVoltage = nullptr;
    QDoubleSpinBox* m_cellEmptyVoltage = nullptr;
    QDoubleSpinBox* m_cellCutoffVoltage = nullptr;
    QDoubleSpinBox* m_cellCapacity = nullptr;
    QDoubleSpinBox* m_cellsInSeries = nullptr;
    QDoubleSpinBox* m_cellsInParallel = nullptr;
    QDoubleSpinBox* m_internalResistance = nullptr;
    QDoubleSpinBox* m_ambientTemp = nullptr;
    QDoubleSpinBox* m_dischargeCurrent = nullptr;
    QDoubleSpinBox* m_duration = nullptr;
    QDoubleSpinBox* m_timeStep = nullptr;
    QDoubleSpinBox* m_initialSoc = nullptr;
    QDoubleSpinBox* m_packMass = nullptr;
    QDoubleSpinBox* m_packHeatCapacity = nullptr;
    QDoubleSpinBox* m_coolingCoeff = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QTabWidget* m_topChartTabs = nullptr;
    QTabWidget* m_bottomChartTabs = nullptr;
    ChartWidget* m_voltageChartView = nullptr;
    ChartWidget* m_currentChartView = nullptr;
    ChartWidget* m_temperatureChartView = nullptr;
    ChartWidget* m_coreTemperatureChartView = nullptr;
    ChartWidget* m_surfaceTemperatureChartView = nullptr;
    ChartWidget* m_socChartView = nullptr;
    ChartWidget* m_socEnvelopeChartView = nullptr;
    ChartWidget* m_powerChartView = nullptr;
    ChartWidget* m_groupVoltageChartView = nullptr;
    ChartWidget* m_groupCoreTemperatureChartView = nullptr;
    ChartWidget* m_groupSurfaceTemperatureChartView = nullptr;
    ChartWidget* m_groupDiffusionStressChartView = nullptr;
    ChartWidget* m_groupHysteresisChartView = nullptr;
    ChartWidget* m_groupSocChartView = nullptr;
    QComboBox* m_overlayMetricCombo = nullptr;
    QSlider* m_resultTimeSlider = nullptr;
    QLabel* m_resultTimeLabel = nullptr;
    QLabel* m_resultSelectionLabel = nullptr;
    QLabel* m_resultOverlayLegendLabel = nullptr;
    QTableWidget* m_groupTable = nullptr;
    QLabel* m_groupDetailLabel = nullptr;
    CadViewportWidget* m_cadWorkspaceView = nullptr;
    QLabel* m_cadSelectedType = nullptr;
    QLabel* m_cadSelectedId = nullptr;
    QLineEdit* m_cadLabelEdit = nullptr;
    QCheckBox* m_cadVisibleCheck = nullptr;
    QLabel* m_cadPosXLabel = nullptr;
    QLabel* m_cadPosYLabel = nullptr;
    QLabel* m_cadPosZLabel = nullptr;
    QDoubleSpinBox* m_cadPosX = nullptr;
    QDoubleSpinBox* m_cadPosY = nullptr;
    QDoubleSpinBox* m_cadPosZ = nullptr;
    QLabel* m_cadRadiusLabel = nullptr;
    QLabel* m_cadHeightLabel = nullptr;
    QDoubleSpinBox* m_cadRadius = nullptr;
    QDoubleSpinBox* m_cadHeight = nullptr;
    QLabel* m_cadSizeXLabel = nullptr;
    QLabel* m_cadSizeYLabel = nullptr;
    QLabel* m_cadSizeZLabel = nullptr;
    QDoubleSpinBox* m_cadSizeX = nullptr;
    QDoubleSpinBox* m_cadSizeY = nullptr;
    QDoubleSpinBox* m_cadSizeZ = nullptr;
    QLabel* m_cadThicknessLabel = nullptr;
    QDoubleSpinBox* m_cadThickness = nullptr;
    QPushButton* m_cadApplyButton = nullptr;
    QPushButton* m_cadUndoButton = nullptr;
    QPushButton* m_cadRedoButton = nullptr;
    QPushButton* m_cadResetPositionButton = nullptr;
    QPushButton* m_cadResetGeometryButton = nullptr;
    QPushButton* m_cadResetLabelButton = nullptr;
    QPlainTextEdit* m_outputText = nullptr;
    QComboBox* m_virtualTestCombo = nullptr;
    QLabel* m_virtualTestDescription = nullptr;
    QFormLayout* m_virtualTestFormLayout = nullptr;
    QPlainTextEdit* m_virtualTestStatus = nullptr;
    QTableWidget* m_virtualTestComparisonTable = nullptr;
    QPushButton* m_vetTestButton = nullptr;
    QPushButton* m_runTestButton = nullptr;
    QJsonArray m_virtualTestCatalog;
    QJsonArray m_systemPresetCatalog;
    std::vector<VirtualTestField> m_virtualTestFields;
    QPushButton* m_runButton = nullptr;
    QPushButton* m_captureBaselineButton = nullptr;
    QPushButton* m_compareBaselineButton = nullptr;
    QPushButton* m_saveProjectButton = nullptr;
    QPushButton* m_loadProjectButton = nullptr;
    ThemeSettings m_theme;
    QJsonObject m_activeSystemPreset;
    QJsonObject m_baselineConfig;
    QJsonObject m_baselineResult;
    QJsonObject m_lastExportPayload;
    std::optional<desktop::SimulationResultModel> m_activeResult;
    int m_activeResultPointIndex = -1;
    int m_selectedResultGroupIndex = -1;
    bool m_isSyncingCadInspector = false;
    bool m_isSyncingGroupPanel = false;
};
