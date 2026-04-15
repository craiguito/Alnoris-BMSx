#pragma once

#include <QWidget>

class QComboBox;
class QFormLayout;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

class TradeStudyTestsPanel : public QWidget
{
public:
    explicit TradeStudyTestsPanel(QWidget* parent = nullptr);

    QComboBox* virtualTestCombo = nullptr;
    QLabel* virtualTestDescription = nullptr;
    QFormLayout* virtualTestFormLayout = nullptr;
    QPlainTextEdit* virtualTestStatus = nullptr;
    QTableWidget* virtualTestComparisonTable = nullptr;
    QPushButton* vetTestButton = nullptr;
    QPushButton* runTestButton = nullptr;
};

