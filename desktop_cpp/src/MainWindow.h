#pragma once

#include "CadViewportWidget.h"
#include "ChartWidget.h"
#include "SimulationClient.h"

#include <QColor>
#include <QMainWindow>
#include <QJsonObject>
#include <QVariantList>

class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QTabWidget;

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
    QString formatSummaryLines(const QJsonObject& payload) const;
    QString formatTraceLines(const QJsonObject& payload) const;
    ChartWidget* createGraphWidget();
    QFrame* createWorkspacePanel();
    void applyTheme();
    void applyChartTheme(ChartWidget* graphWidget);
    void updateCadWorkspace();
    charts::Series toChartSeries(const QJsonArray& timeSeries, const QString& metricKey, const QString& name, const QColor& color, bool dashed = false) const;
    void populateChart(ChartWidget* graphWidget, const QJsonArray& timeSeries, const QString& metricKey, const QString& title, const QString& yTitle);
    void populateComparisonChart(
        ChartWidget* graphWidget,
        const QJsonArray& baselineSeries,
        const QJsonArray& candidateSeries,
        const QString& metricKey,
        const QString& title,
        const QString& yTitle
    );

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
    ChartWidget* m_temperatureChartView = nullptr;
    ChartWidget* m_socChartView = nullptr;
    ChartWidget* m_powerChartView = nullptr;
    CadViewportWidget* m_cadWorkspaceView = nullptr;
    QPlainTextEdit* m_outputText = nullptr;
    QPushButton* m_runButton = nullptr;
    QPushButton* m_captureBaselineButton = nullptr;
    QPushButton* m_compareBaselineButton = nullptr;
    QPushButton* m_saveProjectButton = nullptr;
    QPushButton* m_loadProjectButton = nullptr;
    ThemeSettings m_theme;
    QJsonObject m_baselineConfig;
    QJsonObject m_baselineResult;
};
