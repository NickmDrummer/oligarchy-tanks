#include "padsdl.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>

#include "backend.h"

namespace {

double clampUnit(double v) {
    return std::max(-1.0, std::min(1.0, v));
}

} // namespace

PadSdl::PadSdl(Backend *backend, QObject *parent) : QObject(parent), m_backend(backend) {
    m_debug = !qEnvironmentVariableIsEmpty("OLIGARCHY_TANKS_PAD_DEBUG");
}

PadSdl::~PadSdl() {
    // The timer must stop before the controllers and SDL go away: a pending
    // timeout would otherwise read a closed handle.
    if (m_timer)
        m_timer->stop();
    for (Pad &p : m_pads)
        if (p.controller)
            SDL_GameControllerClose(p.controller);
    m_pads.clear();
    if (m_ready)
        SDL_Quit();
}

void PadSdl::start() {
    // macOS delivers pad events to the frontmost window only unless this is
    // set; the manual overlay and a screenshot tool would otherwise leave the
    // pad deaf for a moment.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    // SDL installs SIGINT/SIGTERM handlers of its own and turns them into
    // SDL_QUIT events: with nothing listening for those, the process would
    // ignore both Ctrl+C and `timeout`. Qt owns the event loop here.
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "[pad] SDL could not start: %s\n", SDL_GetError());
        return;
    }
    m_ready = true;
    SDL_GameControllerEventState(SDL_ENABLE);
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
        if (SDL_IsGameController(i))
            addController(i);

    // 16 ms: the game ticks at 60 Hz and the mapper only acts on changes, so
    // this is the cheapest cadence that feels immediate.
    m_timer = new QTimer(this);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, [this] {
        pump();
        feed();
    });
    m_timer->start();

    if (m_debug)
        fprintf(stderr, "[pad] SDL %d.%d.%d, %d controller(s)\n",
                SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL,
                int(m_pads.size()));
}

void PadSdl::pump() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_CONTROLLERDEVICEADDED)
            addController(e.cdevice.which);   // a device index
        else if (e.type == SDL_CONTROLLERDEVICEREMOVED)
            removeController(e.cdevice.which); // an instance id
    }
}

void PadSdl::addController(int deviceIndex) {
    _SDL_GameController *controller = SDL_GameControllerOpen(deviceIndex);
    if (!controller) {
        if (m_debug)
            fprintf(stderr, "[pad] open %d failed: %s\n", deviceIndex, SDL_GetError());
        return;
    }
    _SDL_Joystick *joystick = SDL_GameControllerGetJoystick(controller);
    const qint64 id = SDL_JoystickInstanceID(joystick);
    if (padForId(id)) {
        SDL_GameControllerClose(controller);
        return;
    }

    Pad pad;
    pad.index = nextIndex();
    pad.id = id;
    const char *name = SDL_GameControllerName(controller);
    pad.name = QString::fromUtf8(name ? name : "Pad");
    pad.controller = controller;
    pad.state.index = pad.index;
    m_pads.push_back(pad);

    if (m_debug)
        fprintf(stderr, "[pad] index %d: %s (id %lld)\n", pad.index,
                qPrintable(pad.name), (long long)pad.id);
    if (m_backend)
        m_backend->padConnected(pad.index, pad.name);
}

void PadSdl::removeController(qint64 id) {
    for (auto it = m_pads.begin(); it != m_pads.end(); ++it) {
        if (it->id != id)
            continue;
        if (m_debug)
            fprintf(stderr, "[pad] index %d (%s) gone\n", it->index,
                    qPrintable(it->name));
        // Let go of whatever it was holding before its slot disappears.
        m_mapper.release(it->index, *this);
        if (it->controller)
            SDL_GameControllerClose(it->controller);
        const int index = it->index;
        m_pads.erase(it);
        if (m_backend)
            m_backend->padDisconnected(index);
        return;
    }
}

PadSdl::Pad *PadSdl::padForId(qint64 id) {
    for (Pad &p : m_pads)
        if (p.id == id)
            return &p;
    return nullptr;
}

int PadSdl::nextIndex() const {
    for (int i = 0;; ++i) {
        bool used = false;
        for (const Pad &p : m_pads)
            if (p.index == i)
                used = true;
        if (!used)
            return i;
    }
}

void PadSdl::feed() {
    for (Pad &p : m_pads) {
        _SDL_GameController *c = p.controller;
        PadState &s = p.state;
        s.lx = clampUnit(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX) / 32768.0);
        s.ly = clampUnit(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY) / 32768.0);
        s.lt = std::max(0.0, SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0);
        s.rt = std::max(0.0, SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0);
        // By position, as PadState documents: WEST is X on an Xbox pad and
        // Square on a PlayStation one, and SDL names its buttons that way.
        s.up = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
        s.down = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
        s.left = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
        s.right = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
        s.west = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_X) != 0;
        s.start = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_START) != 0;
        s.select = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_BACK) != 0;
        m_mapper.apply(s, *this);
    }
}

void PadSdl::padMove(int pad, int dx, int dy, bool pressed) {
    if (m_debug)
        fprintf(stderr, "[pad] %d move %s%s\n", pad,
                dx < 0 ? "L" : dx > 0 ? "R" : "",
                dy < 0 ? "U" : dy > 0 ? "D" : "");
    if (m_backend)
        m_backend->padMove(pad, dx, dy, pressed);
}

void PadSdl::padFire(int pad) {
    if (m_debug)
        fprintf(stderr, "[pad] %d fire\n", pad);
    if (m_backend)
        m_backend->padFire(pad);
}

void PadSdl::padStart(int pad) {
    if (m_debug)
        fprintf(stderr, "[pad] %d START\n", pad);
    if (m_backend)
        m_backend->padStart(pad);
}

void PadSdl::padSelect(int pad) {
    if (m_debug)
        fprintf(stderr, "[pad] %d SELECT\n", pad);
    if (m_backend)
        m_backend->padSelect(pad);
}
