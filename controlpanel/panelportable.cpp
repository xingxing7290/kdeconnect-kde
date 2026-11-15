/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "panelportable.h"

#include "panelassets.h"
#include "paneldocument.h"
#include "panelstore.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QSaveFile>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <cmath>
#include <optional>

namespace ControlPanel
{
namespace
{
constexpr int FormatVersion = 1;
constexpr qint64 MaxBundleBytes = 64 * 1024 * 1024;
constexpr int MaxAssets = PanelDocument::MaxPages * PanelDocument::SlotCount;
constexpr qint64 MaxEncodedAsset = (PanelAssets::MaxBytes / 3 + 2) * 4;

QString bundleFormat()
{
    return QStringLiteral("kdeconnect-controlpanel");
}

QVariantMap fail(QString *error, const QString &code)
{
    if (error)
        *error = code;
    return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), code}};
}

void setError(QString *error, const QString &code)
{
    if (error)
        *error = code;
}

QString localPath(const QUrl &url, QString *error)
{
    if (!url.isLocalFile() || url.toLocalFile().isEmpty()) {
        setError(error, QStringLiteral("invalid_target"));
        return {};
    }
    return url.toLocalFile();
}

/** Same nesting guard as PanelDocument; bundle JSON is user-editable text. */
bool safeDepth(const QByteArray &json)
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
            if (++depth > PanelDocument::MaxDepth)
                return false;
        } else if (character == '}' || character == ']') {
            --depth;
        }
    }
    return true;
}

std::optional<QByteArray> readBundle(const QUrl &url, QString *error)
{
    const QString path = localPath(url, error);
    if (path.isEmpty())
        return std::nullopt;
    const QFileInfo info(path);
    if (!info.isFile() || info.isSymLink()) {
        setError(error, QStringLiteral("bundle_unreadable"));
        return std::nullopt;
    }
    if (info.size() > MaxBundleBytes) {
        setError(error, QStringLiteral("bundle_too_large"));
        return std::nullopt;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("bundle_unreadable"));
        return std::nullopt;
    }
    const auto bytes = file.read(MaxBundleBytes + 1);
    if (bytes.size() > MaxBundleBytes) {
        setError(error, QStringLiteral("bundle_too_large"));
        return std::nullopt;
    }
    return bytes;
}

struct Bundle
{
    QJsonObject config;
    QList<QPair<QString, QByteArray>> assets;
    QVariantMap summary;
};

