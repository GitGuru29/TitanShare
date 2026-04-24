#include "input/virtual_input.hpp"
#include "utils/logger.hpp"
#include "config.hpp"

#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <linux/input-event-codes.h>
#include <time.h>

namespace bybridge {

VirtualInput::VirtualInput() {
    std::memset(m_asciiMap, 0, sizeof(m_asciiMap));
}

VirtualInput::~VirtualInput() {
    if (m_mouseFd >= 0) {
        ioctl(m_mouseFd, UI_DEV_DESTROY);
        close(m_mouseFd);
    }
    if (m_kbFd >= 0) {
        ioctl(m_kbFd, UI_DEV_DESTROY);
        close(m_kbFd);
    }
}

bool VirtualInput::init() {
    m_mouseFd = createMouse();
    if (m_mouseFd < 0) {
        Logger::error("INPUT", "❌ Failed to create virtual mouse");
        return false;
    }

    m_kbFd = createKeyboard();
    if (m_kbFd < 0) {
        Logger::error("INPUT", "❌ Failed to create virtual keyboard");
        return false;
    }

    initAsciiMap();
    m_initialized = true;
    Logger::info("INPUT", "✅ Virtual input devices ready (mouse + keyboard)");
    return true;
}

int VirtualInput::createMouse() {
    int fd = open(config::UINPUT_PATH.c_str(), O_WRONLY | O_NONBLOCK);
    if (fd < 0) return -1;

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);

    ioctl(fd, UI_SET_EVBIT, EV_REL);
    ioctl(fd, UI_SET_RELBIT, REL_X);
    ioctl(fd, UI_SET_RELBIT, REL_Y);
    ioctl(fd, UI_SET_RELBIT, REL_WHEEL);

    struct uinput_user_dev uud{};
    std::strncpy(uud.name, config::MOUSE_DEVICE_NAME.c_str(), UINPUT_MAX_NAME_SIZE - 1);
    uud.id.bustype = BUS_USB;
    uud.id.vendor = 0x1;
    uud.id.product = 0x2;
    uud.id.version = 1;

    write(fd, &uud, sizeof(uud));
    ioctl(fd, UI_DEV_CREATE);
    return fd;
}

int VirtualInput::createKeyboard() {
    int fd = open(config::UINPUT_PATH.c_str(), O_WRONLY | O_NONBLOCK);
    if (fd < 0) return -1;

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    for (int i = 1; i < 255; i++) {
        ioctl(fd, UI_SET_KEYBIT, i);
    }

    struct uinput_user_dev uud{};
    std::strncpy(uud.name, config::KEYBOARD_DEVICE_NAME.c_str(), UINPUT_MAX_NAME_SIZE - 1);
    uud.id.bustype = BUS_USB;
    uud.id.vendor = 0x1;
    uud.id.product = 0x3;
    uud.id.version = 1;

    write(fd, &uud, sizeof(uud));
    ioctl(fd, UI_DEV_CREATE);
    return fd;
}

void VirtualInput::emit(int fd, int type, int code, int value) {
    struct input_event ie{};

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ie.time.tv_sec = ts.tv_sec;
    ie.time.tv_usec = ts.tv_nsec / 1000;

    ie.type = type;
    ie.code = code;
    ie.value = value;

    write(fd, &ie, sizeof(ie));
}

void VirtualInput::emitSyn(int fd) {
    emit(fd, EV_SYN, SYN_REPORT, 0);
}

void VirtualInput::moveMouse(int dx, int dy) {
    if (!m_initialized) return;
    emit(m_mouseFd, EV_REL, REL_X, dx);
    emit(m_mouseFd, EV_REL, REL_Y, dy);
    emitSyn(m_mouseFd);
}

void VirtualInput::scrollWheel(int dy) {
    if (!m_initialized) return;
    emit(m_mouseFd, EV_REL, REL_WHEEL, dy);
    emitSyn(m_mouseFd);
}

int VirtualInput::getButtonCode(const std::string& button) {
    if (button == "LEFT") return BTN_LEFT;
    if (button == "RIGHT") return BTN_RIGHT;
    if (button == "MIDDLE") return BTN_MIDDLE;
    return BTN_LEFT;
}

void VirtualInput::clickMouse(const std::string& button) {
    if (!m_initialized) return;
    int btn = getButtonCode(button);
    emit(m_mouseFd, EV_KEY, btn, 1);
    emitSyn(m_mouseFd);
    usleep(50000);
    emit(m_mouseFd, EV_KEY, btn, 0);
    emitSyn(m_mouseFd);
}

