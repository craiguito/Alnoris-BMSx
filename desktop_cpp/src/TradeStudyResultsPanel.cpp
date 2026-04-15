#include "TradeStudyResultsPanel.h"

#include "ChartWidget.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QSlider>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

ChartWidget* TradeStudyResultsPanel::createGraphWidget(QWidget* parent)
{
    auto* view = new ChartWidget(parent);
    view->setMinimumHeight(240);
    return view;
}

TradeStudyResultsPanel::TradeStudyResultsPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* header = new QLabel("Trade Study Results", this);
    header->setStyleSheet("font-size:16px; font-weight:700; color:#f3f7fb;");
    auto* subheader = new QLabel(
        "Stay on decision support: delivered energy, pack voltage, thermal peak, SOC spread, and the weakest or hottest group.",
        this);
    subheader->setWordWrap(true);
    subheader->setStyleSheet("font-size:12px; color:#93a6ba;");
    layout->addWidget(header);
    layout->addWidget(subheader);

    auto* summaryGroup = new QGroupBox("Decision Snapshot", this);
    auto* summaryLayout = new QVBoxLayout(summaryGroup);
    summaryLayout->setContentsMargins(14, 16, 14, 14);
    summaryLabel = new QLabel(
        "Run a candidate study to populate the primary trade-study readout, then compare it against a saved baseline.",
        summaryGroup);
    summaryLabel->setWordWrap(true);
    summaryLabel->setStyleSheet("font-size:13px; color:#d8e5f2;");
    summaryLayout->addWidget(summaryLabel);
    layout->addWidget(summaryGroup);

    auto* playbackGroup = new QGroupBox("Workspace Playback", this);
    auto* playbackLayout = new QGridLayout(playbackGroup);
    playbackLayout->setContentsMargins(14, 16, 14, 14);
    playbackLayout->setHorizontalSpacing(10);
    playbackLayout->setVerticalSpacing(8);
    overlayMetricCombo = new QComboBox(playbackGroup);
    overlayMetricCombo->addItems({
        "Core Temperature Overlay",
        "SOC Overlay",
        "Voltage Overlay",
        "Surface Temperature Overlay"
    });
    resultTimeSlider = new QSlider(Qt::Horizontal, playbackGroup);
    resultTimeSlider->setEnabled(false);
    resultTimeLabel = new QLabel("Time: --", playbackGroup);
    resultSelectionLabel = new QLabel("Selected group: --", playbackGroup);
    resultOverlayLegendLabel = new QLabel("Overlay range: --", playbackGroup);
    resultSelectionLabel->setStyleSheet("color:#93a6ba;");
    resultOverlayLegendLabel->setStyleSheet("color:#93a6ba;");
    playbackLayout->addWidget(new QLabel("Overlay metric", playbackGroup), 0, 0);
    playbackLayout->addWidget(overlayMetricCombo, 0, 1);
    playbackLayout->addWidget(new QLabel("Scrub timestep", playbackGroup), 1, 0);
    playbackLayout->addWidget(resultTimeSlider, 1, 1);
    playbackLayout->addWidget(resultTimeLabel, 2, 0);
    playbackLayout->addWidget(resultSelectionLabel, 2, 1);
    playbackLayout->addWidget(resultOverlayLegendLabel, 3, 0, 1, 2);
    layout->addWidget(playbackGroup);

    voltageChartView = createGraphWidget(this);
    powerChartView = createGraphWidget(this);
    temperatureChartView = createGraphWidget(this);
    socEnvelopeChartView = createGraphWidget(this);
    auto* chartTabs = new QTabWidget(this);
    chartTabs->addTab(voltageChartView, "Pack Voltage");
    chartTabs->addTab(powerChartView, "Power");
    chartTabs->addTab(temperatureChartView, "Thermal Peak");
    chartTabs->addTab(socEnvelopeChartView, "SOC Spread");
    layout->addWidget(chartTabs, 1);

    auto* groupInfoGroup = new QGroupBox("Weakest / Hottest Groups", this);
    auto* groupInfoLayout = new QVBoxLayout(groupInfoGroup);
    groupInfoLayout->setContentsMargins(14, 16, 14, 14);
    groupInfoLayout->setSpacing(10);
    groupTable = new QTableWidget(0, 5, groupInfoGroup);
    groupTable->setHorizontalHeaderLabels({"Group", "SOC", "Voltage (V)", "Core (C)", "Surface (C)"});
    groupTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    groupTable->setSelectionMode(QAbstractItemView::SingleSelection);
    groupTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    groupTable->verticalHeader()->setVisible(false);
    groupTable->horizontalHeader()->setStretchLastSection(true);
    groupTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    groupInfoLayout->addWidget(groupTable);
    groupDetailLabel = new QLabel(
        "Select a group from the table or click the workspace preview to review the hottest or weakest area in the current candidate.",
        groupInfoGroup);
    groupDetailLabel->setWordWrap(true);
    groupDetailLabel->setStyleSheet("color:#93a6ba;");
    groupInfoLayout->addWidget(groupDetailLabel);
    layout->addWidget(groupInfoGroup, 1);
}

