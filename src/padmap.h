#pragma once

#include <vector>

// Pad input, deliberately free of Qt and of any device: what a pad *means*
// is decided here, and the device reading lives in gamepad.cpp. That split is
// what lets the mapping be unit tested without a controller plugged in.
//
// The layout is the standard Linux gamepad: left stick, two triggers, a d-pad
// and the four face buttons by position. Both drivers we care about report it
// that way -- xpad for Xbox pads, hid-playstation for DualSense/DualShock --
// so one table covers both. Face buttons are named by position, not by letter:
// `west` is X on an Xbox pad and Square on a PlayStation one.
struct PadState {
    int index = 0; // stable while the pad stays connected
    double lx = 0, ly = 0; // left stick, -1..1
    double lt = 0, rt = 0; // triggers, 0..1
    bool up = false, down = false, left = false, right = false; // d-pad
    bool west = false; // X / Square, the fire face button
    bool start = false; // START / Options
    bool select = false; // SELECT / Share
};

// What the mapper asks the game to do. One pad at a time, addressed by index,
// so the backend can keep the keyboard's flags separate.
struct PadSink {
    virtual ~PadSink() = default;
    virtual void padMove(int pad, int dx, int dy, bool pressed) = 0;
    virtual void padFire(int pad) = 0;
    virtual void padStart(int pad) = 0;
    virtual void padSelect(int pad) = 0;
};

// Turns pad snapshots into direction flags and edge-triggered actions.
// The stick has to leave the deadzone before it counts; fire is either trigger
// travelled past half, or the fire face button; start and select fire once per
// press. A diagonal stick sets both axes, which the tank movement already
// resolves by keeping its current lane.
class PadMapper {
public:
    static constexpr double DEADZONE = 0.35;
    static constexpr double TRIGGER = 0.4;

    void apply(const PadState &s, PadSink &sink);
    // Forget what a pad was holding, so an unplug cannot leave a direction
    // stuck down.
    void release(int pad, PadSink &sink);

private:
    struct Held {
        bool up = false, down = false, left = false, right = false;
        bool fire = false, start = false, select = false;
    };
    Held &heldFor(int pad);
    std::vector<Held> m_held;
};
