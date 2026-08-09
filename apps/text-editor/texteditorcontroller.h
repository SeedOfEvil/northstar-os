#pragma once

#include <QObject>
#include <QString>

class TextEditorController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString filePath READ filePath NOTIFY stateChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY stateChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY stateChanged)
    Q_PROPERTY(bool canSave READ canSave NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)

public:
    explicit TextEditorController(QObject *parent = nullptr);

    QString filePath() const;
    QString text() const;
    bool dirty() const;
    bool canSave() const;
    QString statusMessage() const;

    Q_INVOKABLE bool loadFile(const QString &path);
    Q_INVOKABLE bool save();

public slots:
    void setText(const QString &text);

signals:
    void stateChanged();

private:
    QString m_filePath;
    QString m_text;
    QString m_savedText;
    QString m_statusMessage;
};
