#include "soundboard.h"

#include <cstdio>
#include <QDateTime>
#include <QDebug>
#include <QMediaDevices>
#include <QSoundEffect>
#include <QThread>
#include <QTimer>
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
    // The endings and the UI: the jingles sound when the screens take
    // over, the ticks ride the victory count-up, the menu pair drives
    // the select screen's feedback. Quieter than the fight sounds:
    // they must talk over nothing. The game over sits 40% below its
    // first take — it was the loudest thing in the room (user report).
    {"gameover", "gameover.wav", 0.43f},
    {"victory", "victory.wav", 0.65f},
    {"stage-start", "stage-start.wav", 0.7f},
    {"score-tick-1", "score-tick-1.wav", 0.4f},
    {"score-tick-2", "score-tick-2.wav", 0.4f},
    {"score-tick-3", "score-tick-3.wav", 0.4f},
    {"score-tick-4", "score-tick-4.wav", 0.4f},
    {"menu-move", "menu-move.wav", 0.3f},
    {"menu-confirm", "menu-confirm.wav", 0.38f},
};

} // namespace

void engineVolumes(bool running, bool moving, bool muted, float *putter, float *gallop) {
    // No run, no sound: the motor is silent on the select screen, on the
    // game over screen and while the next sector loads.
    if (!running || muted) {
        *putter = 0.0f;
        *gallop = 0.0f;
        return;
    }
    // The gallop is the loud one while the tank rolls; the putter takes
    // over when it stops.
    *putter = moving ? 0.0f : 0.40f;
    *gallop = moving ? 0.50f : 0.0f;
}

float musicVolume(bool inStage, bool muted) {
    // 0.6 by the user's final ear: the loop leads the mix over the motor's
    // 0.40, with the bass carrying it at 125 bpm.
    return (inStage && !muted) ? 0.6f : 0.0f;
}

// ---- the worker: it owns the QSoundEffects and lives in its own thread.

SoundWorker::SoundWorker(QObject *parent) : QObject(parent) {
    // OLIGARCHY_TANKS_SOUND_DEBUG=1 traces what reaches the audio thread:
    // the field report of "everything went silent but the loop" needs to know
    // which side gave up.
    m_debug = qEnvironmentVariableIsSet("OLIGARCHY_TANKS_SOUND_DEBUG");
}

SoundWorker::~SoundWorker() = default;

void SoundWorker::initialize() {
    createEffects();

    // The device watcher belongs to this thread: creating it in the GUI (or
    // letting the QML import create it there) is precisely what produces
    // "Socket notifiers cannot be enabled or disabled from another thread"
    // followed by the invalid-socket spin.
    m_devices = new QMediaDevices(this);
    connect(m_devices, &QMediaDevices::audioOutputsChanged,
            this, &SoundWorker::devicesChanged);
}

void SoundWorker::createEffects() {
    // The audio device monitor is a process-wide object with the thread
    // affinity of whoever created it first. Everything QtMultimedia now
    // happens in this thread — the QML import that used to create it in the
    // GUI is gone, and the watcher is built in initialize() — because using
    // it from two threads is what crashes with "QSocketNotifier: setEnabled
    // on a notifier that belongs to another thread" (seen in a core dump).

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
            if (effect->status() == QSoundEffect::Error) {
                // The device went away under it: ask for a rebuild.
                emit audioFailed();
                return;
            }
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

    m_engineMove = new QSoundEffect(this);
    m_engineMove->setSource(QUrl(QStringLiteral("qrc:/sounds/engine_move.wav")));
    m_engineMove->setVolume(0.0f);
    m_engineMove->setLoopCount(QSoundEffect::Infinite);

    // The stage loop: a third endless effect, kept alive the same way as
    // the motor and carried by its volume.
    m_music = new QSoundEffect(this);
    m_music->setSource(QUrl(QStringLiteral("qrc:/sounds/stage-loop.wav")));
    m_music->setVolume(0.0f);
    m_music->setLoopCount(QSoundEffect::Infinite);

    // The loops have the same loading story: they cannot be played before
    // their source is ready, and the first run begins right away.
    for (QSoundEffect *effect : {m_engine, m_engineMove}) {
        connect(effect, &QSoundEffect::statusChanged, this, [this, effect]() {
            if (effect->status() == QSoundEffect::Ready)
                applyEngine();
        });
    }
    connect(m_music, &QSoundEffect::statusChanged, this, [this]() {
        if (m_music->status() == QSoundEffect::Ready)
            applyMusic();
    });
    for (QSoundEffect *effect : {m_engine, m_engineMove, m_music}) {
        connect(effect, &QSoundEffect::statusChanged, this, [this, effect]() {
            if (effect->status() == QSoundEffect::Error)
                emit audioFailed();
        });
    }

}

QSoundEffect *SoundWorker::effect(const QString &name) {
    return m_effects.value(name, nullptr);
}

