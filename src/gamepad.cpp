#include "gamepad.h"

#include "backend.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSocketNotifier>
#include <QTimer>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

bool bitSet(const unsigned long *bits, int bit) {
    const int per = int(8 * sizeof(unsigned long));
    return (bits[bit / per] >> (bit % per)) & 1UL;
}

// Sticks are centred: -1 at the low end, +1 at the high end. Works whether the
// pad reports 0..255 (hid-playstation) or -32768..32767 (xpad).
double stickValue(const PadAxis &a, int raw) {
    if (!a.present || a.hi <= a.lo)
        return 0.0;
    const double mid = (a.lo + a.hi) / 2.0;
    const double half = (a.hi - a.lo) / 2.0;
    return std::clamp((raw - mid) / half, -1.0, 1.0);
}

// Triggers rest at the bottom and only travel one way: 0..1.
double triggerValue(const PadAxis &a, int raw) {
    if (!a.present || a.hi <= a.lo)
        return 0.0;
    return std::clamp(double(raw - a.lo) / double(a.hi - a.lo), 0.0, 1.0);
}

const char *axisName(int code) {
    switch (code) {
    case ABS_X: return "ABS_X";
    case ABS_Y: return "ABS_Y";
    case ABS_Z: return "ABS_Z";
    case ABS_RZ: return "ABS_RZ";
    case ABS_HAT0X: return "ABS_HAT0X";
    case ABS_HAT0Y: return "ABS_HAT0Y";
    default: return "ABS_?";
    }
}

// The capability masks are in sysfs too, and reading two small files costs
// nothing while opening (and especially closing) an evdev node costs
// milliseconds: probing the whole of /dev/input froze the event loop for over
// half a second. One bit out of /sys/class/input/eventN/device/capabilities/*.
// Returns true when sysfs cannot answer, so the node is probed for real.
bool sysfsBit(const QString &node, const char *group, int bit) {
    QFile file(QStringLiteral("/sys/class/input/%1/device/capabilities/%2").arg(node, QLatin1String(group)));
    if (!file.open(QIODevice::ReadOnly))
        return true;
    // Words are printed most significant first, in hex, each up to 64 bits.
    const QStringList words = QString::fromLatin1(file.readAll()).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const int index = int(words.size()) - 1 - bit / 64;
    if (index < 0 || index >= words.size())
        return false;
    bool ok = false;
    const quint64 value = words.at(index).trimmed().toULongLong(&ok, 16);
    return !ok || (value >> (bit % 64)) & 1ULL;
}

// The same test the kernel probe makes -- a stick with gamepad buttons -- but
// answered without opening the node whenever sysfs knows the answer.
bool looksLikeGamepad(const QString &node) {
    const bool stick = sysfsBit(node, "abs", ABS_X) && sysfsBit(node, "abs", ABS_Y);
    const bool buttons = sysfsBit(node, "key", BTN_GAMEPAD) || sysfsBit(node, "key", BTN_JOYSTICK);
    return stick && buttons;
}

const char *keyName(int code) {
    switch (code) {
    case BTN_WEST: return "BTN_WEST";
    case BTN_START: return "BTN_START";
    case BTN_SELECT: return "BTN_SELECT";
    case BTN_DPAD_UP: return "BTN_DPAD_UP";
    case BTN_DPAD_DOWN: return "BTN_DPAD_DOWN";
    case BTN_DPAD_LEFT: return "BTN_DPAD_LEFT";
    case BTN_DPAD_RIGHT: return "BTN_DPAD_RIGHT";
    default: return "BTN_?";
    }
}

} // namespace

Gamepad::Gamepad(Backend *backend, QObject *parent) : QObject(parent), m_backend(backend) {
    m_debug = !qEnvironmentVariableIsEmpty("TANKS_PAD_DEBUG");
}

Gamepad::~Gamepad() {
    for (const Device &d : m_devices)
        if (d.fd >= 0)
            ::close(d.fd);
}

void Gamepad::start() {
    scan();
    m_rescan = new QTimer(this);
    m_rescan->setInterval(1000);
    connect(m_rescan, &QTimer::timeout, this, &Gamepad::scan);
    m_rescan->start();
}

