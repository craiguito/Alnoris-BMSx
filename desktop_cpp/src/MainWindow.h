#pragma once

#include "SimulationClient.h"
#include "SimulationMappingBuilder.h"
#include "SimulationResultModel.h"
#include "TradeStudySummary.h"
#include "../cad/battery/BatteryConfig.h"
#include "../charts/ChartTypes.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>

#include <optional>
#include <vector>

class ChartWidget;
class QCheckBox;
class QComboBox;
class QFormLayout;
class QLineEdit;
class QTimer;
class TradeStudyResultsPanel;
class TradeStudySetupPanel;
class TradeStudyTestsPanel;
class TradeStudyWorkspacePanel;
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
    void generateLayout();
    void exportTradeStudyReport();
    void handleResultScrubChanged(int value);
    void handleOverlayMetricChanged(int index);
    void handleGroupSelectionChanged();
    void handleVirtualTestSelectionChanged(int index);
    void handleTruthDatasetSelectionChanged();
    void handleBackendRequestFinished(bool ok, const QString& error, const QJsonObject& payload);
    void vetSelectedVirtualTest();
    void runSelectedVirtualTest();
    void applySelectedTruthDatasetsToValidation();
    void exportActiveResultJson();
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

    enum class PendingBackendAction
    {
        None,
        RunSimulation,
        CaptureBaseline,
        CompareAgainstBaseline,
        VetVirtualTest,
        RunVirtualTest
    };

    void createMainToolbar();
    void applyTheme();
    void applyChartTheme(ChartWidget* graphWidget);
    QJsonObject buildSimulationConfig() const;
    SimulationMappingBuilder::MappingResult buildSimulationMapping() const;
    cad::battery::BatteryCadConfig buildCadWorkspaceConfig() const;
    void renderResult(const QJsonObject& payload);
    void renderComparison(const QJsonObject& baselinePayload, const QJsonObject& candidatePayload);
    void applySimulationConfig(const QJsonObject& config);
    void scheduleCadWorkspaceUpdate();
    void flushPendingCadWorkspaceUpdate();
    void updateCadWorkspace();
    void refreshWorkspaceSummary();
    void setBackendBusy(bool busy, const QString& statusText = QString());
    charts::Series makeSeries(const std::vector<charts::Point>& points, const QString& name, const QColor& color, bool dashed = false) const;
    void refreshSimulationViews();
    void refreshCharts();
    void refreshGroupTable();
    void refreshGroupDetailPanel();
    void refreshCadOverlay();
    void refreshResultScrubber();
    std::vector<charts::Point> pointSeriesForMetric(const desktop::SimulationResultModel& result, const QString& metricKey) const;
    void clearSimulationVisualization();
    void loadSystemPresetCatalog();
    void refreshSelectedSystemPresetDescription();
    void applySystemPreset(const QJsonObject& preset);
    void loadVirtualTestCatalog();
    void loadTruthDatasetCatalog();
    void rebuildVirtualTestForm();
    QJsonObject buildVirtualTestPayload() const;
    void setVirtualTestStatus(const QString& text, const QColor& accent);
    void renderVirtualTestResult(const QJsonObject& payload);
    void refreshTruthDatasetTable();
    void refreshTruthDatasetDetailPanel();
    void syncTruthDatasetSelectionFromSettings();
    void applyValidationSettingsToForm();
    void refreshValidationScorecardPanel();
    QString currentArchetypeId() const;
    QString currentArchetypeName() const;
    trade_study::ReportContext packStudyContext(const QString& workflowName) const;

    SimulationClient m_client;
    TradeStudySetupPanel* m_setupPanel = nullptr;
    TradeStudyWorkspacePanel* m_workspacePanel = nullptr;
    TradeStudyTestsPanel* m_testsPanel = nullptr;
    TradeStudyResultsPanel* m_resultsPanel = nullptr;
    ThemeSettings m_theme;
    QJsonArray m_virtualTestCatalog;
    QJsonArray m_systemPresetCatalog;
    QJsonArray m_truthDatasetCatalog;
    std::vector<VirtualTestField> m_virtualTestFields;
    QJsonObject m_activeSystemPreset;
    QJsonObject m_baselineConfig;
    QJsonObject m_baselineResult;
    QJsonObject m_activeResultPayload;
    QJsonObject m_pendingVirtualTestPayload;
    QJsonObject m_validationSettings;
    QJsonObject m_lastValidationResult;
    trade_study::ReportContext m_reportContext;
    std::optional<trade_study::ComparisonSummary> m_activeComparisonSummary;
    std::optional<desktop::SimulationResultModel> m_activeResult;
    int m_activeResultPointIndex = -1;
    int m_selectedResultGroupIndex = -1;
    PendingBackendAction m_pendingBackendAction = PendingBackendAction::None;
    QTimer* m_cadRefreshTimer = nullptr;
    bool m_backendBusy = false;
    bool m_isSyncingGroupPanel = false;
    bool m_isSyncingTruthDatasetTable = false;
    bool m_runVirtualTestAfterVetting = false;
};