void VirtualInput::mouseDown(const std::string& button) {
    if (!m_initialized) return;
    emit(m_mouseFd, EV_KEY, getButtonCode(button), 1);
    emitSyn(m_mouseFd);
}

void VirtualInput::mouseUp(const std::string& button) {
    if (!m_initialized) return;
    emit(m_mouseFd, EV_KEY, getButtonCode(button), 0);
    emitSyn(m_mouseFd);
}

void VirtualInput::typeKeyCode(int code, bool shift) {
    if (shift) {
        emit(m_kbFd, EV_KEY, KEY_LEFTSHIFT, 1);
        emitSyn(m_kbFd);
    }
    emit(m_kbFd, EV_KEY, code, 1);
    emitSyn(m_kbFd);
    emit(m_kbFd, EV_KEY, code, 0);
    emitSyn(m_kbFd);
    if (shift) {
        emit(m_kbFd, EV_KEY, KEY_LEFTSHIFT, 0);
        emitSyn(m_kbFd);
    }
}

void VirtualInput::typeText(const std::string& text) {
    if (!m_initialized) return;
    for (char c : text) {
        if (c < 0 || c >= 128) continue;
        auto& m = m_asciiMap[static_cast<int>(c)];
        if (m.code != 0) {
            typeKeyCode(m.code, m.shift);
        }
    }
}

int VirtualInput::getKeyCodeFromName(const std::string& name) {
    if (name == "BACKSPACE") return KEY_BACKSPACE;
    if (name == "ENTER") return KEY_ENTER;
    if (name == "TAB") return KEY_TAB;
    if (name == "SHIFT") return KEY_LEFTSHIFT;
    if (name == "ESC") return KEY_ESC;
    if (name == "SPACE") return KEY_SPACE;
    if (name == "PRINTSCREEN") return KEY_PRINT;
    if (name == "LALT") return KEY_LEFTALT;
    if (name == "CAPSLOCK") return KEY_CAPSLOCK;
    if (name == "DELETE") return KEY_DELETE;
    if (name == "HOME") return KEY_HOME;
    if (name == "END") return KEY_END;
    if (name == "UP") return KEY_UP;
    if (name == "DOWN") return KEY_DOWN;
    if (name == "LEFT") return KEY_LEFT;
    if (name == "RIGHT") return KEY_RIGHT;
    return 0;
}

void VirtualInput::pressKey(const std::string& keyName) {
    if (!m_initialized) return;
    int code = getKeyCodeFromName(keyName);
    if (code != 0) {
        emit(m_kbFd, EV_KEY, code, 1);
        emitSyn(m_kbFd);
        emit(m_kbFd, EV_KEY, code, 0);
        emitSyn(m_kbFd);
    }
}