void Gamepad::scan() {
    const qint64 started = m_debug ? QDateTime::currentMSecsSinceEpoch() : 0;
    static const QString dir = QStringLiteral("/dev/input");
    const QStringList files = QDir(dir).entryList(QStringList() << QStringLiteral("event*"), QDir::System);

    // Whatever is no longer there is gone: walking backwards keeps the
    // indices valid while devices are dropped.
    for (int i = int(m_devices.size()) - 1; i >= 0; --i)
        if (!files.contains(QFileInfo(m_devices[i].path).fileName()))
            closeDevice(m_devices[i].index);

    // A node that has gone away must be looked at again if it comes back: the
    // name may now belong to a different device.
    for (const QString &path : QStringList(m_rejected.begin(), m_rejected.end()))
        if (!files.contains(QFileInfo(path).fileName()))
            m_rejected.remove(path);

    for (const QString &file : files) {
        const QString path = dir + QLatin1Char('/') + file;
        bool known = false;
        for (const Device &d : m_devices)
            if (d.path == path)
                known = true;
        if (!known && !m_rejected.contains(path))
            if (!looksLikeGamepad(file) || !openDevice(path, nextIndex()))
                m_rejected.insert(path);
    }

    if (m_debug)
        fprintf(stderr, "[pad] scan took %lld ms (%d nodes, %d known not-a-pad)\n",
                static_cast<long long>(QDateTime::currentMSecsSinceEpoch() - started), int(files.size()),
                int(m_rejected.size()));
}

int Gamepad::nextIndex() const {
    for (int i = 0; i < 8; ++i) {
        bool used = false;
        for (const Device &d : m_devices)
            if (d.index == i)
                used = true;
        if (!used)
            return i;
    }
    return int(m_devices.size());
}

bool Gamepad::openDevice(const QString &path, int index) {
    const int fd = ::open(path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        if (m_debug)
            fprintf(stderr, "[pad] %s: cannot open: %s\n", qPrintable(path), strerror(errno));
        return false;
    }

    unsigned long absBits[ABS_CNT / (8 * sizeof(unsigned long)) + 1] = {};
    unsigned long keyBits[KEY_CNT / (8 * sizeof(unsigned long)) + 1] = {};
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) < 0
        || ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) < 0) {
        ::close(fd);
        return false;
    }

    // A gamepad is a stick with gamepad buttons. Mice and touchpads also carry
    // ABS_X/ABS_Y, and this is what keeps them out.
    const bool stick = bitSet(absBits, ABS_X) && bitSet(absBits, ABS_Y);
    const bool buttons = bitSet(keyBits, BTN_GAMEPAD) || bitSet(keyBits, BTN_JOYSTICK)
                         || bitSet(keyBits, BTN_TRIGGER);
    if (!stick || !buttons) {
        if (m_debug)
            fprintf(stderr, "[pad] %s: not a gamepad (stick=%d buttons=%d)\n", qPrintable(path), stick, buttons);
        ::close(fd);
        return false;
    }

    Device d;
    d.index = index;
    d.fd = fd;
    d.path = path;
    char name[256] = {};
    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) > 0)
        d.name = QString::fromLocal8Bit(name);

    auto range = [&fd](int code, PadAxis &a) {
        struct input_absinfo info {};
        if (ioctl(fd, EVIOCGABS(code), &info) < 0)
            return;
        a.present = true;
        a.lo = info.minimum;
        a.hi = info.maximum;
    };
    range(ABS_X, d.x);
    range(ABS_Y, d.y);
    range(ABS_Z, d.z);
    range(ABS_RZ, d.rz);
    range(ABS_HAT0X, d.hatX);
    range(ABS_HAT0Y, d.hatY);
    d.triggersAreZ = d.z.present && d.z.lo >= 0 && d.z.hi <= 1023;

    d.state.index = index;
    m_devices.push_back(d);
    Device &stored = m_devices.back();
    stored.notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    // No-argument slot on purpose: the signal's arguments differ between Qt
    // versions, and the fd we need is the one we opened.
    connect(stored.notifier, &QSocketNotifier::activated, this, [this, fd] { readDevice(fd); });

    if (m_debug)
        fprintf(stderr, "[pad] index %d: %s (%s) x=%d..%d y=%d..%d z=%d..%d rz=%d..%d triggerZ=%d\n",
                index, qPrintable(stored.name), qPrintable(path), stored.x.lo, stored.x.hi, stored.y.lo, stored.y.hi,
                stored.z.lo, stored.z.hi, stored.rz.lo, stored.rz.hi, stored.triggersAreZ);
    if (m_backend)
        m_backend->padConnected(index, stored.name);
    return true;
}

