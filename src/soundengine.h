#pragma once

#include <QString>

// The game's audio, on SDL2.
//
// It replaces Qt Multimedia, whose audio device monitor does not survive a
// device change (see AGENTS.md 21.29): the monitor is a process-wide object
// tied to the thread that created it and Qt never rebuilds it, so every sound
// died for the rest of the session. SDL owns the device here, the mixing runs
// in its callback, and the engine reopens the device by itself.
//
// The interface is deliberately small and QML-free: the SoundBoard keeps the
// game's rules (volumes, mute, which loop is on) and this class only makes
// sound. The three long voices (engine, engine-move, music) are always
// running; their volume carries the state, exactly as the QSoundEffect loops
// did (an effect that is stopped does not come back).
class SoundEngine {
public:
    SoundEngine();
    ~SoundEngine();

    // Opens the device and loads every clip from the binary's resources.
    // Safe to call once; a second call is ignored.
    void start();
    void shutdown();

    // A one-shot by name: "shoot", "hit", "explode", "power", "levelup",
    // "nuke", "air-support", "blast_missile-shoot", "blast_missile-hit",
    // "gameover", "victory", "stage-start", "score-tick-1..4", "menu-move",
    // "menu-confirm".
    void play(const QString &name);

    // The long voices: "engine" (idle putter), "engine-move" (gallop) and
    // "music" (the stage loop). 0 is silent; they never stop playing.
    void setLoopVolume(const QString &voice, float volume);

    // Mute silences everything without stopping the device.
    void setMuted(bool muted);

    // Asks the pump to reopen the device (a device change reported by SDL).
    // The voices and their volumes survive it.
    void reopen();

    // Health: the pump asks for a full backend rebuild when the device stops
    // consuming; the board, on the GUI thread, performs it (SDL requires the
    // main thread for SDL_InitSubSystem / SDL_QuitSubSystem).
    bool reinitRequested() const;
    void reinitNow();

private:
    struct Impl;
    Impl *d = nullptr;
};
