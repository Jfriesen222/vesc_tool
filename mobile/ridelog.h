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

#ifndef RIDELOG_H
#define RIDELOG_H

#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QTimer>
#include <QElapsedTimer>
#include <QTemporaryFile>
#include <QThread>
#include <atomic>
#include <memory>
#include "vescinterface.h"

struct RideLogData;

class RideLog : public QObject
{
    Q_OBJECT
    Q_PROPERTY(VescInterface *vesc READ vesc WRITE setVesc NOTIFY vescChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString name READ name NOTIFY logChanged)
    Q_PROPERTY(int sampleCount READ sampleCount NOTIFY logChanged)
    Q_PROPERTY(double duration READ duration NOTIFY logChanged)
    Q_PROPERTY(QString timeLabel READ timeLabel NOTIFY logChanged)
    Q_PROPERTY(QVariantList fields READ fields NOTIFY logChanged)
    Q_PROPERTY(QVariantList route READ route NOTIFY logChanged)
    Q_PROPERTY(QVariantList files READ files NOTIFY filesChanged)
    Q_PROPERTY(QString directory READ directory NOTIFY filesChanged)

public:
    explicit RideLog(QObject *parent = nullptr);
    ~RideLog();

    VescInterface *vesc() const;
    void setVesc(VescInterface *vesc);
    bool busy() const;
    double progress() const;
    QString error() const;
    QString name() const;
    int sampleCount() const;
    double duration() const;
    QString timeLabel() const;
    QVariantList fields() const;
    QVariantList route() const;
    QVariantList files() const;
    QString directory() const;

    Q_INVOKABLE void openLocal(QString path);
    Q_INVOKABLE void listLocal(QString path = QString());
    Q_INVOKABLE void listRemote(QString path = QString("log_can"));
    Q_INVOKABLE void openRemote(QString path);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE QVariantList series(QVariantList fieldIndices, double from, double to, int pixels) const;
    Q_INVOKABLE QVariantMap sample(double time) const;

signals:
    void vescChanged();
    void busyChanged();
    void progressChanged();
    void errorChanged();
    void logChanged();
    void filesChanged();

private:
    enum Operation { Idle, Parsing, Listing, Reading };
    QPointer<VescInterface> mVesc;
    std::shared_ptr<RideLogData> mData;
    std::shared_ptr<std::atomic_bool> mCancel;
    std::shared_ptr<std::atomic_int> mParseProgress;
    QThread *mWorker;
    QTimer mTimer;
    QElapsedTimer mRemoteDrain;
    std::unique_ptr<QTemporaryFile> mDownload;
    Operation mOperation;
    QString mError;
    QString mName;
    QString mDirectory;
    QString mRemotePath;
    QString mRemoteFrom;
    QVariantList mFiles;
    QVariantList mPendingFiles;
    double mProgress;
    int mRetries;
    qint32 mOffset;
    qint32 mSize;
    bool mRemoteSendCan;
    int mRemoteCanId;

    void setError(QString error);
    void setProgress(double progress);
    void finish(QString error = QString());
    bool beginRemote(Operation operation, QString path);
    bool checkRemote();
    void requestRemote();
    void parse(QString path, QString name);
};

#endif // RIDELOG_H