void Gamepad::closeDevice(int index) {
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        if (it->index != index)
            continue;
        if (m_debug)
            fprintf(stderr, "[pad] index %d (%s) gone\n", index, qPrintable(it->name));
        // Let go of whatever it was holding before its slot disappears.
        m_mapper.release(index, *this);
        delete it->notifier;
        if (it->fd >= 0)
            ::close(it->fd);
        m_devices.erase(it);
        break;
    }
    if (m_backend)
        m_backend->padDisconnected(index);
}

int Gamepad::indexForFd(int fd) const {
    for (size_t i = 0; i < m_devices.size(); ++i)
        if (m_devices[i].fd == fd)
            return int(i);
    return -1;
}

void Gamepad::readDevice(int fd) {
    struct input_event ev[64];
    for (;;) {
        const int slot = indexForFd(fd);
        if (slot < 0)
            return;
        const ssize_t n = ::read(fd, ev, sizeof(ev));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return;
            closeDevice(m_devices[slot].index); // unplugged
            return;
        }
        if (n == 0)
            return;
        const int count = int(n / ssize_t(sizeof(struct input_event)));
        for (int i = 0; i < count; ++i) {
            const int at = indexForFd(fd);
            if (at < 0)
                return;
            if (ev[i].type == EV_SYN && ev[i].code == SYN_REPORT) {
                flush(m_devices[at]);
                continue;
            }
            handleEvent(m_devices[at], ev[i]);
        }
    }
}

void Gamepad::handleEvent(Device &d, const input_event &ev) {
    if (ev.type == EV_ABS) {
        // Sticks and triggers poll continuously and an 8-bit pad jitters by a
        // couple of counts, so only real movement counts. The d-pad hat is
        // -1/0/1 and must go through untouched.
        const bool analog = ev.code == ABS_X || ev.code == ABS_Y || ev.code == ABS_Z || ev.code == ABS_RZ;
        if (analog && ev.code < 64) {
            if (std::abs(ev.value - d.lastAbs[ev.code]) < 5)
                return;
            d.lastAbs[ev.code] = ev.value;
        }
        if (m_debug)
            fprintf(stderr, "[pad] %s %s=%d\n", qPrintable(d.path), axisName(ev.code), ev.value);
        switch (ev.code) {
        case ABS_X:
            d.state.lx = stickValue(d.x, ev.value);
            break;
        case ABS_Y:
            d.state.ly = stickValue(d.y, ev.value);
            break;
        case ABS_Z:
            if (d.triggersAreZ)
                d.state.lt = triggerValue(d.z, ev.value);
            break;
        case ABS_RZ:
            if (d.triggersAreZ)
                d.state.rt = triggerValue(d.rz, ev.value);
            break;
        case ABS_HAT0X:
            d.state.left = ev.value < 0;
            d.state.right = ev.value > 0;
            break;
        case ABS_HAT0Y:
            d.state.up = ev.value < 0;
            d.state.down = ev.value > 0;
            break;
        default:
            break;
        }
        return;
    }

    if (ev.type != EV_KEY)
        return;
    const bool down = ev.value != 0;
    if (m_debug && ev.value == 1)
        fprintf(stderr, "[pad] %s %s\n", qPrintable(d.path), keyName(ev.code));
    switch (ev.code) {
    // Fire is the trigger axis plus the face button in the west position:
    // Square on a PlayStation pad, X on an Xbox one.
    case BTN_WEST:
        d.state.west = down;
        break;
    case BTN_START:
        d.state.start = down;
        break;
    case BTN_SELECT:
        d.state.select = down;
        break;
    // Some pads report the d-pad as four buttons instead of a hat.
    case BTN_DPAD_UP:
        d.state.up = down;
        break;
    case BTN_DPAD_DOWN:
        d.state.down = down;
        break;
    case BTN_DPAD_LEFT:
        d.state.left = down;
        break;
    case BTN_DPAD_RIGHT:
        d.state.right = down;
        break;
    default:
        break;
    }
}

void Gamepad::flush(Device &d) {
    m_mapper.apply(d.state, *this);
}

void Gamepad::padMove(int pad, int dx, int dy, bool pressed) {
    if (m_backend)
        m_backend->padMove(pad, dx, dy, pressed);
}

void Gamepad::padFire(int pad) {
    if (m_backend)
        m_backend->padFire(pad);
}

void Gamepad::padStart(int pad) {
    if (m_backend)
        m_backend->padStart(pad);
}

void Gamepad::padSelect(int pad) {
    if (m_backend)
        m_backend->padSelect(pad);
}
