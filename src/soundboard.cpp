#include "soundboard.h"


float engineVolumesFor(bool running, bool moving, bool muted, bool gallop) {
    // No run, no sound: the motor is silent on the select screen, on the
    // game over screen and while the next sector loads.
    if (!running || muted)
        return 0.0f;
    // The gallop is the loud one while the tank rolls; the putter takes
    // over when it stops.
    if (gallop)
        return moving ? 0.50f : 0.0f;
    return moving ? 0.0f : 0.40f;
}

float stageMusicVolume(bool inStage, bool muted) {
    // 0.6 by the user's final ear: the loop leads the mix over the motor's
    // 0.40, with the bass carrying it at 125 bpm.
    return (inStage && !muted) ? 0.6f : 0.0f;
}

SoundBoard::SoundBoard(QObject *parent) : QObject(parent) {
    m_engine.start();

    apply();
}

SoundBoard::~SoundBoard() {
    m_engine.shutdown();
}

void SoundBoard::setEngine(bool running, bool moving) {
    m_engineRunning = running;
    m_engineMoving = moving;
    emit engineChanged(running, moving, m_muted);
    apply();
}

void SoundBoard::setMuted(bool muted) {
    m_muted = muted;
    m_engine.setMuted(muted);
    emit mutedChanged(muted);
    // The engine volumes follow the mute too.
    emit engineChanged(m_engineRunning, m_engineMoving, m_muted);
    apply();
}

void SoundBoard::setMusic(bool on) {
    m_musicOn = on;
    emit musicChanged(on);
    apply();
}

void SoundBoard::reopenAudio() {
    m_engine.reopen();
    apply();
}

void SoundBoard::play(const QString &name) {
    emit soundPlayed(name);
    m_engine.play(name);
}

void SoundBoard::apply() {
    m_engine.setLoopVolume(QStringLiteral("engine"),
                           engineVolumesFor(m_engineRunning, m_engineMoving, m_muted, false));
    m_engine.setLoopVolume(QStringLiteral("engine-move"),
                           engineVolumesFor(m_engineRunning, m_engineMoving, m_muted, true));
    m_engine.setLoopVolume(QStringLiteral("music"), stageMusicVolume(m_musicOn, m_muted));
}

double SoundBoard::putterVolume() const {
    return engineVolumesFor(m_engineRunning, m_engineMoving, m_muted, false);
}

double SoundBoard::gallopVolume() const {
    return engineVolumesFor(m_engineRunning, m_engineMoving, m_muted, true);
}

double SoundBoard::musicVolume() const {
    return stageMusicVolume(m_musicOn, m_muted);
}
