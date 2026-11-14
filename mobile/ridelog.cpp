/*
    This file is part of VESC Tool.

    VESC Tool is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    VESC Tool is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with VESC Tool.  If not, see <http://www.gnu.org/licenses/>.
    */

#include "ridelog.h"
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QPointF>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
const int blockSize = 256;
const qint64 maxFileSize = 512 * 1024 * 1024;
const qint64 maxValues = 32 * 1024 * 1024;

struct Extremes {
    int minimum = -1;
    int maximum = -1;
    bool hasGap = false;
};

struct LogColumn {
    QString key;
    QString name;
    QString unit;
    bool relative = false;
    bool timestamp = false;
    QVector<double> values;
    QVector<QVector<Extremes>> levels;

    void add(Extremes &result, int index) const {
        if (index < 0) return;
        if (!std::isfinite(values.at(index))) {
            result.hasGap = true;
            return;
        }
        if (result.minimum < 0 || values.at(index) < values.at(result.minimum)) result.minimum = index;
        if (result.maximum < 0 || values.at(index) > values.at(result.maximum)) result.maximum = index;
    }

    Extremes range(int first, int end) const {
        Extremes result;
        while (first < end && first % blockSize) add(result, first++);
        while (first + blockSize <= end) {
            int block = first / blockSize;
            int level = 0;
            while (level + 1 < levels.size() && block % (2 << level) == 0 &&
                   first + qint64(blockSize) * (2 << level) <= end) {
                level++;
            }
            const auto &e = levels.at(level).at(block >> level);
            add(result, e.minimum);
            add(result, e.maximum);
            result.hasGap |= e.hasGap;
            first += blockSize * (1 << level);
        }
        while (first < end) add(result, first++);
        return result;
    }

    void index() {
        QVector<Extremes> level;
        for (int first = 0; first < values.size(); first += blockSize) {
            Extremes e;
            for (int i = first; i < qMin(first + blockSize, int(values.size())); i++) add(e, i);
            level.append(e);
        }
        levels.append(level);
        while (level.size() > 1) {
            QVector<Extremes> next;
            for (int i = 0; i < level.size(); i += 2) {
                Extremes e = level.at(i);
                if (i + 1 < level.size()) {
                    add(e, level.at(i + 1).minimum);
                    add(e, level.at(i + 1).maximum);
                    e.hasGap |= level.at(i + 1).hasGap;
                }
                next.append(e);
            }
            levels.append(next);
            level = next;
        }
    }
};

QString localPath(QString path)
{
    return path.startsWith("file:") ? QUrl(path).toLocalFile() : path;
}

QStringList csvLine(const QString &line, QChar separator)
{
    if (!line.contains('"')) return line.split(separator);
    QStringList fields;
    QString field;
    bool quoted = false;
    for (int i = 0; i < line.size(); i++) {
        QChar ch = line.at(i);
        if (ch == '"') {
            if (quoted && i + 1 < line.size() && line.at(i + 1) == '"') {
                field += ch;
                i++;
            } else {
                quoted = !quoted;
            }
        } else if (ch == separator && !quoted) {
            fields.append(field);
            field.clear();
        } else {
            field += ch;
        }
    }
    fields.append(field);
    return fields;
}

QString rtUnit(const QString &key)
{
    if (key.startsWith("ms_today") || key == "gnss_posTime") return "ms";
    if (key.contains("voltage")) return "V";
    if (key.startsWith("temp_")) return "°C";
    if (key.contains("current")) return "A";
    if (key.startsWith("amp_hours")) return "Ah";
    if (key.startsWith("watt_hours")) return "Wh";
    if (key == "battery_wh_tot") return "Wh";
    if (key == "speed_meters_per_sec" || key == "gnss_gVel" || key == "gnss_vVel") return "m/s";
    if (key.startsWith("tacho_") || key == "gnss_alt" || key == "gnss_hAcc" || key == "gnss_vAcc") return "m";
    if (key == "gnss_lat" || key == "gnss_lon" || key == "encoder_position") return "°";
    if (key == "roll" || key == "pitch" || key == "yaw") return "rad";
    if (key.startsWith("acc")) return "G";
    if (key.startsWith("gyro")) return "°/s";
    if (key == "power_in" || key == "setup_power" || key == "power") return "W";
    return QString();
}

