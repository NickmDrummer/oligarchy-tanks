#include "soundboard.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QSoundEffect>
#include <QThread>
#include <QUrl>

namespace {

// Every sound the game plays, with the volume the QML used. The ceiling
// of a QSoundEffect is 1.0, so the louder ones sit at it.
struct SoundSpec {
    const char *name;
    const char *file;
    float volume;
};

const SoundSpec kSounds[] = {
    {"shoot", "shoot.wav", 0.7f},
    {"hit", "hit.wav", 0.6f},
    {"explode", "explode.wav", 1.0f},
    {"power", "power.wav", 0.6f},
    {"levelup", "levelup.wav", 0.6f},
    {"nuke", "nuke.wav", 1.0f},
    {"air-support", "air-support.wav", 1.0f},
    {"blast_missile-shoot", "blast_missile-shoot.wav", 0.805f},
    {"blast_missile-hit", "blast_missile-hit.wav", 0.96f},
};

} // namespace

// ---- the worker: it owns the QSoundEffects and lives in its own thread.

SoundWorker::SoundWorker(QObject *parent) : QObject(parent) {}

SoundWorker::~SoundWorker() = default;

void SoundWorker::initialize() {
    // The audio device monitor is a process-wide object with the thread
    // affinity of whoever created it. If it is born in the main thread and
    // the effects live in this one, Qt enables its socket notifier from
    // the wrong thread, and the game aborts when the thread is destroyed.
    // Touching it HERE, first, gives it this thread's affinity.
    QMediaDevices::audioOutputs();

    for (const SoundSpec &spec : kSounds) {
        auto *effect = new QSoundEffect(this);
        effect->setSource(QUrl(QStringLiteral("qrc:/sounds/%1").arg(QLatin1String(spec.file))));
        effect->setVolume(spec.volume);
        m_effects.insert(QLatin1String(spec.name), effect);
    }
    // The two engine loops: the idle putter and the gallop.
    m_engine = new QSoundEffect(this);
    m_engine->setSource(QUrl(QStringLiteral("qrc:/sounds/engine.wav")));
    m_engine->setVolume(0.40f);
    m_engine->setLoopCount(QSoundEffect::Infinite);

    m_engineMove = new QSoundEffect(this);
    m_engineMove->setSource(QUrl(QStringLiteral("qrc:/sounds/engine_move.wav")));
    m_engineMove->setVolume(0.0f);
    m_engineMove->setLoopCount(QSoundEffect::Infinite);
}

QSoundEffect *SoundWorker::effect(const QString &name) {
    return m_effects.value(name, nullptr);
}

void SoundWorker::setEngine(bool running, bool moving) {
    if (m_engineRunning == running && m_engineMoving == moving)
        return;
    m_engineRunning = running;
    m_engineMoving = moving;

    if (!running) {
        // The putter and the gallop are the only loops; they stop with
        // the run.
        if (m_engine->isPlaying())
            m_engine->stop();
        if (m_engineMove->isPlaying())
            m_engineMove->stop();
        return;
    }
    if (!m_engine->isPlaying())
        m_engine->play();
    if (!m_engineMove->isPlaying())
        m_engineMove->play();
    // The gallop is the loud one while the tank rolls; the putter takes
    // over when it stops. The volumes only change when they change.
    m_engine->setVolume(moving ? 0.0f : 0.40f);
    m_engineMove->setVolume(moving ? 0.50f : 0.0f);
}

void SoundWorker::setMuted(bool muted) {
    if (m_muted == muted)
        return;
    m_muted = muted;
    // M silences the engine loops too: until the freeze round the mute
    // only stopped the event sounds and the putter kept going.
    if (muted) {
        if (m_engine->isPlaying())
            m_engine->stop();
        if (m_engineMove->isPlaying())
            m_engineMove->stop();
    } else if (m_engineRunning) {
        if (!m_engine->isPlaying())
            m_engine->play();
        if (!m_engineMove->isPlaying())
            m_engineMove->play();
    }
}

void SoundWorker::play(const QString &name) {
    if (m_muted)
        return;
    QSoundEffect *effect = this->effect(name);
    if (!effect)
        return;
    // A sound that is still playing is restarted, which is what the QML
    // did: the air-support sample is 2 s and fires every 1.5 s, so the
    // plays overlap.
    effect->play();
}

// ---- the board: it forwards the calls to the worker, on its thread.

SoundBoard::SoundBoard(QObject *parent) : QObject(parent) {
    m_worker = new SoundWorker;
    m_worker->moveToThread(&m_thread);
    m_thread.setObjectName(QStringLiteral("soundboard"));
    m_thread.start();
    // The effects are created in the worker thread, where they live.
    QMetaObject::invokeMethod(m_worker, "initialize", Qt::BlockingQueuedConnection);
}

SoundBoard::~SoundBoard() {
    // The effects are destroyed in the worker thread, which is where they
    // were created: a QSoundEffect must be destroyed in its own thread.
    QMetaObject::invokeMethod(m_worker, "deleteLater", Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

void SoundBoard::setEngine(bool running, bool moving) {
    QMetaObject::invokeMethod(m_worker, "setEngine", Qt::QueuedConnection,
                              Q_ARG(bool, running), Q_ARG(bool, moving));
}

void SoundBoard::setMuted(bool muted) {
    QMetaObject::invokeMethod(m_worker, "setMuted", Qt::QueuedConnection, Q_ARG(bool, muted));
}

void SoundBoard::play(const QString &name) {
    QMetaObject::invokeMethod(m_worker, "play", Qt::QueuedConnection, Q_ARG(QString, name));
}
