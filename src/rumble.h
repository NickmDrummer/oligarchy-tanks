// Rumble for DualSense pads, built on a plain hidraw write.
//
// The pad's kind matters: a DualSense talks Sony's proprietary output
// report, not the evdev force-feedback interface (the kernel driver
// never registers one). The protocol below comes from
// drivers/hid/hid-playstation.c and was felt working on the real pad
// (AGENTS.md section 21.21):
//
//   - USB output report 0x02, 63 bytes. The common section starts at
//     offset 1: valid_flag0 [0], valid_flag1 [1], motor_right [2] — the
//     small high-frequency motor, felt on the right — motor_left [3] —
//     the big low-frequency one — and valid_flag2 at [38].
//   - Rumble needs DS_OUTPUT_VALID_FLAG0_HAPTICS_SELECT (bit 1) plus
//     either COMPATIBLE_VIBRATION (bit 0 of flag0, original firmware)
//     or COMPATIBLE_VIBRATION2 (bit 2 of flag2). Which one is decided
//     by the firmware's update_version, read from feature report 0x20
//     at offset 44: v2 from 2.21 on, exactly as the kernel does it.
//   - The motors keep running until a report with both at zero.
//
// Xbox pads are deliberately out of scope: their rumble normally goes
// through an evdev driver (xpad/xpadneo) whose presence varies, and
// there is no Xbox pad on the bench. Nothing in the game breaks when
// one is plugged: only DualSense pads are picked up.
//
// Everything the hardware never sees lives next to the class: the
// report builder, the firmware-version parse and the event-to-strength
// table are plain functions, so the tests can check bytes without a pad.

#ifndef RUMBLE_H
#define RUMBLE_H

#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

class QTimer;

namespace RumbleProtocol {

constexpr int kReportSize = 63;
// Offsets inside the output report buffer.
constexpr int kValidFlag0 = 1;
constexpr int kMotorRight = 3;
constexpr int kMotorLeft = 4;
constexpr int kValidFlag2 = 39;
constexpr int kFlag0HapticsSelect = 0x02;
constexpr int kFlag0CompatibleVibration = 0x01;
constexpr int kFlag2CompatibleVibration2 = 0x04;

// Builds one vibration report: the strength of each motor, 0..255. A
// report with both at zero stops the motors.
void buildReport(unsigned char *buf, int size, bool v2,
                 unsigned char strong, unsigned char weak);

// The firmware's update_version, read out of a feature report 0x20 of
// 64 bytes (or empty on a failed read: the caller passes nullptr in
// that case and the version is unknown). The two bytes at offset 44
// are little endian, exactly as the kernel reads them.
int featureVersion(const unsigned char *feature, int size);

bool usesV2(int featureVersion);

} // namespace RumbleProtocol

// How long and how hard one event rumbles. rumbleFor() answers the
// shared sound names (a step of a menu is not a rumble event); a name
// that does not rumble comes back all zero.
struct RumbleSpec {
    unsigned char strong = 0;
    unsigned char weak = 0;
    int ms = 0;
};
RumbleSpec rumbleFor(const QString &name);

// The rumble player. It opens every DualSense hidraw it can find
// (rescanning every 2 s, so a pad that is plugged in while playing
// joins in), writes the reports and stops the motors when a pattern
// runs out. The writes are O_NONBLOCK and an error only drops that
// pad: like the sounds, a dead pad cannot wedge the GUI thread.
//
// OLIGARCHY_TANKS_RUMBLE=0 turns the whole thing off.
class Rumble : public QObject {
    Q_OBJECT

public:
    explicit Rumble(QObject *parent = nullptr);
    ~Rumble() override;

    void start();

    // QML can only call slots and Q_INVOKABLE methods: a plain method
    // means a call that fails silently (the sound board's lesson,
    // AGENTS.md section 21.18.1).
    Q_INVOKABLE void play(const QString &name);
    Q_INVOKABLE void setMuted(bool muted);

private slots:
    void scan();
    void settle();

private:
    struct Link {
        int fd = -1;
        QString node;      // the hidraw name, "hidraw8", for hotplug checks
        bool v2 = false;
    };
    // Sends one report; a broken link is only marked dead. prune()
    // drops the dead ones, which keeps list mutation out of a loop.
    void send(Link &link, unsigned char strong, unsigned char weak);
    void prune();
    void stopAll();

    QList<Link> m_links;
    QSet<QString> m_rejected;           // hidraw nodes we cannot use yet
    QTimer *m_scan = nullptr;
    QTimer *m_settle = nullptr;
    qint64 m_until = 0;                 // when the motors go back to zero
    bool m_muted = false;
};

#endif // RUMBLE_H