QString rtName(QString key)
{
    static const QHash<QString, QString> names {
        {"ms_today", "Time"},
        {"input_voltage", "Battery voltage"},
        {"current_motor", "Motor current"},
        {"current_motor_setup", "Total motor current"},
        {"current_in", "Battery current"},
        {"current_in_setup", "Total battery current"},
        {"speed_meters_per_sec", "Speed"},
        {"temp_motor", "Motor temperature"},
        {"temp_mos_max", "Controller temperature"},
        {"temp_mos_1", "MOSFET temperature 1"},
        {"temp_mos_2", "MOSFET temperature 2"},
        {"temp_mos_3", "MOSFET temperature 3"},
        {"tacho_meters", "Distance"},
        {"tacho_abs_meters", "Absolute distance"},
        {"gnss_gVel", "GNSS speed"},
        {"gnss_vVel", "GNSS vertical speed"},
        {"gnss_lat", "Latitude"},
        {"gnss_lon", "Longitude"},
        {"gnss_alt", "Altitude"},
        {"gnss_hAcc", "GNSS horizontal accuracy"},
        {"gnss_vAcc", "GNSS vertical accuracy"},
        {"power_in", "Battery power"},
        {"setup_power", "Total battery power"},
        {"power", "Power"},
        {"erpm", "ERPM"}
    };
    if (names.contains(key)) return names.value(key);
    key.replace('_', ' ');
    if (!key.isEmpty()) key[0] = key.at(0).toUpper();
    return key;
}
}

struct RideLogData {
    QVector<LogColumn> columns;
    QVector<double> times;
    QVariantList fields;
    QVariantList route;
    QString error;
    QString timeLabel = "Time (s)";
    int latitude = -1;
    int longitude = -1;
    int positionTime = -1;
    int accuracy = -1;

    bool position(int index) const {
        if (latitude < 0 || longitude < 0) return false;
        double lat = columns.at(latitude).values.at(index);
        double lon = columns.at(longitude).values.at(index);
        return std::isfinite(lat) && std::isfinite(lon) && std::abs(lat) <= 90.0 &&
                std::abs(lon) <= 180.0 && (lat != 0.0 || lon != 0.0) &&
                (positionTime < 0 || columns.at(positionTime).values.at(index) >= 0.0) &&
                (accuracy < 0 || columns.at(accuracy).values.at(index) > 0.0);
    }
};

