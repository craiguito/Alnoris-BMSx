#pragma once

#include "CadViewportWidget.h"
#include "ChartWidget.h"
#include "SimulationClient.h"
#include "SimulationResultModel.h"

#include <QColor>
#include <QMainWindow>
#include <QJsonObject>
#include <QVariantList>

#include <optional>
#include <vector>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QFrame;
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

private:
    struct ThemeSettings
    {
        QColor appBackground = QColor(34, 34, 34);
        QColor textColor = QColor(243, 247, 251);
        QColor cadBackground = QColor(198, 205, 214);
        QColor chartBackground = QColor(198, 205, 214);
    };

    QDoubleSpinBox* createDoubleSpin(double value, double min, double max, int decimals);
    QJsonObject buildSimulationConfig() const;
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
    QGroupBox* createOutputPanel();
    void applyTheme();
    void applyChartTheme(ChartWidget* graphWidget);
    void updateCadWorkspace();
    void setCadEditorEnabled(bool enabled);
    charts::Series makeSeries(const std::vector<charts::Point>& points, const QString& name, const QColor& color, bool dashed = false) const;
    void refreshSimulationViews();
    void refreshCharts();
    void refreshGroupTable();
    void refreshCadOverlay();
    void refreshResultScrubber();
    void refreshSelectedGroupCharts();
    std::vector<charts::Point> pointSeriesForMetric(const desktop::SimulationResultModel& result, const QString& metricKey) const;
    std::vector<charts::Point> pointSeriesForGroupMetric(const desktop::SimulationResultModel& result, int groupIndex, const QString& metricKey) const;
    void clearSimulationVisualization();

    SimulationClient m_client;
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
    ChartWidget* m_socChartView = nullptr;
    ChartWidget* m_socEnvelopeChartView = nullptr;
    ChartWidget* m_powerChartView = nullptr;
    ChartWidget* m_groupVoltageChartView = nullptr;
    ChartWidget* m_groupTemperatureChartView = nullptr;
    ChartWidget* m_groupSocChartView = nullptr;
    QComboBox* m_overlayMetricCombo = nullptr;
    QSlider* m_resultTimeSlider = nullptr;
    QLabel* m_resultTimeLabel = nullptr;
    QLabel* m_resultSelectionLabel = nullptr;
    QTableWidget* m_groupTable = nullptr;
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
    QPushButton* m_runButton = nullptr;
    QPushButton* m_captureBaselineButton = nullptr;
    QPushButton* m_compareBaselineButton = nullptr;
    QPushButton* m_saveProjectButton = nullptr;
    QPushButton* m_loadProjectButton = nullptr;
    ThemeSettings m_theme;
    QJsonObject m_baselineConfig;
    QJsonObject m_baselineResult;
    std::optional<desktop::SimulationResultModel> m_activeResult;
    int m_activeResultPointIndex = -1;
    int m_selectedResultGroupIndex = -1;
    bool m_isSyncingCadInspector = false;
    bool m_isSyncingGroupPanel = false;
};
