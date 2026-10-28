/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "paneldocument.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QUuid>
#include <cmath>

namespace ControlPanel
{
namespace
{
bool hasSafeDepth(const QByteArray &json)
{
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (const char character : json) {
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inString = false;
            }
        } else if (character == '"') {
            inString = true;
        } else if (character == '{' || character == '[') {
            if (++depth > PanelDocument::MaxDepth) {
                return false;
            }
        } else if (character == '}' || character == ']') {
            --depth;
        }
    }
    return true;
}

bool text(const QJsonValue &value, qsizetype maximum, bool nonEmpty = false)
{
    return value.isString() && value.toString().size() <= maximum && (!nonEmpty || !value.toString().trimmed().isEmpty());
}

bool integer(const QJsonValue &value, qint64 minimum, qint64 maximum)
{
    const double number = value.toDouble(-1);
    return value.isDouble() && std::isfinite(number) && std::floor(number) == number && number >= minimum && number <= maximum;
}

QJsonObject project(const QJsonObject &source, const QStringList &keys)
{
    QJsonObject result;
    for (const QString &key : keys) {
        if (source.contains(key)) {
            result.insert(key, source.value(key));
        }
    }
    return result;
}
}

PanelDocument::PanelDocument(const QJsonObject &config)
    : m_config(config)
{
}

std::optional<PanelDocument> PanelDocument::fromJson(const QByteArray &json, QString *error)
{
    if (error) {
        error->clear();
    }
    if (json.size() > MaxBytes) {
        if (error) {
            *error = QStringLiteral("config_too_large");
        }
        return std::nullopt;
    }
    if (!hasSafeDepth(json)) {
        if (error) {
            *error = QStringLiteral("config_too_deep");
        }
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = QStringLiteral("invalid_json");
        }
        return std::nullopt;
    }
    if (!validate(document.object(), error)) {
        return std::nullopt;
    }
    return PanelDocument(document.object());
}

bool PanelDocument::validate(const QJsonObject &config, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return false;
    };
    static const QRegularExpression idPattern(QStringLiteral("\\A[A-Za-z0-9][A-Za-z0-9._-]{0,63}\\z"));
    static const QRegularExpression assetPattern(QStringLiteral("\\A[a-f0-9]{64}\\z"));
    static const QRegularExpression colorPattern(QStringLiteral("\\A#[a-fA-F0-9]{6}\\z"));
    static const QSet<QString> modes = {QStringLiteral("single"), QStringLiteral("pages"), QStringLiteral("follow")};
    static const QSet<QString> kinds = {QStringLiteral("program"),
                                        QStringLiteral("website"),
                                        QStringLiteral("file"),
                                        QStringLiteral("folder"),
                                        QStringLiteral("shortcut"),
                                        QStringLiteral("system"),
                                        QStringLiteral("command")};
    static const QSet<QString> dials = {QStringLiteral("digital"),
                                        QStringLiteral("analog"),
                                        QStringLiteral("weather"),
                                        QStringLiteral("info"),
                                        QStringLiteral("car")};

    if (!integer(config.value(QStringLiteral("schemaVersion")), 1, 1)) {
        return fail(QStringLiteral("unsupported_version"));
    }
    if (!integer(config.value(QStringLiteral("revision")), 0, MaxRevision)) {
        return fail(QStringLiteral("invalid_revision"));
    }
    if (!modes.contains(config.value(QStringLiteral("mode")).toString())) {
        return fail(QStringLiteral("invalid_mode"));
    }
    const QJsonArray pages = config.value(QStringLiteral("pages")).toArray();
    if (pages.isEmpty() || pages.size() > MaxPages) {
        return fail(QStringLiteral("invalid_page_count"));
    }
    QSet<QString> pageIds;
    QSet<QString> buttonIds;
    for (const QJsonValue &pageValue : pages) {
        const QJsonObject page = pageValue.toObject();
        const QString id = page.value(QStringLiteral("id")).toString();
        if (!idPattern.match(id).hasMatch() || pageIds.contains(id)) {
            return fail(QStringLiteral("invalid_page_id"));
        }
        pageIds.insert(id);
        if (!text(page.value(QStringLiteral("name")), 120, true)) {
            return fail(QStringLiteral("invalid_page_name"));
        }
        const QJsonValue application = page.value(QStringLiteral("application"));
        if (!application.isUndefined() && (!text(application, 260) || application.toString().contains(QChar::Null))) {
            return fail(QStringLiteral("invalid_application"));
        }
        const QJsonArray buttonSlots = page.value(QStringLiteral("slots")).toArray();
        if (buttonSlots.size() != SlotCount) {
            return fail(QStringLiteral("invalid_slot_count"));
        }
        for (const QJsonValue &slot : buttonSlots) {
            if (slot.isNull()) {
                continue;
            }
            const QJsonObject button = slot.toObject();
            const QString buttonId = button.value(QStringLiteral("id")).toString();
            if (!idPattern.match(buttonId).hasMatch() || buttonIds.contains(buttonId)) {
                return fail(QStringLiteral("invalid_button_id"));
            }
            buttonIds.insert(buttonId);
            if (!text(button.value(QStringLiteral("name")), 120, true)) {
                return fail(QStringLiteral("invalid_button_name"));
            }
            if (!kinds.contains(button.value(QStringLiteral("kind")).toString())) {
                return fail(QStringLiteral("invalid_button_kind"));
            }
            const QJsonValue icon = button.value(QStringLiteral("icon"));
            if (!icon.isUndefined() && (!icon.isString() || (!icon.toString().isEmpty() && !assetPattern.match(icon.toString()).hasMatch()))) {
                return fail(QStringLiteral("invalid_icon"));
            }
        }
    }
    if (!pageIds.contains(config.value(QStringLiteral("homePageId")).toString())
        || !pageIds.contains(config.value(QStringLiteral("activePageId")).toString())) {
        return fail(QStringLiteral("invalid_active_page"));
    }
    const QJsonObject appearance = config.value(QStringLiteral("appearance")).toObject();
    if (!colorPattern.match(appearance.value(QStringLiteral("background")).toString()).hasMatch()) {
        return fail(QStringLiteral("invalid_background"));
    }
    if (!appearance.value(QStringLiteral("showLabels")).isBool() || !appearance.value(QStringLiteral("showVolume")).isBool()) {
        return fail(QStringLiteral("invalid_appearance"));
    }
    const QString direction = appearance.value(QStringLiteral("swipeDirection")).toString();
    if (direction != QLatin1String("horizontal") && direction != QLatin1String("vertical")) {
        return fail(QStringLiteral("invalid_swipe_direction"));
    }
    const QJsonObject idle = config.value(QStringLiteral("idle")).toObject();
    if (!idle.value(QStringLiteral("enabled")).isBool() || !integer(idle.value(QStringLiteral("timeoutMs")), 1000, 86400000)) {
        return fail(QStringLiteral("invalid_idle_timeout"));
    }
    if (!dials.contains(idle.value(QStringLiteral("dialId")).toString())) {
        return fail(QStringLiteral("invalid_dial"));
    }
    if (!text(idle.value(QStringLiteral("city")), 120) || !text(idle.value(QStringLiteral("message")), 240)) {
        return fail(QStringLiteral("invalid_dial_text"));
    }
    return true;
}