/** Parses and fully validates a bundle. No files are written here. */
std::optional<Bundle> parseBundle(const QByteArray &bytes, const PanelAssets &assets, QString *error)
{
    if (bytes.size() > MaxBundleBytes) {
        setError(error, QStringLiteral("bundle_too_large"));
        return std::nullopt;
    }
    if (!safeDepth(bytes)) {
        setError(error, QStringLiteral("bundle_too_deep"));
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("invalid_json"));
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString() != bundleFormat()) {
        setError(error, QStringLiteral("unsupported_bundle"));
        return std::nullopt;
    }
    const double version = root.value(QStringLiteral("formatVersion")).toDouble(-1);
    if (!std::isfinite(version) || version != FormatVersion) {
        setError(error, QStringLiteral("unsupported_bundle_version"));
        return std::nullopt;
    }
    const QJsonValue configValue = root.value(QStringLiteral("config"));
    const QJsonValue assetsValue = root.value(QStringLiteral("assets"));
    if (!configValue.isObject()) {
        setError(error, QStringLiteral("bundle_missing_config"));
        return std::nullopt;
    }
    if (!assetsValue.isArray()) {
        setError(error, QStringLiteral("bundle_missing_assets"));
        return std::nullopt;
    }
    const QJsonArray assetArray = assetsValue.toArray();
    if (assetArray.size() > MaxAssets) {
        setError(error, QStringLiteral("too_many_assets"));
        return std::nullopt;
    }

    // A document may be edited by hand. Its revision is irrelevant: the local
    // store owns the monotonic counter, so normalize it before validation.
    QJsonObject config = configValue.toObject();
    const qint64 exportedRevision = config.value(QStringLiteral("revision")).toInteger(-1);
    config.insert(QStringLiteral("revision"), 0);
    QString validationError;
    const auto parsed = PanelDocument::fromJson(QJsonDocument(config).toJson(QJsonDocument::Compact), &validationError);
    if (!parsed) {
        setError(error, validationError);
        return std::nullopt;
    }
    config = parsed->privateConfig();

    QSet<QString> ids;
    Bundle bundle;
    for (const QJsonValue &value : assetArray) {
        const QJsonObject entry = value.toObject();
        const QString id = entry.value(QStringLiteral("id")).toString();
        if (!PanelAssets::validId(id)) {
            setError(error, QStringLiteral("invalid_bundle_asset"));
            return std::nullopt;
        }
        if (ids.contains(id)) {
            setError(error, QStringLiteral("duplicate_asset"));
            return std::nullopt;
        }
        ids.insert(id);
        if (entry.value(QStringLiteral("encoding")).toString() != QStringLiteral("base64")) {
            setError(error, QStringLiteral("invalid_bundle_asset"));
            return std::nullopt;
        }
        const QByteArray encoded = entry.value(QStringLiteral("data")).toString().toLatin1();
        if (encoded.isEmpty() || encoded.size() > MaxEncodedAsset) {
            setError(error, QStringLiteral("asset_too_large"));
            return std::nullopt;
        }
        const QByteArray decoded = QByteArray::fromBase64(encoded);
        // Round-tripping rejects malformed padding, stray characters and truncation.
        if (decoded.isEmpty() || decoded.toBase64() != encoded || decoded.size() > PanelAssets::MaxBytes) {
            setError(error, QStringLiteral("invalid_bundle_asset"));
            return std::nullopt;
        }
        if (QCryptographicHash::hash(decoded, QCryptographicHash::Sha256).toHex() != id.toLatin1()) {
            setError(error, QStringLiteral("asset_hash_mismatch"));
            return std::nullopt;
        }
        if (!PanelAssets::validPng(decoded)) {
            setError(error, QStringLiteral("invalid_bundle_asset"));
            return std::nullopt;
        }
        const QJsonValue declaredHash = entry.value(QStringLiteral("sha256"));
        if (!declaredHash.isUndefined() && declaredHash.toString() != id) {
            setError(error, QStringLiteral("asset_hash_mismatch"));
            return std::nullopt;
        }
        const QJsonValue declaredSize = entry.value(QStringLiteral("bytes"));
        if (!declaredSize.isUndefined() && declaredSize.toInteger(-1) != decoded.size()) {
            setError(error, QStringLiteral("asset_size_mismatch"));
            return std::nullopt;
        }
        bundle.assets.append(QPair<QString, QByteArray>(id, decoded));
    }

    // Every non-empty icon reference of every slot of every page must resolve,
    // either inside the bundle or in the already-installed local asset store.
    QStringList missing;
    int pageCount = 0;
    int buttonCount = 0;
    int alreadyPresent = 0;
    QVariantList pageDetails;
    for (const QJsonValue &pageValue : config.value(QStringLiteral("pages")).toArray()) {
        const QJsonObject page = pageValue.toObject();
        ++pageCount;
        int pageButtons = 0;
        for (const QJsonValue &slot : page.value(QStringLiteral("slots")).toArray()) {
            if (slot.isNull())
                continue;
            ++buttonCount;
            ++pageButtons;
            const QString icon = slot.toObject().value(QStringLiteral("icon")).toString();
            if (icon.isEmpty())
                continue;
            if (!ids.contains(icon)) {
                if (assets.read(icon))
                    ++alreadyPresent;
                else
                    missing.append(icon);
            }
        }
        pageDetails.append(QVariantMap{{QStringLiteral("name"), page.value(QStringLiteral("name")).toString()},
                                       {QStringLiteral("buttons"), pageButtons}});
    }
    bundle.config = config;
    bundle.summary = {
        {QStringLiteral("pages"), pageCount},
        {QStringLiteral("buttons"), buttonCount},
        {QStringLiteral("assets"), bundle.assets.size()},
        {QStringLiteral("alreadyPresent"), alreadyPresent},
        {QStringLiteral("missingAssets"), missing},
        {QStringLiteral("pageDetails"), pageDetails},
        {QStringLiteral("exportedAt"), root.value(QStringLiteral("exportedAt")).toString()},
        {QStringLiteral("exportedRevision"), exportedRevision},
        {QStringLiteral("formatVersion"), FormatVersion},
        {QStringLiteral("bytes"), bytes.size()},
    };
    if (!missing.isEmpty()) {
        setError(error, QStringLiteral("asset_missing_in_bundle"));
        return std::nullopt;
    }
    return bundle;
}

std::optional<Bundle> loadBundle(const QUrl &url, const PanelAssets &assets, QString *error)
{
    QString readError;
    const auto bytes = readBundle(url, &readError);
    if (!bytes) {
        setError(error, readError);
        return std::nullopt;
    }
    return parseBundle(*bytes, assets, error);
}
}

QVariantMap PanelPortable::inspect(const QUrl &url, const PanelAssets &assets, QString *error)
{
    QString detail;
    const auto bundle = loadBundle(url, assets, &detail);
    if (!bundle)
        return fail(error, detail);
    QVariantMap result = bundle->summary;
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("error"), QString());
    result.insert(QStringLiteral("path"), url.toLocalFile());
    return result;
}

