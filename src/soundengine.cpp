#include "soundengine.h"

#include <SDL.h>

#include <QDebug>
#include <QFile>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

namespace {

// The same files and the same base volumes the QSoundEffect board used, so
// the mix does not change when the engine does.
struct ClipSpec {
    const char *name;
    const char *file;
    float volume;
};

const ClipSpec kOneShots[] = {
    {"shoot", "shoot.wav", 0.7f},
    {"hit", "hit.wav", 0.6f},
    {"explode", "explode.wav", 1.0f},
    {"power", "power.wav", 0.6f},
    {"levelup", "levelup.wav", 0.6f},
    {"nuke", "nuke.wav", 1.0f},
    {"air-support", "air-support.wav", 1.0f},
    {"blast_missile-shoot", "blast_missile-shoot.wav", 0.805f},
    {"blast_missile-hit", "blast_missile-hit.wav", 0.96f},
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

// The three long voices, in the order the engine keeps them.
const ClipSpec kLoops[] = {
    {"engine", "engine.wav", 1.0f},
    {"engine-move", "engine_move.wav", 1.0f},
    {"music", "stage-loop.wav", 1.0f},
};

// One voice per simultaneous one-shot. Eight was enough in the old board
// (shoot + hit + explode overlap); sixteen leaves room for the nuke.
constexpr int kVoices = 16;
// A chunk is queued every kTickMs; three chunks of look-ahead ride in the
// device. Small enough that a voice starts without a noticeable delay.
constexpr int kChunkFrames = 512;
constexpr int kTickMs = 10;
constexpr int kLookaheadChunks = 3;

} // namespace

struct SoundEngine::Impl {
    struct Clip {
        std::vector<Sint16> pcm; // mono, at the device rate
        float volume = 1.0f;
    };
    struct Voice {
        int clip = -1;
        size_t pos = 0;
        float volume = 0.0f;
        bool loop = false;
        bool active = false;
    };

    SDL_AudioDeviceID device = 0;
    SDL_AudioSpec spec{};
    std::vector<Clip> clips; // one-shots, then the three loops
    Voice voices[kVoices];
    Voice loops[3];
    float loopVolumes[3] = {0.0f, 0.0f, 0.0f};
    std::mutex mutex;
    bool muted = false;
    bool debug = false;
    bool inited = false;
    int nextVoice = 0;

    std::thread pump;
    std::atomic<bool> running{false};
    std::atomic<bool> reopenRequested{false};
    // A dead backend has to be torn down with SDL_QuitSubSystem, and SDL asks
    // for that on the main thread: the pump only raises the flag, the board
    // (GUI thread) does the work.
    std::atomic<bool> reinitRequested{false};
    std::recursive_mutex deviceMutex;

    void mix(Sint16 *out, int frames, int channels) {
        std::lock_guard<std::mutex> lock(mutex);
        const float master = muted ? 0.0f : 1.0f;
        for (int i = 0; i < frames; ++i) {
            float acc = 0.0f;
            for (Voice &v : voices) {
                if (!v.active || v.clip < 0)
                    continue;
                const std::vector<Sint16> &pcm = clips[size_t(v.clip)].pcm;
                if (pcm.empty())
                    continue;
                acc += pcm[v.pos] / 32768.0f * v.volume;
                if (++v.pos >= pcm.size()) {
                    if (v.loop)
                        v.pos = 0;
                    else
                        v.active = false;
                }
            }
            for (Voice &v : loops) {
                if (!v.active || v.clip < 0)
                    continue;
                const std::vector<Sint16> &pcm = clips[size_t(v.clip)].pcm;
                if (pcm.empty())
                    continue;
                acc += pcm[v.pos] / 32768.0f * v.volume;
                v.pos = (v.pos + 1) % pcm.size();
            }
            const float sample = std::max(-1.0f, std::min(1.0f, acc * master));
            const Sint16 s = Sint16(std::lround(sample * 32767.0f));
            for (int c = 0; c < channels; ++c)
                *out++ = s;
        }
    }

