#include "TradeStudyTestsPanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

TradeStudyTestsPanel::TradeStudyTestsPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* header = new QLabel("Flagship Virtual Tests", this);
    header->setStyleSheet("font-size:16px; font-weight:700; color:#f3f7fb;");
    auto* subheader = new QLabel(
        "Keep the study on the four flagship workflows. Archived lab tests remain available in code, not in the primary cockpit.",
        this);
    subheader->setWordWrap(true);
    subheader->setStyleSheet("font-size:12px; color:#93a6ba;");
    layout->addWidget(header);
    layout->addWidget(subheader);

    auto* selectionGroup = new QGroupBox("Choose Workflow", this);
    auto* selectionLayout = new QVBoxLayout(selectionGroup);
    selectionLayout->setContentsMargins(14, 16, 14, 14);
    selectionLayout->setSpacing(8);
    virtualTestCombo = new QComboBox(selectionGroup);
    virtualTestDescription = new QLabel("Loading flagship test catalog...", selectionGroup);
    virtualTestDescription->setWordWrap(true);
    virtualTestDescription->setStyleSheet("color:#93a6ba;");
    selectionLayout->addWidget(virtualTestCombo);
    selectionLayout->addWidget(virtualTestDescription);
    layout->addWidget(selectionGroup);

    auto* parametersGroup = new QGroupBox("Test Inputs", this);
    virtualTestFormLayout = new QFormLayout(parametersGroup);
    virtualTestFormLayout->setContentsMargins(14, 16, 14, 14);
    virtualTestFormLayout->setHorizontalSpacing(10);
    virtualTestFormLayout->setVerticalSpacing(8);
    layout->addWidget(parametersGroup);

    auto* actionsGroup = new QGroupBox("Vet & Run", this);
    auto* actionsLayout = new QVBoxLayout(actionsGroup);
    actionsLayout->setContentsMargins(14, 16, 14, 14);
    actionsLayout->setSpacing(8);
    auto* actionButtons = new QHBoxLayout();
    vetTestButton = new QPushButton("Vet Test", actionsGroup);
    runTestButton = new QPushButton("Run Test", actionsGroup);
    actionButtons->addWidget(vetTestButton);
    actionButtons->addWidget(runTestButton);
    virtualTestStatus = new QPlainTextEdit(actionsGroup);
    virtualTestStatus->setReadOnly(true);
    virtualTestStatus->setMinimumHeight(140);
    actionsLayout->addLayout(actionButtons);
    actionsLayout->addWidget(virtualTestStatus);
    layout->addWidget(actionsGroup);

    auto* compareGroup = new QGroupBox("Scenario Breakdown", this);
    auto* compareLayout = new QVBoxLayout(compareGroup);
    compareLayout->setContentsMargins(14, 16, 14, 14);
    compareLayout->setSpacing(8);
    virtualTestComparisonTable = new QTableWidget(0, 3, compareGroup);
    virtualTestComparisonTable->setHorizontalHeaderLabels({"Scenario", "Metric", "Value"});
    virtualTestComparisonTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    virtualTestComparisonTable->setSelectionMode(QAbstractItemView::NoSelection);
    virtualTestComparisonTable->verticalHeader()->setVisible(false);
    virtualTestComparisonTable->horizontalHeader()->setStretchLastSection(true);
    virtualTestComparisonTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    virtualTestComparisonTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    compareLayout->addWidget(virtualTestComparisonTable);
    layout->addWidget(compareGroup, 1);
}