static std::shared_ptr<RideLogData> readLog(QString path, std::shared_ptr<std::atomic_bool> cancel,
                                          std::shared_ptr<std::atomic_int> progress)
{
    auto result = std::make_shared<RideLogData>();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result->error = file.errorString();
        return result;
    }
    if (file.size() > maxFileSize) {
        result->error = "Log file is larger than 512 MB";
        return result;
    }
    QTextStream stream(&file);
    QString header = stream.readLine(65537);
    if (header.size() > 65536) {
        result->error = "Log header is too long";
        return result;
    }
    QChar separator = header.contains(';') ? ';' : ',';
    auto headers = csvLine(header, separator);
    while (!headers.isEmpty() && headers.last().trimmed().isEmpty()) headers.removeLast();
    if (headers.isEmpty() || headers.size() > 512) {
        result->error = "Invalid log header";
        return result;
    }
    bool rt = headers.first() == "ms_today";
    bool custom = headers.first().contains(':');
    int timeColumn = -1;
    int voltageColumn = -1;
    int currentColumn = -1;
    bool hasPower = false;
    double timeScale = 1.0;
    for (int i = 0; i < headers.size(); i++) {
        LogColumn column;
        auto parts = headers.at(i).split(':');
        column.key = parts.first().trimmed();
        column.name = custom && parts.size() > 1 ? parts.at(1) : (rt ? rtName(column.key) : column.key);
        column.unit = custom && parts.size() > 2 ? parts.at(2) : (rt ? rtUnit(column.key) : QString());
        column.relative = custom && parts.size() > 4 && parts.at(4).toInt();
        column.timestamp = custom && parts.size() > 5 && parts.at(5).toInt();
        if (column.key == "t_day" || column.key == "ms_today" ||
            (timeColumn < 0 && (column.timestamp || column.key == "time" || column.key == "timestamp"))) {
            timeColumn = i;
            timeScale = column.unit == "ms" || column.key == "ms_today" ? 0.001 : 1.0;
        }
        if (column.key == "gnss_lat" || column.key == "latitude") result->latitude = i;
        if (column.key == "gnss_lon" || column.key == "longitude") result->longitude = i;
        if (column.key == "gnss_posTime" || column.key == "t_day_pos") result->positionTime = i;
        if (column.key == "gnss_hAcc" || column.key == "gnss_h_acc") result->accuracy = i;
        if (column.key == "input_voltage") voltageColumn = i;
        if (column.key == "current_in") currentColumn = i;
        if (column.key == "power_in" || column.key == "setup_power" || column.key == "power") hasPower = true;
        result->fields.append(QVariantMap{{"index", i}, {"key", column.key},
                                         {"name", column.name}, {"unit", column.unit}});
        result->columns.append(column);
    }
    bool derivePower = !hasPower && voltageColumn >= 0 && currentColumn >= 0;
    bool voltageKnown = false;
    bool currentKnown = false;
    if (derivePower) {
        LogColumn power;
        power.key = "power_in";
        power.name = "Battery power";
        power.unit = "W";
        result->fields.append(QVariantMap{{"index", result->columns.size()}, {"key", power.key},
                                         {"name", power.name}, {"unit", power.unit}});
        result->columns.append(power);
    }
    if (timeColumn < 0) result->timeLabel = "Sample";
    QVector<double> previous(headers.size(), custom ? 0.0 : std::numeric_limits<double>::quiet_NaN());
    QVector<double> firstValues;
    double firstTime = 0.0;
    double lastRawTime = 0.0;
    double dayOffset = 0.0;
    int skipped = 0;
    while (!stream.atEnd() && !cancel->load()) {
        QString line = stream.readLine(65537);
        if (line.size() > 65536) {
            result->error = "Log row is too long";
            return result;
        }
        if (line.trimmed().isEmpty()) continue;
        auto values = csvLine(line, separator);
        while (values.size() > headers.size() && values.last().trimmed().isEmpty()) values.removeLast();
        if (values.size() != headers.size()) {
            skipped++;
            continue;
        }
        QVector<double> row = previous;
        bool rowVoltageKnown = voltageKnown;
        bool rowCurrentKnown = currentKnown;
        for (int i = 0; i < values.size(); i++) {
            if (values.at(i).trimmed().isEmpty()) continue;
            bool ok;
            double value = values.at(i).toDouble(&ok);
            row[i] = ok && std::isfinite(value) ? value : std::numeric_limits<double>::quiet_NaN();
            if (i == voltageColumn) rowVoltageKnown = std::isfinite(row.at(i));
            if (i == currentColumn) rowCurrentKnown = std::isfinite(row.at(i));
        }
        double rawTime = timeColumn < 0 ? result->times.size() : row.at(timeColumn) * timeScale;
        if (!std::isfinite(rawTime)) {
            skipped++;
            continue;
        }
        if (result->times.isEmpty()) {
            firstTime = rawTime;
            firstValues = row;
        } else if (rawTime < lastRawTime) {
            if ((rt || headers.at(timeColumn).startsWith("t_day:")) && lastRawTime - rawTime > 43200.0) {
                dayOffset += 86400.0;
            } else {
                skipped++;
                continue;
            }
        }
        if (qint64(result->times.size() + 1) * (result->columns.size() + 1) > maxValues) {
            result->error = "Log contains too many values (256 MB limit)";
            return result;
        }
        lastRawTime = rawTime;
        previous = row;
        voltageKnown = rowVoltageKnown;
        currentKnown = rowCurrentKnown;
        result->times.append(rawTime + dayOffset - firstTime);
        for (int i = 0; i < row.size(); i++) {
            auto &column = result->columns[i];
            column.values.append(column.relative && !column.timestamp ? row.at(i) - firstValues.at(i) : row.at(i));
        }
        if (derivePower) {
            double power = row.at(voltageColumn) * row.at(currentColumn);
            result->columns.last().values.append(voltageKnown && currentKnown && std::isfinite(power) ?
                                                    power : std::numeric_limits<double>::quiet_NaN());
        }
        if (result->times.size() % 2048 == 0) progress->store(int(90 * file.pos() / qMax(qint64(1), file.size())));
    }
    if (cancel->load()) return result;
    if (result->times.isEmpty()) {
        result->error = "No valid samples in log";
        return result;
    }
    for (int i = 0; i < result->columns.size(); i++) {
        if (cancel->load()) return result;
        result->columns[i].index();
        progress->store(90 + 10 * (i + 1) / result->columns.size());
    }
    QVector<int> positions;
    QVector<int> positionBreaks;
    bool positionGap = false;
    for (int i = 0; i < result->times.size(); i++) {
        if (!result->position(i)) {
            if (!positions.isEmpty()) positionGap = true;
            continue;
        }
        if (positionGap || positions.isEmpty() ||
            result->columns.at(result->latitude).values.at(i) != result->columns.at(result->latitude).values.at(positions.last()) ||
            result->columns.at(result->longitude).values.at(i) != result->columns.at(result->longitude).values.at(positions.last())) {
            if (positionGap) positionBreaks.append(positions.size());
            positions.append(i);
        }
        positionGap = false;
    }
    int count = qMin(5000 - qMin(2500, int(positionBreaks.size())), int(positions.size()));
    int previousPosition = -1;
    for (int i = 0; i < count; i++) {
        int position = count == 1 ? 0 : qint64(i) * (positions.size() - 1) / (count - 1);
        auto gap = std::upper_bound(positionBreaks.begin(), positionBreaks.end(), previousPosition);
        if (i > 0 && gap != positionBreaks.end() && *gap <= position) {
            double missing = std::numeric_limits<double>::quiet_NaN();
            result->route.append(QVariantMap{{"latitude", missing}, {"longitude", missing}});
        }
        int index = positions.at(position);
        result->route.append(QVariantMap{{"latitude", result->columns.at(result->latitude).values.at(index)},
                                        {"longitude", result->columns.at(result->longitude).values.at(index)}});
        previousPosition = position;
    }
    Q_UNUSED(skipped)
    return result;
}