    void open();
    void close();
    void reinit();
    bool loadClips();
    void pumpLoop();
};

bool SoundEngine::Impl::loadClips() {
    // The device rate is what the PCM is resampled to once, at load: mixing
    // in the pump must not resample anything.
    const int rate = spec.freq;
    if (rate <= 0)
        return false;

    auto load = [&](const char *file, const char *name, float volume) -> Clip {
        Clip clip;
        clip.volume = volume;
        QFile resource(QStringLiteral(":/sounds/%1").arg(QLatin1String(file)));
        if (!resource.open(QIODevice::ReadOnly)) {
            qWarning("SoundEngine: %s is missing from the resources", name);
            return clip;
        }
        const QByteArray data = resource.readAll();
        SDL_RWops *rw = SDL_RWFromConstMem(data.constData(), int(data.size()));
        if (!rw)
            return clip;
        SDL_AudioSpec wav{};
        Uint8 *buffer = nullptr;
        Uint32 length = 0;
        if (!SDL_LoadWAV_RW(rw, 1, &wav, &buffer, &length) || wav.format != AUDIO_S16SYS
                || wav.channels != 1) {
            qWarning("SoundEngine: %s is not mono 16-bit PCM", name);
            if (buffer)
                SDL_FreeWAV(buffer);
            return clip;
        }
        const auto *samples = reinterpret_cast<const Sint16 *>(buffer);
        const int srcFrames = int(length / sizeof(Sint16));
        const auto srcRate = double(wav.freq);
        const auto dstRate = double(rate);
        const int dstFrames = int(std::lround(srcFrames * dstRate / srcRate));
        clip.pcm.resize(size_t(dstFrames));
        for (int i = 0; i < dstFrames; ++i) {
            const double at = double(i) * srcRate / dstRate;
            const auto index = size_t(at);
            const double frac = at - double(index);
            const Sint16 a = samples[std::min(index, size_t(srcFrames - 1))];
            const Sint16 b = samples[std::min(index + 1, size_t(srcFrames - 1))];
            clip.pcm[size_t(i)] = Sint16(std::lround(double(a) * (1.0 - frac) + double(b) * frac));
        }
        SDL_FreeWAV(buffer);
        return clip;
    };

    clips.clear();
    for (const ClipSpec &c : kOneShots)
        clips.push_back(load(c.file, c.name, c.volume));
    for (const ClipSpec &c : kLoops)
        clips.push_back(load(c.file, c.name, c.volume));
    return true;
}

void SoundEngine::Impl::open() {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex);
    close();
    SDL_AudioSpec want{};
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = kChunkFrames;
    // Queue mode (no callback): the pump thread mixes and queues, which is
    // also what lets it notice that the device stopped consuming audio.
    want.callback = nullptr;
    want.userdata = nullptr;

    device = SDL_OpenAudioDevice(nullptr, 0, &want, &spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!device) {
        qWarning("SoundEngine: no audio device: %s", SDL_GetError());
        return;
    }
    if (!loadClips()) {
        close();
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (Voice &v : voices)
            v = Voice{};
        for (int i = 0; i < 3; ++i) {
            Voice &v = loops[i];
            v = Voice{};
            v.clip = int(clips.size() - 3 + size_t(i));
            v.loop = true;
            v.active = true;
            v.pos = 0;
            v.volume = loopVolumes[i];
        }
    }
    SDL_PauseAudioDevice(device, 0);
    if (debug)
        fprintf(stderr, "[sound] device open at %d Hz, %d channel(s), driver %s\n",
                spec.freq, spec.channels, SDL_GetCurrentAudioDriver());
}

void SoundEngine::Impl::close() {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex);
    if (device) {
        SDL_CloseAudioDevice(device);
        device = 0;
    }
}

void SoundEngine::Impl::pumpLoop() {
    std::vector<Sint16> chunk(size_t(kChunkFrames) * size_t(2));
    int stuckMs = 0;
    int meterMs = 0;
    float peak = 0.0f;
    int closedMs = 0;
    Uint32 lastQueued = 0;
    while (running.load(std::memory_order_relaxed)) {
        // Only this thread touches the device once start() has returned.
        if (reinitRequested.load()) {
            // The board is tearing the backend down on the GUI thread.
            SDL_Delay(kTickMs);
            continue;
        }
        if (reopenRequested.exchange(false)) {
            open();
            stuckMs = 0;
            lastQueued = 0;
            continue;
        }
        if (!device) {
            // No device yet (a laptop with no sink plugged): keep trying.
            closedMs += kTickMs;
            if (closedMs >= 2000) {
                closedMs = 0;
                open();
            }
            SDL_Delay(kTickMs);
            continue;
        }
        Uint32 queued = 0;
        {
            std::lock_guard<std::recursive_mutex> lock(deviceMutex);
            queued = device ? SDL_GetQueuedAudioSize(device) : 0;
            const Uint32 maxQueued = Uint32(kChunkFrames * int(sizeof(Sint16))
                                            * spec.channels * kLookaheadChunks);
            if (device && queued < maxQueued) {
                chunk.resize(size_t(kChunkFrames) * size_t(spec.channels));
                mix(chunk.data(), kChunkFrames, spec.channels);
                if (debug) {
                    for (Sint16 s : chunk)
                        peak = std::max(peak, std::abs(float(s)) / 32768.0f);
                }
                SDL_QueueAudio(device, chunk.data(),
                               Uint32(chunk.size() * sizeof(Sint16)));
            }
        }
        // A device that stops consuming is gone: the queue does not drain.
        // That is the case this engine exists for (a suspend, a PipeWire
        // restart, headphones pulled), and Qt Multimedia never noticed it.
        if (device && queued == lastQueued && queued > 0) {
            stuckMs += kTickMs;
            if (stuckMs > 1000) {
                if (debug)
                    fprintf(stderr, "[sound] the device stopped consuming; rebuilding the backend\n");
                reinitRequested = true;
                stuckMs = 0;
                lastQueued = 0;
                continue;
            }
        } else {
            stuckMs = 0;
        }
        lastQueued = queued;
        if (debug) {
            meterMs += kTickMs;
            if (meterMs >= 1000) {
                meterMs = 0;
                fprintf(stderr, "[sound] peak %.3f\n", double(peak));
                peak = 0.0f;
            }
        }
        SDL_Delay(kTickMs);
    }
}

