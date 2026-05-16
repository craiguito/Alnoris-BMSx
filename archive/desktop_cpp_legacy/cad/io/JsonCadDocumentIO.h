#pragma once

#include "../core/CadDocument.h"

#include <QJsonObject>
#include <QString>

namespace cad::io {

QJsonObject serializeCadDocument(const core::CadDocument& document);
bool deserializeCadDocument(const QJsonObject& object, core::CadDocument& document, QString* error = nullptr);
bool tryLoadCadDocumentFromProject(const QJsonObject& project_root, core::CadDocument& document, QString* error = nullptr);

} // namespace cad::io
