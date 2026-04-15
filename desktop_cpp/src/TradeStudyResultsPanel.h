#pragma once

#include <QWidget>

class ChartWidget;
class QComboBox;
class QLabel;
class QSlider;
class QTableWidget;

class TradeStudyResultsPanel : public QWidget
{
public:
    explicit TradeStudyResultsPanel(QWidget* parent = nullptr);

    QLabel* summaryLabel = nullptr;
    QComboBox* overlayMetricCombo = nullptr;
    QSlider* resultTimeSlider = nullptr;
    QLabel* resultTimeLabel = nullptr;
    QLabel* resultSelectionLabel = nullptr;
    QLabel* resultOverlayLegendLabel = nullptr;
    QTableWidget* groupTable = nullptr;
    QLabel* groupDetailLabel = nullptr;
    ChartWidget* voltageChartView = nullptr;
    ChartWidget* powerChartView = nullptr;
    ChartWidget* temperatureChartView = nullptr;
    ChartWidget* socEnvelopeChartView = nullptr;

private:
    static ChartWidget* createGraphWidget(QWidget* parent);
};

