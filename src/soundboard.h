// The game's sounds, on a thread of their own.
//
// Why a thread: Qt's QSoundEffect owns a QAutoResetEvent, which on Linux
// is an eventfd with a QSocketNotifier registered on it. When the audio
// device dies, two things can go wrong in Qt 6.12.0, both of them in the
// thread that owns the effect:
//
//   - the sink's stream is torn down and the engine waits on a semaphore
//     with no timeout (QTBUG-150678), so the thread that destroys the
//     effect waits for ever;
//   - the eventfd is closed while its notifier is still registered
//     (QTBUG-144711), so the event dispatcher spins on an invalid fd and
//     prints "QSocketNotifier: Invalid socket NN and type 'Read'".
//
// Both happen in the GUI thread when the QML type SoundEffect is used,
// and that is the freeze the user reported: the window stops responding
// while the sound that was already buffered keeps playing.
//
// Here the effects live in a worker thread, so a wedged audio backend
// can only wedge that thread: the game keeps running (and stays quiet).
// The QML type cannot be moved between threads, hence the C++ one.
//
// The thread is created when the board is constructed and joined when it
// is destroyed.

#ifndef SOUNDBOARD_H
#define SOUNDBOARD_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QThread>

class QSoundEffect;

// The worker: it owns the QSoundEffects and lives in its own thread.
class SoundWorker : public QObject {
    Q_OBJECT

public:
    explicit SoundWorker(QObject *parent = nullptr);
    ~SoundWorker() override;

public slots:
    // The QSoundEffects are created here, in the worker thread: an
    // effect must be created and destroyed in the thread that owns it.
    void initialize();
    void setEngine(bool running, bool moving);
    void setMuted(bool muted);
    void play(const QString &name);

private:
    QSoundEffect *effect(const QString &name);
    QHash<QString, QSoundEffect *> m_effects;
    QSoundEffect *m_engine = nullptr;
    QSoundEffect *m_engineMove = nullptr;
    bool m_engineRunning = false;
    bool m_engineMoving = false;
    bool m_muted = false;
};

// The board: the game's front door to the sounds. It forwards every call
// to the worker, which lives in its own thread.
class SoundBoard : public QObject {
    Q_OBJECT

public:
    explicit SoundBoard(QObject *parent = nullptr);
    ~SoundBoard() override;

    // The engine loops: one for the idle putter, one for the gallop
    // while a tank rolls. They are separate sounds that cross-fade, and
    // they are the only ones that play for a long time.
    //
    // The state is what the QML decides, not what the board polls: the
    // board never asks a sound whether it is playing, because reading
    // that property is a call into the audio backend, and that is what
    // used to touch the audio from the GUI thread on every tick.
    void setEngine(bool running, bool moving);
    void setMuted(bool muted);

    // One-shot sounds: "shoot", "hit", "explode", "power", "levelup",
    // "nuke", "air-support", "blast_missile-shoot", "blast_missile-hit".
    void play(const QString &name);

private:
    QThread m_thread;
    SoundWorker *m_worker = nullptr;
};

#endif // SOUNDBOARD_H