RideLog::RideLog(QObject *parent) : QObject(parent), mWorker(nullptr), mOperation(Idle),
    mProgress(0.0), mRetries(0), mOffset(0), mSize(-1), mRemoteSendCan(false), mRemoteCanId(-1)
{
    connect(&mTimer, &QTimer::timeout, this, [this]() {
        if (mOperation == Parsing) {
            setProgress(mParseProgress->load() / 100.0);
        } else if (mOperation != Idle) {
            if (mRetries++ < 3) requestRemote();
            else finish("Vehicle file transfer timed out");
        }
    });
}

RideLog::~RideLog()
{
    if (mCancel) mCancel->store(true);
    if (mWorker) mWorker->wait();
}

VescInterface *RideLog::vesc() const { return mVesc; }
bool RideLog::busy() const { return mOperation != Idle; }
double RideLog::progress() const { return mProgress; }
QString RideLog::error() const { return mError; }
QString RideLog::name() const { return mName; }
int RideLog::sampleCount() const { return mData ? mData->times.size() : 0; }
double RideLog::duration() const { return mData && !mData->times.isEmpty() ? mData->times.last() : 0.0; }
QString RideLog::timeLabel() const { return mData ? mData->timeLabel : "Time (s)"; }
QVariantList RideLog::fields() const { return mData ? mData->fields : QVariantList(); }
QVariantList RideLog::route() const { return mData ? mData->route : QVariantList(); }
QVariantList RideLog::files() const { return mFiles; }
QString RideLog::directory() const { return mDirectory; }

void RideLog::setError(QString error)
{
    if (mError != error) {
        mError = error;
        emit errorChanged();
    }
}

