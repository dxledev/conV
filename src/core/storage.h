#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace conv {

/** @brief Resolve the configured user directory, honoring CONV_CONFIG_DIR. */
QString defaultConfigDirectory();
/** @brief Read a JSON object from disk and throw on I/O, size, or format errors. */
QJsonObject readJson(const QString &path);
/** @brief Atomically write a JSON object, creating parent directories as needed. */
void writeJson(const QString &path, const QJsonObject &data, QJsonDocument::JsonFormat format = QJsonDocument::Indented);
/** @brief Atomically write text, skipping the write when the file already holds it. */
bool writeTextIfChanged(const QString &path, const QByteArray &bytes);

}