void SoundWorker::rebuild() {
    // Every effect is destroyed and recreated here, in the thread that owns
    // them. The device monitor is left alone: it is global and belongs to
    // this thread, and abandoning the thread crashes Qt in
    // QSocketNotifier::setEnabled (a core dump proved it).
    if (m_debug)
        fprintf(stderr, "[sound] rebuilding the effects in place\n");
    qDeleteAll(m_effects);
    m_effects.clear();
    m_pending.clear();
    delete m_engine;
    delete m_engineMove;
    delete m_music;
    m_engine = m_engineMove = m_music = nullptr;
    createEffects();
    applyEngine();
    applyMusic();
}

void SoundWorker::setEngine(bool running, bool moving, bool muted) {
    m_engineRunning = running;
    m_engineMoving = running && moving;
    m_muted = muted;
    applyEngine();
}

void SoundWorker::devicesChanged() {
    if (m_debug)
        fprintf(stderr, "[sound] audio devices changed\n");
    // Every QtMultimedia object lives in this thread, so the watcher fires
    // here and the long loops are re-armed from here.
    applyEngine();
    applyMusic();
}

void SoundWorker::setMuted(bool muted) {
    m_muted = muted;
    applyEngine();
    applyMusic();
}

void SoundWorker::setMusic(bool on) {
    m_musicOn = on;
    applyMusic();
}

void SoundWorker::applyMusic() {
    // Same rule as the motor: an infinite QSoundEffect that is stopped does
    // not come back with play() in Qt 6.12, so the loop is never stopped —
    // the volume carries the state. It is also what keeps the loop in phase
    // when a pause or an ending interrupts it.
    if (m_music->status() == QSoundEffect::Ready && !m_music->isPlaying())
        m_music->play();
    m_music->setVolume(musicVolume(m_musicOn, m_muted));
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
    float putter = 0.0f;
    float gallop = 0.0f;
    engineVolumes(m_engineRunning, m_engineMoving, m_muted, &putter, &gallop);
    m_engine->setVolume(putter);
    m_engineMove->setVolume(gallop);
}

void SoundWorker::ping(int token) {
    // A sink that went away (a device change, a laptop suspending the output)
    // can leave a long loop stopped while the state says it should be
    // playing, and nothing reports an error. The heartbeat is a free place to
    // nudge them back from the thread that owns them.
    if (!m_muted) {
        if (m_engineRunning) {
            if (m_engine->status() == QSoundEffect::Ready && !m_engine->isPlaying())
                m_engine->play();
            if (m_engineMove->status() == QSoundEffect::Ready && !m_engineMove->isPlaying())
                m_engineMove->play();
        }
        if (m_musicOn && m_music->status() == QSoundEffect::Ready && !m_music->isPlaying())
            m_music->play();
    }
    if (m_debug)
        fprintf(stderr, "[sound] ping %d\n", token);
    emit pong(token);
}

void SoundWorker::play(const QString &name) {
    if (m_debug)
        fprintf(stderr, "[sound] play %s (muted %d)\n", qPrintable(name), int(m_muted));
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
    startWorker();

    // The heartbeat: if the worker stops answering, its event loop is wedged
    // in the audio backend (the Qt 6.12 failures this file documents) and a
    // rebuild is the only way back.
    m_watchdog = new QTimer(this);
    m_watchdog->setInterval(1000);
    connect(m_watchdog, &QTimer::timeout, this, [this]() {
        if (m_pingPending) {
            if (QDateTime::currentMSecsSinceEpoch() - m_pingAt > 2500) {
                // A thread that does not answer is stuck inside the audio
                // backend. Killing it leaves Qt's global audio state broken,
                // so sound stops here instead of crashing later.
                qWarning("SoundBoard: the audio thread stopped answering; silent");
                if (qEnvironmentVariableIsSet("OLIGARCHY_TANKS_SOUND_DEBUG"))
                    fprintf(stderr, "[sound] watchdog: the thread stopped answering\n");
                m_soundDead = true;
                stopWorker();
                if (m_watchdog)
                    m_watchdog->stop();
                if (m_troublePoll)
                    m_troublePoll->stop();
            }
            return;
        }
        m_pingPending = true;
        m_pingAt = QDateTime::currentMSecsSinceEpoch();
        emit pingSent(++m_pingToken);
    });
    m_watchdog->start();

    // A rebuild is never immediate: device changes arrive in bursts and a
    // failing effect can report several times.
    m_restart = new QTimer(this);
    m_restart->setSingleShot(true);
    m_restart->setInterval(400);
    connect(m_restart, &QTimer::timeout, this, [this]() {
        if (m_soundDead)
            return;
        if (m_rebuilds >= 3) {
            // Three rebuilds did not fix it: the device backend is gone for
            // this process. Go quiet and keep the game playable.
            m_soundDead = true;
            qWarning("SoundBoard: the audio keeps failing; silent until restart");
            if (qEnvironmentVariableIsSet("OLIGARCHY_TANKS_SOUND_DEBUG"))
                fprintf(stderr, "[sound] giving up for this session\n");
            return;
        }
        ++m_rebuilds;
        qWarning("SoundBoard: rebuilding the audio");
        if (qEnvironmentVariableIsSet("OLIGARCHY_TANKS_SOUND_DEBUG"))
            fprintf(stderr, "[sound] rebuild requested\n");
        emit rebuildRequested();
    });

    // Qt prints "QSocketNotifier: Invalid socket ..." by the thousand when
    // the audio device's fd goes bad: the event loop spins (the fans the
    // user heard) and every sound dies while a loop keeps playing. The
    // message handler counts them; here they turn into an in-place rebuild.
    m_troublePoll = new QTimer(this);
    m_troublePoll->setInterval(500);
    connect(m_troublePoll, &QTimer::timeout, this, [this]() {
        const int strikes = m_audioTrouble.exchange(0);
        if (strikes >= 20 && !m_soundDead) {
            qWarning("SoundBoard: the audio device socket went bad; rebuilding");
            scheduleRestart();
        }
    });
    m_troublePoll->start();
}