void RideLog::setProgress(double progress)
{
    if (mProgress != progress) {
        mProgress = progress;
        emit progressChanged();
    }
}

void RideLog::finish(QString error)
{
    if (!error.isEmpty() && (mOperation == Listing || mOperation == Reading)) mRemoteDrain.start();
    mTimer.stop();
    mOperation = Idle;
    mDownload.reset();
    setError(error);
    emit busyChanged();
}

void RideLog::setVesc(VescInterface *vesc)
{
    if (mVesc == vesc) return;
    if (mOperation == Listing || mOperation == Reading) finish("Vehicle connection changed");
    if (mVesc) {
        disconnect(mVesc, nullptr, this, nullptr);
        disconnect(mVesc->commands(), nullptr, this, nullptr);
    }
    mVesc = vesc;
    if (mVesc) {
        connect(mVesc, &QObject::destroyed, this, [this]() {
            if (mOperation == Listing || mOperation == Reading) finish("Vehicle connection closed");
        });
        connect(mVesc, &VescInterface::portConnectedChanged, this, [this]() {
            if (!mVesc->isPortConnected() && (mOperation == Listing || mOperation == Reading)) {
                finish("Vehicle disconnected");
            }
        });
        connect(mVesc->commands(), &Commands::fileListRx, this, [this](bool more, QList<FILE_LIST_ENTRY> files) {
            if (mOperation != Listing || !checkRemote()) return;
            if (more && (files.isEmpty() || files.last().name == mRemoteFrom)) {
                finish("Invalid vehicle directory response");
                return;
            }
            for (const auto &file: files) {
                if (file.name.contains('/') || file.name == "." || file.name == "..") continue;
                if (file.isDir || file.name.endsWith(".csv", Qt::CaseInsensitive) ||
                    file.name.endsWith(".log", Qt::CaseInsensitive)) {
                    mPendingFiles.append(QVariantMap{{"name", file.name}, {"path", mRemotePath + "/" + file.name},
                                                     {"isDir", file.isDir}, {"size", file.size}});
                }
            }
            if (more) {
                mRemoteFrom = files.last().name;
                mRetries = 0;
                requestRemote();
            } else {
                std::sort(mPendingFiles.begin(), mPendingFiles.end(), [](const QVariant &a, const QVariant &b) {
                    return a.toMap().value("name").toString().toLower() < b.toMap().value("name").toString().toLower();
                });
                mFiles = mPendingFiles;
                mDirectory = mRemotePath.isEmpty() ? "/" : mRemotePath;
                emit filesChanged();
                setProgress(1.0);
                finish();
            }
        });
        connect(mVesc->commands(), &Commands::fileReadRx, this, [this](qint32 offset, qint32 size, QByteArray data) {
            if (mOperation != Reading || !checkRemote() || offset != mOffset) return;
            if (size <= 0 || size > maxFileSize || (mSize >= 0 && size != mSize) ||
                data.isEmpty() || qint64(offset) + data.size() > size) {
                finish("Invalid vehicle file response");
                return;
            }
            mSize = size;
            if (mDownload->write(data) != data.size()) {
                finish(mDownload->errorString());
                return;
            }
            mOffset += data.size();
            setProgress(double(mOffset) / mSize);
            if (!checkRemote()) return;
            if (mOffset == mSize) {
                mTimer.stop();
                mDownload->close();
                parse(mDownload->fileName(), QFileInfo(mRemotePath).fileName());
            } else {
                mRetries = 0;
                requestRemote();
            }
        });
    }
    emit vescChanged();
}

void RideLog::openLocal(QString path)
{
    if (busy()) return;
    path = localPath(path);
    parse(path, QFileInfo(path).fileName());
}