SoundEngine::SoundEngine() : d(new Impl) {
    d->debug = qEnvironmentVariableIsSet("OLIGARCHY_TANKS_SOUND_DEBUG");
}

SoundEngine::~SoundEngine() {
    shutdown();
    delete d;
}

void SoundEngine::start() {
    if (d->device)
        return;
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    if (!d->inited) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            qWarning("SoundEngine: SDL audio could not start: %s", SDL_GetError());
            return;
        }
        d->inited = true;
    }
    d->open();
    // The pump owns the device from here on: it mixes, queues, notices a
    // device that stopped consuming and retries when there is none.
    if (!d->running.exchange(true))
        d->pump = std::thread([this] { d->pumpLoop(); });
}

void SoundEngine::shutdown() {
    if (d->running.exchange(false) && d->pump.joinable())
        d->pump.join();
    d->close();
    if (d->inited) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        d->inited = false;
    }
}

void SoundEngine::reopen() {
    d->reopenRequested = true;
}

bool SoundEngine::reinitRequested() const {
    return d->reinitRequested.load();
}

void SoundEngine::reinitNow() {
    // SDL_QuitSubSystem / SDL_InitSubSystem: the main thread, as SDL asks.
    std::lock_guard<std::recursive_mutex> lock(d->deviceMutex);
    d->close();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    const bool ok = SDL_InitSubSystem(SDL_INIT_AUDIO) == 0;
    d->inited = ok;
    if (!ok)
        qWarning("SoundEngine: SDL audio could not restart: %s", SDL_GetError());
    else
        d->open();
    d->reinitRequested = false;
}

void SoundEngine::play(const QString &name) {
    if (!d->device)
        return;
    int clip = -1;
    for (size_t i = 0; i < std::size(kOneShots); ++i)
        if (name == QLatin1String(kOneShots[i].name))
            clip = int(i);
    if (clip < 0)
        return;
    std::lock_guard<std::mutex> lock(d->mutex);
    // A free voice, or the one that has been playing longest.
    int slot = -1;
    for (int i = 0; i < kVoices; ++i) {
        const int at = (d->nextVoice + i) % kVoices;
        if (!d->voices[at].active) {
            slot = at;
            break;
        }
    }
    if (slot < 0) {
        slot = d->nextVoice;
        d->nextVoice = (d->nextVoice + 1) % kVoices;
    }
    Impl::Voice &v = d->voices[slot];
    v.clip = clip;
    v.pos = 0;
    v.volume = d->clips[size_t(clip)].volume;
    v.loop = false;
    v.active = true;
    if (d->debug)
        fprintf(stderr, "[sound] play %s on voice %d\n", qPrintable(name), slot);
}

void SoundEngine::setLoopVolume(const QString &voice, float volume) {
    int index = -1;
    for (int i = 0; i < 3; ++i)
        if (voice == QLatin1String(kLoops[i].name))
            index = i;
    if (index < 0)
        return;
    const float clamped = std::max(0.0f, std::min(1.0f, volume));
    std::lock_guard<std::mutex> lock(d->mutex);
    d->loopVolumes[index] = clamped;
    d->loops[index].volume = clamped;
}

void SoundEngine::setMuted(bool muted) {
    std::lock_guard<std::mutex> lock(d->mutex);
    d->muted = muted;
}