SoundBoard::~SoundBoard() {
    if (m_watchdog)
        m_watchdog->stop();
    if (m_restart)
        m_restart->stop();
    stopWorker();
}

void SoundBoard::startWorker() {
    m_thread = new QThread;
    m_worker = new SoundWorker;
    m_worker->moveToThread(m_thread);
    m_thread->setObjectName(QStringLiteral("soundboard"));
    // The worker is destroyed by its own thread when the loop ends: a
    // QSoundEffect must be destroyed in the thread that owns it.
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    // The calls to the worker travel as signals: a connection across
    // threads is queued on its own, and it cannot fail silently the way
    // an invokeMethod by name does.
    connect(this, &SoundBoard::engineChanged, m_worker, &SoundWorker::setEngine);
    connect(this, &SoundBoard::mutedChanged, m_worker, &SoundWorker::setMuted);
    connect(this, &SoundBoard::musicChanged, m_worker, &SoundWorker::setMusic);
    connect(this, &SoundBoard::soundPlayed, m_worker, &SoundWorker::play);
    connect(this, &SoundBoard::pingSent, m_worker, &SoundWorker::ping);
    connect(this, &SoundBoard::rebuildRequested, m_worker, &SoundWorker::rebuild);
    connect(m_worker, &SoundWorker::pong, this, &SoundBoard::onPong);
    connect(m_worker, &SoundWorker::audioFailed, this, &SoundBoard::scheduleRestart);
    m_thread->start();
    m_pingPending = false;
    // The effects are created in the worker thread, where they live.
    if (!QMetaObject::invokeMethod(m_worker, "initialize", Qt::BlockingQueuedConnection))
        qWarning("SoundBoard: the sound thread could not be initialized");
    // Fresh effects know nothing: hand them the state the game is in.
    emit engineChanged(m_engineRunning, m_engineMoving, m_muted);
    emit mutedChanged(m_muted);
    emit musicChanged(m_musicOn);
}

void SoundBoard::stopWorker() {
    if (!m_thread)
        return;
    m_thread->quit();
    if (!m_thread->wait(1200)) {
        // A thread stuck inside the audio backend does not come back on its
        // own, and the game must still quit (the user had to pkill it).
        qWarning("SoundBoard: the audio thread did not quit; terminating it");
        m_thread->terminate();
        m_thread->wait(600);
    }
    if (m_thread->isFinished()) {
        delete m_thread;
    } else {
        // Refused to die: abandon it. The process exit reclaims it, and a
        // leaked thread is better than a game that cannot close.
        m_thread->setParent(nullptr);
    }
    m_thread = nullptr;
    m_worker = nullptr;
    m_pingPending = false;
}

void SoundBoard::scheduleRestart() {
    if (m_restart && !m_restart->isActive())
        m_restart->start();
}

void SoundBoard::onPong(int token) {
    if (token == m_pingToken)
        m_pingPending = false;
}

void SoundBoard::setEngine(bool running, bool moving) {
    m_engineRunning = running;
    // No run, no rolling: the tank is not moving on a menu.
    m_engineMoving = running && moving;
    emit engineChanged(m_engineRunning, m_engineMoving, m_muted);
}

void SoundBoard::setMuted(bool muted) {
    m_muted = muted;
    emit mutedChanged(m_muted);
    // The engine volumes follow the mute too.
    emit engineChanged(m_engineRunning, m_engineMoving, m_muted);
}

void SoundBoard::setMusic(bool on) {
    m_musicOn = on;
    emit musicChanged(on);
}

void SoundBoard::play(const QString &name) {
    emit soundPlayed(name);
}

double SoundBoard::putterVolume() const {
    float putter = 0.0f;
    float gallop = 0.0f;
    engineVolumes(m_engineRunning, m_engineMoving, m_muted, &putter, &gallop);
    return putter;
}

double SoundBoard::gallopVolume() const {
    float putter = 0.0f;
    float gallop = 0.0f;
    engineVolumes(m_engineRunning, m_engineMoving, m_muted, &putter, &gallop);
    return gallop;
}

double SoundBoard::musicVolume() const {
    return ::musicVolume(m_musicOn, m_muted);
}