void RideLog::parse(QString path, QString name)
{
    mOperation = Parsing;
    setError(QString());
    setProgress(0.0);
    emit busyChanged();
    mCancel = std::make_shared<std::atomic_bool>(false);
    mParseProgress = std::make_shared<std::atomic_int>(0);
    auto cancel = mCancel;
    auto progress = mParseProgress;
    auto result = std::make_shared<std::shared_ptr<RideLogData>>();
    mWorker = QThread::create([path, cancel, progress, result]() {
        *result = readLog(path, cancel, progress);
    });
    mWorker->setParent(this);
    connect(mWorker, &QThread::finished, this, [this, name, cancel, result]() {
        mWorker = nullptr;
        const auto &data = *result;
        if (!cancel->load() && data->error.isEmpty()) {
            mData = data;
            mName = name;
            setProgress(1.0);
            emit logChanged();
        }
        finish(cancel->load() ? QString() : data->error);
    });
    connect(mWorker, &QThread::finished, mWorker, &QObject::deleteLater);
    mTimer.start(100);
    mWorker->start();
}

void RideLog::listLocal(QString path)
{
    if (busy()) return;
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/logs";
    if (path.isEmpty()) path = defaultPath;
    QDir dir(localPath(path));
    mFiles.clear();
    mDirectory = dir.absolutePath();
    if (!dir.exists()) {
        setError(mDirectory == QDir(defaultPath).absolutePath() ? QString() : "Log directory does not exist");
        emit filesChanged();
        return;
    }
    if (!dir.isReadable()) {
        setError("Cannot read log directory");
        emit filesChanged();
        return;
    }
    for (const auto &file: dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name)) {
        if (file.isDir() || file.suffix().compare("csv", Qt::CaseInsensitive) == 0 ||
            file.suffix().compare("log", Qt::CaseInsensitive) == 0) {
            mFiles.append(QVariantMap{{"name", file.fileName()}, {"path", file.absoluteFilePath()},
                                     {"isDir", file.isDir()}, {"size", file.size()}});
        }
    }
    setError(QString());
    emit filesChanged();
}

bool RideLog::beginRemote(Operation operation, QString path)
{
    if (busy()) return false;
    if (!mVesc || !mVesc->isPortConnected()) {
        setError("Vehicle is not connected");
        return false;
    }
    if (mRemoteDrain.isValid() && mRemoteDrain.elapsed() < 1500) {
        setError("Please wait a moment before the next vehicle file request");
        return false;
    }
    mRemotePath = QDir::cleanPath(path);
    if (mRemotePath == "." || mRemotePath == "/") mRemotePath.clear();
    mOperation = operation;
    mRemoteSendCan = mVesc->commands()->getSendCan();
    mRemoteCanId = mVesc->commands()->getCanSendId();
    mRetries = 0;
    mOffset = 0;
    mSize = -1;
    mRemoteFrom.clear();
    mPendingFiles.clear();
    setError(QString());
    setProgress(0.0);
    emit busyChanged();
    return true;
}

void RideLog::listRemote(QString path)
{
    if (busy()) return;
    mFiles.clear();
    mDirectory = QDir::cleanPath(path);
    if (mDirectory == ".") mDirectory = "/";
    emit filesChanged();
    if (beginRemote(Listing, path)) requestRemote();
}

void RideLog::openRemote(QString path)
{
    if (!beginRemote(Reading, path)) return;
    mDownload.reset(new QTemporaryFile());
    if (!mDownload->open()) {
        finish(mDownload->errorString());
        return;
    }
    requestRemote();
}

void RideLog::requestRemote()
{
    if (!checkRemote()) return;
    mTimer.start(1500);
    if (mOperation == Listing) mVesc->commands()->fileList(mRemotePath, mRemoteFrom);
    else if (mOperation == Reading) mVesc->commands()->fileRead(mRemotePath, mOffset);
}

bool RideLog::checkRemote()
{
    if (mOperation != Listing && mOperation != Reading) return false;
    if (!mVesc || !mVesc->isPortConnected()) {
        finish("Vehicle disconnected");
        return false;
    }
    if (mVesc->commands()->getSendCan() != mRemoteSendCan ||
        mVesc->commands()->getCanSendId() != mRemoteCanId) {
        finish("Selected vehicle changed during file transfer");
        return false;
    }
    return true;
}

void RideLog::cancel()
{
    if (mOperation == Parsing) mCancel->store(true);
    else if (busy()) {
        mRemoteDrain.start();
        finish();
    }
}

