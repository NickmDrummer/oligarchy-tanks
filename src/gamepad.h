#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <vector>

#include "padmap.h"

class Backend;
class QSocketNotifier;
class QTimer;
struct input_event;

// One analog axis as the kernel describes it, with its own range: pads differ
// (0..255 on a DualSense, -32768..32767 on an Xbox pad), so the raw value is
// always scaled through these.
struct PadAxis {
    bool present = false;
    int lo = 0, hi = 0;
};

// Reads gamepads straight from /dev/input/event*.
//
// The kernel already presents Xbox pads (xpad) and PlayStation pads
// (hid-playstation) with the same standard codes -- left stick on ABS_X/Y,
// triggers on ABS_Z/RZ, d-pad on ABS_HAT0X/Y, face buttons by position -- so
// one table covers both brands and no mapping database is needed. Reading
// those nodes needs the user in the `input` group; with no pad, or no
// permission, the game runs on the keyboard exactly as before.
//
// This class only turns bytes into PadState and hands them to PadMapper,
// which owns what each control means and is unit tested on its own.
class Gamepad : public QObject, public PadSink {
    Q_OBJECT
public:
    explicit Gamepad(Backend *backend, QObject *parent = nullptr);
    ~Gamepad() override;

    // Scans once, then keeps scanning on a slow timer, so pads can be plugged
    // in and out while the game runs.
    void start();

    // PadSink: what the mapper made of a pad snapshot.
    void padMove(int pad, int dx, int dy, bool pressed) override;
    void padFire(int pad) override;
    void padStart(int pad) override;
    void padSelect(int pad) override;

private:
    struct Device {
        int index = 0;
        int fd = -1;
        QString path;
        QString name;
        // ABS_Z/ABS_RZ are triggers (0..255) on both drivers we support. If a
        // pad puts a stick there instead, those axes are simply not read.
        bool triggersAreZ = false;
        PadAxis x, y, z, rz, hatX, hatY;
        PadState state;
        // Last seen raw value per axis, so a poll that only jitters is ignored.
        int lastAbs[64] = {};
        QSocketNotifier *notifier = nullptr;
    };

    void scan();
    bool openDevice(const QString &path, int index);
    void closeDevice(int index);
    void readDevice(int fd);
    void handleEvent(Device &d, const input_event &ev);
    void flush(Device &d);
    int indexForFd(int fd) const;
    int nextIndex() const;

    Backend *m_backend = nullptr;
    PadMapper m_mapper;
    std::vector<Device> m_devices;
    // Nodes already looked at that are not gamepads. Opening and probing one
    // costs milliseconds, and /dev/input has dozens: re-probing them all every
    // second froze the event loop for over half a second.
    QSet<QString> m_rejected;
    QTimer *m_rescan = nullptr;
    bool m_debug = false;
};
