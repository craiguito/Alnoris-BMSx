#include "TradeStudyWorkspacePanel.h"

#include "CadViewportWidget.h"

#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

TradeStudyWorkspacePanel::TradeStudyWorkspacePanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto* workspaceGroup = new QGroupBox("", this);
    auto* workspaceLayout = new QVBoxLayout(workspaceGroup);
    workspaceLayout->setContentsMargins(12, 12, 12, 12);
    workspaceLayout->setSpacing(8);
    auto* workspaceHeader = new QLabel("Generated Layout -> Thermal Zoning", workspaceGroup);
    workspaceHeader->setStyleSheet("font-size:16px; font-weight:700; color:#f3f7fb;");
    auto* workspaceSubheader = new QLabel(
        "The workspace stays preview-first: review the generated pack layout, inspect CAD-derived zone mapping, and keep decisions centered on the trade study.",
        workspaceGroup);
    workspaceSubheader->setWordWrap(true);
    workspaceSubheader->setStyleSheet("font-size:12px; color:#93a6ba;");
    workspaceLayout->addWidget(workspaceHeader);
    workspaceLayout->addWidget(workspaceSubheader);

    auto* frame = new QFrame(workspaceGroup);
    frame->setObjectName("workspacePanel");
    frame->setStyleSheet(
        "#workspacePanel {"
        "background:#e9edf2;"
        "border:1px solid #48515b;"
        "border-radius:12px;"
        "}");
    auto* frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    workspaceView = new CadViewportWidget(frame);
    frameLayout->addWidget(workspaceView, 1);
    workspaceLayout->addWidget(frame, 1);

    auto* readoutGroup = new QGroupBox("Layout Readout", workspaceGroup);
    auto* readoutLayout = new QVBoxLayout(readoutGroup);
    readoutLayout->setContentsMargins(14, 16, 14, 14);
    readoutLayout->setSpacing(8);
    layoutSummaryLabel = new QLabel("Archetype and pack layout summary will appear here.", readoutGroup);
    thermalZoneSummaryLabel = new QLabel("Thermal zoning summary will appear here.", readoutGroup);
    workspaceSelectionLabel = new QLabel(
        "Workspace focus: preview-only. Click a group, module, or cooling channel to inspect the generated mapping.",
        readoutGroup);
    layoutSummaryLabel->setWordWrap(true);
    thermalZoneSummaryLabel->setWordWrap(true);
    workspaceSelectionLabel->setWordWrap(true);
    layoutSummaryLabel->setStyleSheet("color:#d8e5f2;");
    thermalZoneSummaryLabel->setStyleSheet("color:#d8e5f2;");
    workspaceSelectionLabel->setStyleSheet("color:#93a6ba;");
    readoutLayout->addWidget(layoutSummaryLabel);
    readoutLayout->addWidget(thermalZoneSummaryLabel);
    readoutLayout->addWidget(workspaceSelectionLabel);
    workspaceLayout->addWidget(readoutGroup);

    layout->addWidget(workspaceGroup, 1);

    auto* reportGroup = new QGroupBox("", this);
    auto* reportLayout = new QVBoxLayout(reportGroup);
    reportLayout->setContentsMargins(12, 12, 12, 12);
    reportLayout->setSpacing(8);
    auto* reportHeader = new QLabel("Trade-Study Report Preview", reportGroup);
    reportHeader->setStyleSheet("font-size:14px; font-weight:700; color:#f3f7fb;");
    auto* reportHint = new QLabel(
        "This preview mirrors the concise report narrative that can be exported once a candidate or candidate-vs-baseline view is ready.",
        reportGroup);
    reportHint->setWordWrap(true);
    reportHint->setStyleSheet("font-size:12px; color:#93a6ba;");
    reportNotes = new QPlainTextEdit(reportGroup);
    reportNotes->setReadOnly(true);
    reportNotes->setMinimumHeight(180);
    reportLayout->addWidget(reportHeader);
    reportLayout->addWidget(reportHint);
    reportLayout->addWidget(reportNotes, 1);
    layout->addWidget(reportGroup);
}
