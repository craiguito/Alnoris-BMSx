#pragma once

#include <QWidget>

class CadViewportWidget;
class QLabel;
class QPlainTextEdit;

class TradeStudyWorkspacePanel : public QWidget
{
public:
    explicit TradeStudyWorkspacePanel(QWidget* parent = nullptr);

    CadViewportWidget* workspaceView = nullptr;
    QLabel* layoutSummaryLabel = nullptr;
    QLabel* thermalZoneSummaryLabel = nullptr;
    QLabel* workspaceSelectionLabel = nullptr;
    QPlainTextEdit* reportNotes = nullptr;
};