void VirtualInput::initAsciiMap() {
    // Lowercase letters
    m_asciiMap['a'] = {KEY_A, false}; m_asciiMap['A'] = {KEY_A, true};
    m_asciiMap['b'] = {KEY_B, false}; m_asciiMap['B'] = {KEY_B, true};
    m_asciiMap['c'] = {KEY_C, false}; m_asciiMap['C'] = {KEY_C, true};
    m_asciiMap['d'] = {KEY_D, false}; m_asciiMap['D'] = {KEY_D, true};
    m_asciiMap['e'] = {KEY_E, false}; m_asciiMap['E'] = {KEY_E, true};
    m_asciiMap['f'] = {KEY_F, false}; m_asciiMap['F'] = {KEY_F, true};
    m_asciiMap['g'] = {KEY_G, false}; m_asciiMap['G'] = {KEY_G, true};
    m_asciiMap['h'] = {KEY_H, false}; m_asciiMap['H'] = {KEY_H, true};
    m_asciiMap['i'] = {KEY_I, false}; m_asciiMap['I'] = {KEY_I, true};
    m_asciiMap['j'] = {KEY_J, false}; m_asciiMap['J'] = {KEY_J, true};
    m_asciiMap['k'] = {KEY_K, false}; m_asciiMap['K'] = {KEY_K, true};
    m_asciiMap['l'] = {KEY_L, false}; m_asciiMap['L'] = {KEY_L, true};
    m_asciiMap['m'] = {KEY_M, false}; m_asciiMap['M'] = {KEY_M, true};
    m_asciiMap['n'] = {KEY_N, false}; m_asciiMap['N'] = {KEY_N, true};
    m_asciiMap['o'] = {KEY_O, false}; m_asciiMap['O'] = {KEY_O, true};
    m_asciiMap['p'] = {KEY_P, false}; m_asciiMap['P'] = {KEY_P, true};
    m_asciiMap['q'] = {KEY_Q, false}; m_asciiMap['Q'] = {KEY_Q, true};
    m_asciiMap['r'] = {KEY_R, false}; m_asciiMap['R'] = {KEY_R, true};
    m_asciiMap['s'] = {KEY_S, false}; m_asciiMap['S'] = {KEY_S, true};
    m_asciiMap['t'] = {KEY_T, false}; m_asciiMap['T'] = {KEY_T, true};
    m_asciiMap['u'] = {KEY_U, false}; m_asciiMap['U'] = {KEY_U, true};
    m_asciiMap['v'] = {KEY_V, false}; m_asciiMap['V'] = {KEY_V, true};
    m_asciiMap['w'] = {KEY_W, false}; m_asciiMap['W'] = {KEY_W, true};
    m_asciiMap['x'] = {KEY_X, false}; m_asciiMap['X'] = {KEY_X, true};
    m_asciiMap['y'] = {KEY_Y, false}; m_asciiMap['Y'] = {KEY_Y, true};
    m_asciiMap['z'] = {KEY_Z, false}; m_asciiMap['Z'] = {KEY_Z, true};

    // Numbers
    m_asciiMap['1'] = {KEY_1, false}; m_asciiMap['2'] = {KEY_2, false};
    m_asciiMap['3'] = {KEY_3, false}; m_asciiMap['4'] = {KEY_4, false};
    m_asciiMap['5'] = {KEY_5, false}; m_asciiMap['6'] = {KEY_6, false};
    m_asciiMap['7'] = {KEY_7, false}; m_asciiMap['8'] = {KEY_8, false};
    m_asciiMap['9'] = {KEY_9, false}; m_asciiMap['0'] = {KEY_0, false};

    // Symbols
    m_asciiMap[' ']  = {KEY_SPACE, false};
    m_asciiMap['\n'] = {KEY_ENTER, false};
    m_asciiMap['\t'] = {KEY_TAB, false};
    m_asciiMap['!']  = {KEY_1, true};
    m_asciiMap['@']  = {KEY_2, true};
    m_asciiMap['#']  = {KEY_3, true};
    m_asciiMap['$']  = {KEY_4, true};
    m_asciiMap['%']  = {KEY_5, true};
    m_asciiMap['^']  = {KEY_6, true};
    m_asciiMap['&']  = {KEY_7, true};
    m_asciiMap['*']  = {KEY_8, true};
    m_asciiMap['(']  = {KEY_9, true};
    m_asciiMap[')']  = {KEY_0, true};
    m_asciiMap['-']  = {KEY_MINUS, false};
    m_asciiMap['_']  = {KEY_MINUS, true};
    m_asciiMap['=']  = {KEY_EQUAL, false};
    m_asciiMap['+']  = {KEY_EQUAL, true};
    m_asciiMap['[']  = {KEY_LEFTBRACE, false};
    m_asciiMap['{']  = {KEY_LEFTBRACE, true};
    m_asciiMap[']']  = {KEY_RIGHTBRACE, false};
    m_asciiMap['}']  = {KEY_RIGHTBRACE, true};
    m_asciiMap['\\'] = {KEY_BACKSLASH, false};
    m_asciiMap['|']  = {KEY_BACKSLASH, true};
    m_asciiMap[';']  = {KEY_SEMICOLON, false};
    m_asciiMap[':']  = {KEY_SEMICOLON, true};
    m_asciiMap['\''] = {KEY_APOSTROPHE, false};
    m_asciiMap['"']  = {KEY_APOSTROPHE, true};
    m_asciiMap[',']  = {KEY_COMMA, false};
    m_asciiMap['<']  = {KEY_COMMA, true};
    m_asciiMap['.']  = {KEY_DOT, false};
    m_asciiMap['>']  = {KEY_DOT, true};
    m_asciiMap['/']  = {KEY_SLASH, false};
    m_asciiMap['?']  = {KEY_SLASH, true};
    m_asciiMap['`']  = {KEY_GRAVE, false};
    m_asciiMap['~']  = {KEY_GRAVE, true};
}

} // namespace bybridge