QVariantList RideLog::series(QVariantList fieldIndices, double from, double to, int pixels) const
{
    QVariantList result;
    if (!mData || !std::isfinite(from) || !std::isfinite(to) || to <= from) return result;
    pixels = qBound(1, pixels, 4096);
    const auto &times = mData->times;
    int first = std::lower_bound(times.begin(), times.end(), from) - times.begin();
    int end = std::upper_bound(times.begin(), times.end(), to) - times.begin();
    for (const auto &field: fieldIndices.mid(0, 4)) {
        int index = field.toInt();
        if (index < 0 || index >= mData->columns.size()) continue;
        const auto &column = mData->columns.at(index);
        QVariantList points;
        double minimum = std::numeric_limits<double>::infinity();
        double maximum = -std::numeric_limits<double>::infinity();
        int previousSource = -1;
        bool previousFinite = false;
        int begin = first;
        auto appendPoint = [&](double time, double value, int source) {
            bool finite = std::isfinite(value);
            if (finite && previousFinite && source > previousSource + 1 &&
                column.range(previousSource + 1, source).hasGap) {
                points.append(QPointF(time, std::numeric_limits<double>::quiet_NaN()));
            }
            QPointF point(time, value);
            if (points.isEmpty() || (finite && points.last().toPointF() != point) ||
                (!finite && previousFinite)) points.append(point);
            if (finite) {
                minimum = qMin(minimum, value);
                maximum = qMax(maximum, value);
            }
            previousSource = source;
            previousFinite = finite;
        };
        auto append = [&](int i) { appendPoint(times.at(i), column.values.at(i), i); };
        auto appendBoundary = [&](double time, int right, bool start) {
            if (right > 0 && times.at(right - 1) == time) {
                append(right - 1);
            } else if (right < times.size() && times.at(right) == time) {
                append(right);
            } else if (right > 0 && right < times.size()) {
                double a = column.values.at(right - 1);
                double b = column.values.at(right);
                double value = std::numeric_limits<double>::quiet_NaN();
                if (std::isfinite(a) && std::isfinite(b)) {
                    double fraction = (time - times.at(right - 1)) / (times.at(right) - times.at(right - 1));
                    value = a * (1.0 - fraction) + b * fraction;
                }
                appendPoint(time, value, start ? right - 1 : right);
            }
        };
        appendBoundary(from, first, true);
        for (int pixel = 0; pixel < pixels && begin < end; pixel++) {
            double boundary = from + (to - from) * (pixel + 1) / pixels;
            int next = pixel == pixels - 1 ? end : std::lower_bound(times.begin() + begin, times.begin() + end, boundary) - times.begin();
            auto e = column.range(begin, next);
            if (e.minimum >= 0) {
                append(qMin(e.minimum, e.maximum));
                if (e.minimum != e.maximum) append(qMax(e.minimum, e.maximum));
            } else if (next > begin) {
                append(begin);
            }
            begin = next;
        }
        appendBoundary(to, end, false);
        result.append(QVariantMap{{"field", index}, {"name", column.name}, {"unit", column.unit},
                                  {"points", points},
                                  {"minimum", std::isfinite(minimum) ? minimum : 0.0},
                                  {"maximum", std::isfinite(maximum) ? maximum : 0.0}});
    }
    return result;
}

QVariantMap RideLog::sample(double time) const
{
    if (!mData || mData->times.isEmpty() || !std::isfinite(time)) return QVariantMap();
    const auto &times = mData->times;
    int index = std::lower_bound(times.begin(), times.end(), time) - times.begin();
    if (index == times.size() || (index > 0 && time - times.at(index - 1) < times.at(index) - time)) index--;
    QVariantList values;
    for (const auto &column: mData->columns) values.append(column.values.at(index));
    QVariantMap result{{"time", times.at(index)}, {"values", values}, {"hasPosition", mData->position(index)}};
    if (mData->position(index)) {
        result.insert("latitude", mData->columns.at(mData->latitude).values.at(index));
        result.insert("longitude", mData->columns.at(mData->longitude).values.at(index));
    }
    return result;
}
