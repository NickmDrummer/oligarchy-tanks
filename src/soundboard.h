#pragma once

#include <QObject>

#include "soundengine.h"

class QTimer;

// The volumes of the two engine loops, given the state of the game.
//
// The putter is the idle rumble, the gallop is the faster one while a tank
// rolls, and they cross-fade. Both are silent when there is no run going on
// and when the sound is muted.
float engineVolumesFor(bool running, bool moving, bool muted, bool gallop);

// The stage music: it runs through the whole sector under the motor and is
// silenced by the same mute switch (and by the end beat, which the view
// reports as "not in a stage").
float stageMusicVolume(bool inStage, bool muted);

// The board: the game's front door to the sounds.
//
// It used to be a worker thread with QSoundEffects, because Qt Multimedia
// cannot be touched from the GUI thread and its device monitor does not
// survive a device change (AGENTS.md 21.29). The audio is SDL2 now: this
// class keeps the game's rules — which loop is on, the volumes, the mute —
// and the engine does the mixing. There is no worker thread left to wedge,
// so the board is a thin, synchronous layer again.
//
// The QML API is unchanged on purpose: Main.qml calls setEngine, setMuted,
// setMusic and play, and reads the three volumes in the tests.
class SoundBoard : public QObject {
    Q_OBJECT

public:
    explicit SoundBoard(QObject *parent = nullptr);
    ~SoundBoard() override;

    Q_INVOKABLE void setEngine(bool running, bool moving);
    Q_INVOKABLE void setMuted(bool muted);
    // The stage loop: on while a sector is being played (never while the
    // end beat runs, the pause holds or a menu is up).
    Q_INVOKABLE void setMusic(bool on);

    // One-shot sounds, by name.
    Q_INVOKABLE void play(const QString &name);

    // The device went away or came back (SDL's audio events): reopen it and
    // hand the loops their volumes again. Called from the GUI thread, where
    // SDL's event queue is pumped.
    void reopenAudio();

    // The volumes the loops are playing at right now. The tests use them,
    // and they are the honest answer to "is the motor supposed to be
    // sounding at this moment?".
    Q_INVOKABLE double putterVolume() const;
    Q_INVOKABLE double gallopVolume() const;
    Q_INVOKABLE double musicVolume() const;

signals:
    // Kept as the board's own record of what it was told: the tests spy on
    // them, and they are what makes a mute that never reached the sound
    // visible.
    void engineChanged(bool running, bool moving, bool muted);
    void mutedChanged(bool muted);
    void musicChanged(bool on);
    void soundPlayed(const QString &name);

private:
    void apply();

    SoundEngine m_engine;
    QTimer *m_health = nullptr;
    // The state of the game, kept here so the volumes can be reported
    // without asking the engine.
    bool m_engineRunning = false;
    bool m_engineMoving = false;
    bool m_musicOn = false;
    bool m_muted = false;
};
