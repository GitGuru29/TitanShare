#pragma once
/*
 * TitanShare Daemon — Virtual Input Device
 * Creates uinput mouse + keyboard devices.
 * Direct C++ port of the original input_driver.c
 */

#include <string>

namespace titanshare {

class VirtualInput {
public:
    VirtualInput();
    ~VirtualInput();

    // Initialize uinput devices. Returns false if /dev/uinput is inaccessible.
    bool init();

    // Mouse operations
    void moveMouse(int dx, int dy);
    void scrollWheel(int dy);
    void clickMouse(const std::string& button); // "LEFT" or "RIGHT"
    void mouseDown(const std::string& button);
    void mouseUp(const std::string& button);

    // Keyboard operations
    void typeText(const std::string& text);
    void pressKey(const std::string& keyName);

private:
    void emit(int fd, int type, int code, int value);
    void emitSyn(int fd);
    void typeKeyCode(int code, bool shift);
    int getButtonCode(const std::string& button);
    int getKeyCodeFromName(const std::string& name);

    int m_mouseFd = -1;
    int m_kbFd = -1;
    bool m_initialized = false;

    // ASCII → keycode mapping
    struct KeyMap { int code; bool shift; };
    KeyMap m_asciiMap[128] = {};
    void initAsciiMap();

    int createMouse();
    int createKeyboard();
};

} // namespace titanshare
