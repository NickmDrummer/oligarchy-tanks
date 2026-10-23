// The rumble player and its protocol (rumble.h). Linux only: this file
// is excluded from the .pro on every other platform, and main.cpp
// guards its use the same way.
#include "rumble.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTimer>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/hidraw.h>

namespace {

// The DualSense's USB identity, as sysfs prints it in HID_ID.
constexpr int kVendor = 0x054c;
constexpr int kProduct = 0x0ce6;
// Bus 0x0003 is USB; 0x0005 is Bluetooth, whose rumble report is a
// different, CRC-signed one. Bluetooth pads are skipped, quietly.
constexpr int kUsb = 0x0003;
// The kernel's own line: firmware 2.21 switches the vibration method.
constexpr int kV2Version = (2 << 8) | 21;
constexpr int kFeatureReport = 0x20;
constexpr int kOutputReport = 0x02;

// One small sysfs read answers whether a hidraw node is a DualSense and
// what bus it is on: no ioctls, no open() (the gamepad reader's lesson,
// section 7.2). Returns the bus, or 0 when the node is not a DualSense.
int dualSenseBus(const QString &node) {
    QFile uevent(QStringLiteral("/sys/class/hidraw/%1/device/uevent").arg(node));
    if (!uevent.open(QIODevice::ReadOnly))
        return 0;
    int bus = 0, vendor = 0, product = 0;
    const QStringList lines = QString::fromLatin1(uevent.readAll()).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("HID_ID=")))
            std::sscanf(qPrintable(line.mid(7)), "%x:%x:%x", &bus, &vendor, &product);
    }
    return vendor == kVendor && product == kProduct ? bus : 0;
}

} // namespace

namespace RumbleProtocol {

void buildReport(unsigned char *buf, int size, bool v2,
                 unsigned char strong, unsigned char weak) {
    if (!buf || size < kReportSize)
        return;
    std::memset(buf, 0, size_t(size));
    buf[0] = kOutputReport;
    buf[kValidFlag0] = kFlag0HapticsSelect;
    if (!v2)
        buf[kValidFlag0] |= kFlag0CompatibleVibration;
    else
        buf[kValidFlag2] = kFlag2CompatibleVibration2;
    buf[kMotorRight] = weak;    // the small high-frequency motor
    buf[kMotorLeft] = strong;   // the big low-frequency one
}

int featureVersion(const unsigned char *feature, int size) {
    // Offset 44 is where the kernel reads the update_version (a
    // little-endian word: major in the high half). A failed read is
    // unknown, and 0 is never v2.
    if (!feature || size < 46)
        return 0;
    return int(unsigned(feature[44]) | unsigned(feature[45]) << 8);
}

bool usesV2(int featureVersion) {
    return featureVersion >= kV2Version;
}

} // namespace RumbleProtocol

RumbleSpec rumbleFor(const QString &name) {
    // First cut of the strengths, to be tuned by hand: the small motor
    // is the sharp one on the right, the big one the low rumble. Enemy
    // fire deliberately does not rumble: the board is a battlefield and
    // the pad would never rest.
    if (name == QStringLiteral("shoot"))               return {0x00, 0x40, 60};
    if (name == QStringLiteral("blast_missile-shoot")) return {0x30, 0x50, 90};
    if (name == QStringLiteral("hit"))                 return {0x50, 0x30, 90};
    if (name == QStringLiteral("explode"))             return {0x90, 0x40, 200};
    if (name == QStringLiteral("blast_missile-hit"))   return {0x80, 0x50, 180};
    if (name == QStringLiteral("power"))               return {0x00, 0x50, 100};
    if (name == QStringLiteral("levelup"))             return {0x60, 0x60, 150};
    if (name == QStringLiteral("nuke"))                return {0xff, 0x80, 450};
    if (name == QStringLiteral("air-support"))         return {0x70, 0x50, 300};
    // The endings: the game over is a last grounded thud, the victory
    // a short satisfied buzz. The count-up ticks and the menu clicks
    // stay silent — the hand must not purr at every number.
    if (name == QStringLiteral("gameover"))            return {0xaa, 0x20, 350};
    if (name == QStringLiteral("victory"))             return {0x40, 0x50, 260};
    return {0, 0, 0};
}

// ---- the player ----

