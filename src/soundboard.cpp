#include "soundboard.h"

#include <QDebug>
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
    // affinity of whoever created it, and QtMultimedia creates it in the
    // main thread. Touching it from here would enable its socket notifier
    // from the wrong thread, which crashes the game (QSocketNotifier::
    // setEnabled on a notifier that belongs to another thread: a
    // use-after-free, seen as a SIGSEGV in QSocketNotifier::setEnabled).
    // So the monitor is left alone.

    for (const SoundSpec &spec : kSounds) {
        auto *effect = new QSoundEffect(this);
        effect->setSource(QUrl(QStringLiteral("qrc:/sounds/%1").arg(QLatin1String(spec.file))));
        effect->setVolume(spec.volume);
        // Loading a source is asynchronous: play() on an effect that is
        // still loading is thrown away, and the sound is lost for good.
        // The QML type had time to load before the first sound; here the
        // first play can arrive right away, so the effect asks to be
        // played again once it is ready.
        const QString name = QLatin1String(spec.name);
        connect(effect, &QSoundEffect::statusChanged, this, [this, effect, name]() {
            if (effect->status() != QSoundEffect::Ready)
                return;
            if (m_pending.contains(name))
                effect->play();
        });
        m_effects.insert(name, effect);
    }
    // The two engine loops: the idle putter and the gallop.
    m_engine = new QSoundEffect(this);
    m_engine->setSource(QUrl(QStringLiteral("qrc:/sounds/engine.wav")));
    m_engine->setVolume(0.40f);
    m_engine->setLoopCount(QSoundEffect::Infinite);
    m_engine->setObjectName(QStringLiteral("engine"));

    m_engineMove = new QSoundEffect(this);
    m_engineMove->setSource(QUrl(QStringLiteral("qrc:/sounds/engine_move.wav")));
    m_engineMove->setVolume(0.0f);
    m_engineMove->setLoopCount(QSoundEffect::Infinite);
    m_engineMove->setObjectName(QStringLiteral("engine-move"));

    // The loops have the same loading story: they cannot be played before
    // their source is ready, and the first run begins right away.
    for (QSoundEffect *effect : {m_engine, m_engineMove}) {
        connect(effect, &QSoundEffect::statusChanged, this, [this, effect]() {
            if (effect->status() == QSoundEffect::Ready)
                applyEngine();
        });
    }
}

QSoundEffect *SoundWorker::effect(const QString &name) {
    return m_effects.value(name, nullptr);
}

void SoundWorker::setEngine(bool running, bool moving) {
    if (m_engineRunning == running && m_engineMoving == moving)
        return;
    m_engineRunning = running;
    m_engineMoving = moving;
    applyEngine();
}

void SoundWorker::setMuted(bool muted) {
    if (m_muted == muted)
        return;
    m_muted = muted;
    applyEngine();
}

void SoundWorker::applyEngine() {
    // The two loops stay alive for the whole session and the state travels
    // with the volume. A QSoundEffect in loop that is stopped does not come
    // back with play() (Qt 6.12), so stopping the motor would leave it
    // mute for good after the first run ends, or after the first M.
    if (m_engineRunning) {
        // A loop whose source is still loading cannot be played yet; the
        // status change calls this again when it is ready.
        if (m_engine->status() == QSoundEffect::Ready && !m_engine->isPlaying())
            m_engine->play();
        if (m_engineMove->status() == QSoundEffect::Ready && !m_engineMove->isPlaying())
            m_engineMove->play();
    }
    // The gallop is the loud one while the tank rolls; the putter takes
    // over when it stops. The mute is the volume down to nothing.
    const float level = m_muted ? 0.0f : 1.0f;
    m_engine->setVolume((m_engineMoving ? 0.0f : 0.40f) * level);
    m_engineMove->setVolume((m_engineMoving ? 0.50f : 0.0f) * level);
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
    if (effect->status() != QSoundEffect::Ready) {
        // The source is still loading: the play would be dropped. It is
        // asked for again when the effect becomes ready.
        m_pending.insert(name);
        return;
    }
    m_pending.remove(name);
    effect->play();
}

// ---- the board: it forwards the calls to the worker, on its thread.

SoundBoard::SoundBoard(QObject *parent) : QObject(parent) {
    m_worker = new SoundWorker;
    m_worker->moveToThread(&m_thread);
    m_thread.setObjectName(QStringLiteral("soundboard"));
    m_thread.start();
    // The calls to the worker travel as signals: a connection across
    // threads is queued on its own, and it cannot fail silently the way
    // an invokeMethod by name does.
    connect(this, &SoundBoard::engineChanged, m_worker, &SoundWorker::setEngine);
    connect(this, &SoundBoard::mutedChanged, m_worker, &SoundWorker::setMuted);
    connect(this, &SoundBoard::soundPlayed, m_worker, &SoundWorker::play);
    // The effects are created in the worker thread, where they live.
    if (!QMetaObject::invokeMethod(m_worker, "initialize", Qt::BlockingQueuedConnection))
        qWarning("SoundBoard: the sound thread could not be initialized");
}

SoundBoard::~SoundBoard() {
    // The effects are destroyed in the worker thread, which is where they
    // were created: a QSoundEffect must be destroyed in its own thread.
    QMetaObject::invokeMethod(m_worker, "deleteLater", Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

void SoundBoard::setEngine(bool running, bool moving) {
    emit engineChanged(running, moving);
}

void SoundBoard::setMuted(bool muted) {
    emit mutedChanged(muted);
}

void SoundBoard::play(const QString &name) {
    emit soundPlayed(name);
}
