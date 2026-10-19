#pragma once

#include <QObject>

// The game's accent is the Omarchy logo green, fixed by the user (their
// desktop theme's blue accent must never leak into the UI; the same decision
// as the base tile's plate). accentForeground() answers black or white,
// whichever reads on it.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)

public:
    explicit Theme(QObject *parent = nullptr);

    QString accent() const { return m_accent; }
    QString accentForeground() const;

signals:
    void changed();

private:
    QString m_accent = QStringLiteral("#9ece6a");
};