QVariantMap PanelPortable::exportTo(const QUrl &url, const PanelStore &store, const PanelAssets &assets, QString *error)
{
    if (!store.isUsable())
        return fail(error, QStringLiteral("store_unavailable"));
    const QJsonObject config = store.document().privateConfig();

    QStringList iconIds;
    QSet<QString> seen;
    int pageCount = 0;
    int buttonCount = 0;
    QVariantList pageDetails;
    for (const QJsonValue &pageValue : config.value(QStringLiteral("pages")).toArray()) {
        const QJsonObject page = pageValue.toObject();
        ++pageCount;
        int pageButtons = 0;
        for (const QJsonValue &slot : page.value(QStringLiteral("slots")).toArray()) {
            if (slot.isNull())
                continue;
            ++buttonCount;
            ++pageButtons;
            const QString id = slot.toObject().value(QStringLiteral("icon")).toString();
            if (!id.isEmpty() && !seen.contains(id)) {
                seen.insert(id);
                iconIds.append(id);
            }
        }
        pageDetails.append(QVariantMap{{QStringLiteral("name"), page.value(QStringLiteral("name")).toString()},
                                       {QStringLiteral("buttons"), pageButtons}});
    }

    // Refuse a partial export: a broken or missing icon would silently lose a
    // website favicon or an application icon after the restore.
    QJsonArray assetArray;
    for (const QString &id : iconIds) {
        const auto bytes = assets.read(id);
        if (!bytes)
            return fail(error, QStringLiteral("asset_missing"));
        QJsonObject entry;
        entry.insert(QStringLiteral("id"), id);
        entry.insert(QStringLiteral("encoding"), QStringLiteral("base64"));
        entry.insert(QStringLiteral("bytes"), bytes->size());
        entry.insert(QStringLiteral("sha256"), id);
        entry.insert(QStringLiteral("data"), QString::fromLatin1(bytes->toBase64()));
        assetArray.append(entry);
    }

    const QString exportedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QJsonObject documentSummary;
    documentSummary.insert(QStringLiteral("pages"), pageCount);
    documentSummary.insert(QStringLiteral("buttons"), buttonCount);
    documentSummary.insert(QStringLiteral("assets"), assetArray.size());
    documentSummary.insert(QStringLiteral("revision"), config.value(QStringLiteral("revision")).toInteger(0));
    documentSummary.insert(QStringLiteral("exportedAt"), exportedAt);
    QJsonObject root;
    root.insert(QStringLiteral("format"), bundleFormat());
    root.insert(QStringLiteral("formatVersion"), FormatVersion);
    root.insert(QStringLiteral("exportedAt"), exportedAt);
    root.insert(QStringLiteral("generator"), QStringLiteral("kdeconnect-controlpanel-desktop"));
    root.insert(QStringLiteral("summary"), documentSummary);
    root.insert(QStringLiteral("config"), config);
    root.insert(QStringLiteral("assets"), assetArray);
    const QByteArray encoded = QJsonDocument(root).toJson(QJsonDocument::Indented);

    QString path = localPath(url, error);
    if (path.isEmpty())
        return fail(error, QStringLiteral("invalid_target"));
    if (!path.endsWith(QLatin1String(".json"), Qt::CaseInsensitive))
        path += QLatin1String(".kdpanel.json");
    const QFileInfo target(path);
    const QFileInfo storeFile(store.filePath());
    const bool reservedName = target.fileName() == storeFile.fileName()
        || target.fileName() == storeFile.fileName() + QLatin1String(".bak")
        || target.fileName() == QStringLiteral("revision");
    if (target.isDir() || (target.absolutePath() == storeFile.absolutePath() && reservedName))
        return fail(error, QStringLiteral("invalid_target"));
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly))
        return fail(error, QStringLiteral("bundle_write_failed"));
    output.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    if (output.write(encoded) != encoded.size() || !output.commit())
        return fail(error, QStringLiteral("bundle_write_failed"));

    QVariantMap result;
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("error"), QString());
    result.insert(QStringLiteral("path"), path);
    result.insert(QStringLiteral("bytes"), encoded.size());
    result.insert(QStringLiteral("pages"), pageCount);
    result.insert(QStringLiteral("buttons"), buttonCount);
    result.insert(QStringLiteral("assets"), assetArray.size());
    result.insert(QStringLiteral("pageDetails"), pageDetails);
    result.insert(QStringLiteral("exportedAt"), exportedAt);
    return result;
}

QVariantMap PanelPortable::importFrom(const QUrl &url, PanelStore &store, const PanelAssets &assets, QString *error)
{
    if (!store.isUsable())
        return fail(error, QStringLiteral("store_unavailable"));
    QString detail;
    const auto bundle = loadBundle(url, assets, &detail);
    if (!bundle)
        return fail(error, detail);

    // Icons are content-addressed: installing an existing id is a no-op, and a
    // failed config write only leaves harmless extra PNGs behind.
    for (const auto &entry : bundle->assets) {
        QString installError;
        if (!assets.install(entry.first, entry.second, &installError))
            return fail(error, installError);
    }

    QJsonObject config = bundle->config;
    const qint64 current = store.document().revision();
    config.insert(QStringLiteral("revision"), current);
    if (!store.save(config, current))
        return fail(error, store.lastError());

    QVariantMap result = bundle->summary;
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("error"), QString());
    result.insert(QStringLiteral("path"), url.toLocalFile());
    result.insert(QStringLiteral("revision"), store.document().revision());
    return result;
}
}
