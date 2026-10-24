#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>
#include <vector>

#include "padmap.h"

class Backend;
struct _SDL_GameController;

// Gamepads through SDL2: one reader for macOS, Windows and Linux.
//
// The Linux-only reader in gamepad.cpp reads evdev directly, which is the
// right thing on Linux but does not exist anywhere else. SDL2 gives the same
// information through a portable API (and its own controller database), so
// this class has the same shape as Gamepad: it turns a pad snapshot into
// PadState and hands it to PadMapper, which owns what each control means and
// is unit tested on its own.
//
// SDL is polled from a timer in the GUI thread: SDL_Init, the event queue and
// the controller handles all belong to the thread that called start().
// Hotplug arrives as SDL_CONTROLLERDEVICEADDED / REMOVED events.
class PadSdl : public QObject, public PadSink {
    Q_OBJECT
public:
    explicit PadSdl(Backend *backend, QObject *parent = nullptr);
    ~PadSdl() override;

    // Opens the controllers already plugged in and starts polling.
    void start();

    // SDL owns one event queue: this reader pumps it, so the audio device
    // events have to leave through here too. The board reopens its device.
    std::function<void()> onAudioDeviceChange;

    // PadSink: what the mapper made of a pad snapshot.
    void padMove(int pad, int dx, int dy, bool pressed) override;
    void padFire(int pad) override;
    void padStart(int pad) override;
    void padSelect(int pad) override;

private:
    struct Pad {
        int index = 0;   // the slot the game sees
        qint64 id = -1;  // SDL's instance id
        QString name;
        _SDL_GameController *controller = nullptr;
        PadState state;
    };

    void pump();
    void feed();
    void addController(int deviceIndex);
    void removeController(qint64 id);
    Pad *padForId(qint64 id);
    int nextIndex() const;

    Backend *m_backend = nullptr;
    PadMapper m_mapper;
    QTimer *m_timer = nullptr;
    std::vector<Pad> m_pads;
    bool m_ready = false;
    bool m_debug = false;
};
