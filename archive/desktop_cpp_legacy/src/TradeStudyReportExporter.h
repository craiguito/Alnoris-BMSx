#pragma once

#include "TradeStudySummary.h"

#include <QString>

namespace trade_study {

QString buildMarkdownReport(const ComparisonSummary& summary);
bool writeMarkdownReport(const QString& path, const ComparisonSummary& summary, QString* error);

} // namespace trade_study