PanelDocument PanelDocument::empty()
{
    const QString pageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonArray buttonSlots;
    for (int i = 0; i < SlotCount; ++i) {
        buttonSlots.append(QJsonValue::Null);
    }
    return PanelDocument({
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("revision"), 0},
        {QStringLiteral("mode"), QStringLiteral("single")},
        {QStringLiteral("homePageId"), pageId},
        {QStringLiteral("activePageId"), pageId},
        {QStringLiteral("pages"),
         QJsonArray{QJsonObject{{QStringLiteral("id"), pageId},
                                {QStringLiteral("name"), QStringLiteral("Home")},
                                {QStringLiteral("application"), QString()},
                                {QStringLiteral("slots"), buttonSlots}}}},
        {QStringLiteral("appearance"),
         QJsonObject{{QStringLiteral("background"), QStringLiteral("#FFFFFF")},
                     {QStringLiteral("showLabels"), true},
                     {QStringLiteral("showVolume"), true},
                     {QStringLiteral("swipeDirection"), QStringLiteral("horizontal")}}},
        {QStringLiteral("idle"),
         QJsonObject{{QStringLiteral("enabled"), true},
                     {QStringLiteral("timeoutMs"), 5000},
                     {QStringLiteral("dialId"), QStringLiteral("digital")},
                     {QStringLiteral("city"), QString()},
                     {QStringLiteral("message"), QString()}}},
    });
}

const QJsonObject &PanelDocument::privateConfig() const
{
    return m_config;
}

QJsonObject PanelDocument::publicConfig() const
{
    QJsonObject result = project(
        m_config,
        {QStringLiteral("schemaVersion"), QStringLiteral("revision"), QStringLiteral("mode"), QStringLiteral("homePageId"), QStringLiteral("activePageId")});
    QJsonArray pages;
    for (const QJsonValue &pageValue : m_config.value(QStringLiteral("pages")).toArray()) {
        const QJsonObject source = pageValue.toObject();
        QJsonObject page = project(source, {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("application")});
        QJsonArray buttonSlots;
        for (const QJsonValue &slot : source.value(QStringLiteral("slots")).toArray()) {
            buttonSlots.append(
                slot.isNull()
                    ? QJsonValue(QJsonValue::Null)
                    : QJsonValue(project(slot.toObject(), {QStringLiteral("id"), QStringLiteral("kind"), QStringLiteral("name"), QStringLiteral("icon")})));
        }
        page.insert(QStringLiteral("slots"), buttonSlots);
        pages.append(page);
    }
    result.insert(QStringLiteral("pages"), pages);
    result.insert(QStringLiteral("appearance"),
                  project(m_config.value(QStringLiteral("appearance")).toObject(),
                          {QStringLiteral("background"), QStringLiteral("showLabels"), QStringLiteral("showVolume"), QStringLiteral("swipeDirection")}));
    result.insert(
        QStringLiteral("idle"),
        project(m_config.value(QStringLiteral("idle")).toObject(),
                {QStringLiteral("enabled"), QStringLiteral("timeoutMs"), QStringLiteral("dialId"), QStringLiteral("city"), QStringLiteral("message")}));
    return result;
}

qint64 PanelDocument::revision() const
{
    return m_config.value(QStringLiteral("revision")).toInteger();
}

QJsonObject PanelDocument::button(const QString &id) const
{
    for (const QJsonValue &page : m_config.value(QStringLiteral("pages")).toArray()) {
        for (const QJsonValue &slot : page.toObject().value(QStringLiteral("slots")).toArray()) {
            const QJsonObject button = slot.toObject();
            if (button.value(QStringLiteral("id")).toString() == id && !id.isEmpty()) {
                return button;
            }
        }
    }
    return {};
}
}
