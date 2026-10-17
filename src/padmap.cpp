#include "padmap.h"

PadMapper::Held &PadMapper::heldFor(int pad) {
    if (pad < 0)
        pad = 0;
    if (int(m_held.size()) <= pad)
        m_held.resize(pad + 1);
    return m_held[pad];
}

void PadMapper::apply(const PadState &s, PadSink &sink) {
    Held &h = heldFor(s.index);

    // One pad drives one player, so only the left stick moves the tank. Both
    // axes can be set at once on a diagonal; the backend keeps the lane.
    const bool up = s.up || s.ly <= -DEADZONE;
    const bool down = s.down || s.ly >= DEADZONE;
    const bool left = s.left || s.lx <= -DEADZONE;
    const bool right = s.right || s.lx >= DEADZONE;

    // Fire face button by position (X / Square), or either trigger pulled
    // past the halfway mark. Comfort buttons are folded in by the reader.
    const bool fire = s.west || s.lt >= TRIGGER || s.rt >= TRIGGER;

    if (up != h.up) {
        h.up = up;
        sink.padMove(s.index, 0, -1, up);
    }
    if (right != h.right) {
        h.right = right;
        sink.padMove(s.index, 1, 0, right);
    }
    if (down != h.down) {
        h.down = down;
        sink.padMove(s.index, 0, 1, down);
    }
    if (left != h.left) {
        h.left = left;
        sink.padMove(s.index, -1, 0, left);
    }
    if (fire != h.fire) {
        h.fire = fire;
        if (fire)
            sink.padFire(s.index);
    }
    if (s.start != h.start) {
        h.start = s.start;
        if (s.start)
            sink.padStart(s.index);
    }
    if (s.select != h.select) {
        h.select = s.select;
        if (s.select)
            sink.padSelect(s.index);
    }
}

void PadMapper::release(int pad, PadSink &sink) {
    Held &h = heldFor(pad);
    if (h.up)
        sink.padMove(pad, 0, -1, false);
    if (h.right)
        sink.padMove(pad, 1, 0, false);
    if (h.down)
        sink.padMove(pad, 0, 1, false);
    if (h.left)
        sink.padMove(pad, -1, 0, false);
    h = Held{};
}