Rumble::Rumble(QObject *parent) : QObject(parent) {
    m_scan = new QTimer(this);
    m_scan->setInterval(2000);
    connect(m_scan, &QTimer::timeout, this, &Rumble::scan);

    // The until timer: a DualSense keeps its motors running until a
    // zero report arrives, so the stop is the player's job.
    m_settle = new QTimer(this);
    m_settle->setInterval(40);
    connect(m_settle, &QTimer::timeout, this, &Rumble::settle);
}

Rumble::~Rumble() {
    stopAll();
    for (const Link &l : m_links)
        if (l.fd >= 0)
            ::close(l.fd);
}

void Rumble::start() {
    // OLIGARCHY_TANKS_RUMBLE=0 is the off switch; with it the timers do
    // not even run.
    if (qEnvironmentVariable("OLIGARCHY_TANKS_RUMBLE") == QStringLiteral("0"))
        return;
    scan();
    m_scan->start();
    m_settle->start();
}

void Rumble::scan() {
    // What is on the bus now. The nodes are symlinks to directories, and
    // the device-file filter the /dev/input scan uses sees none of them
    // (measured: System lists 0 of the 9 nodes, AllEntries 9).
    const QStringList nodes =
        QDir(QStringLiteral("/sys/class/hidraw"))
            .entryList(QStringList() << QStringLiteral("hidraw*"),
                       QDir::AllEntries | QDir::NoDotAndDotDot);

    // A hidraw gone from sysfs is a pad gone: close it. Walking backwards
    // keeps the indices valid, exactly like the evdev reader does.
    for (int i = m_links.size() - 1; i >= 0; --i)
        if (!nodes.contains(m_links[i].node)) {
            ::close(m_links[i].fd);
            m_links.removeAt(i);
        }
    // A rejected node that disappears may come back as something else.
    for (const QString &node : QSet<QString>(m_rejected).values())
        if (!nodes.contains(node))
            m_rejected.remove(node);

    for (const QString &node : nodes) {
        if (m_rejected.contains(node))
            continue;
        bool known = false;
        for (const Link &l : m_links)
            if (l.node == node)
                known = true;
        if (known)
            continue;
        const int bus = dualSenseBus(node);
        if (bus == 0) {
            m_rejected.insert(node);    // not a DualSense; do not ask sysfs twice
            continue;
        }
        if (bus != kUsb) {
            m_rejected.insert(node);    // Bluetooth: another report format, out of scope
            continue;
        }
        Link link;
        link.node = node;
        link.fd = ::open(QString(QStringLiteral("/dev/%1").arg(node)).toLocal8Bit().constData(),
                         O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (link.fd < 0) {
            // No permission, or the node died between sysfs and open().
            m_rejected.insert(node);
            continue;
        }
        unsigned char feature[64] = {0};
        feature[0] = kFeatureReport;
        if (ioctl(link.fd, HIDIOCGFEATURE(64), feature) == 64)
            link.v2 = RumbleProtocol::usesV2(RumbleProtocol::featureVersion(feature, 64));
        m_links.append(link);
    }
}

void Rumble::play(const QString &name) {
    if (m_muted)
        return;
    const RumbleSpec spec = rumbleFor(name);
    if (spec.ms <= 0 || m_links.isEmpty())
        return;
    for (int i = m_links.size() - 1; i >= 0; --i)
        send(m_links[i], spec.strong, spec.weak);
    prune();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now > m_until)
        m_until = now + spec.ms;
}

void Rumble::setMuted(bool muted) {
    m_muted = muted;
    if (muted) {
        stopAll();
        m_until = 0;
    }
}

void Rumble::settle() {
    if (m_until == 0 || QDateTime::currentMSecsSinceEpoch() < m_until)
        return;
    stopAll();
    m_until = 0;
}

// Sends one report. send() itself only marks a broken link dead (it is
// sometimes called from a walk of m_links); prune() drops the dead
// ones, which keeps the mutation out of the loop that is running.
void Rumble::send(Link &link, unsigned char strong, unsigned char weak) {
    if (link.fd < 0)
        return;
    unsigned char buf[RumbleProtocol::kReportSize];
    RumbleProtocol::buildReport(buf, sizeof(buf), link.v2, strong, weak);
    if (::write(link.fd, buf, sizeof(buf)) < 0 && errno != EAGAIN) {
        ::close(link.fd);
        link.fd = -1;
    }
}

void Rumble::prune() {
    for (int i = m_links.size() - 1; i >= 0; --i)
        if (m_links[i].fd < 0)
            m_links.removeAt(i);
}

void Rumble::stopAll() {
    for (int i = m_links.size() - 1; i >= 0; --i)
        send(m_links[i], 0, 0);
    prune();
}
