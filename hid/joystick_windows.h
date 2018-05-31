#pragma once

#include "../os/windows_include.h"

#include <xinput.h>
#pragma comment(lib, "XInput.lib")

#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <thread>

namespace GeNNRobotics {
namespace HID {
enum class Button
{
    A = XINPUT_GAMEPAD_A,
    B = XINPUT_GAMEPAD_B,
    X = XINPUT_GAMEPAD_X,
    Y = XINPUT_GAMEPAD_Y,
    LB = XINPUT_GAMEPAD_LEFT_SHOULDER,
    RB = XINPUT_GAMEPAD_RIGHT_SHOULDER,
    Back = XINPUT_GAMEPAD_BACK,
    Start = XINPUT_GAMEPAD_START,
    LeftStick = XINPUT_GAMEPAD_LEFT_THUMB,
    RightStick = XINPUT_GAMEPAD_RIGHT_THUMB,
    Left = XINPUT_GAMEPAD_DPAD_LEFT,
    Right = XINPUT_GAMEPAD_DPAD_RIGHT,
    Up = XINPUT_GAMEPAD_DPAD_UP,
    Down = XINPUT_GAMEPAD_DPAD_DOWN,
    NOTBUTTON
};
} // HID
} // GeNNRobotics

#include "joystick_base.h"

namespace GeNNRobotics {
namespace HID {
class Joystick : public JoystickBase
{
private:
    XINPUT_STATE _controllerState;
    int _controllerNum = 0;
    unsigned int pressed = 0;
    int lThumbXState1 = 0;
    int lThumbYState1 = 0;
    int rThumbXState1 = 0;
    int rThumbYState1 = 0;

    bool Change();
    void open();    

public:
    Joystick();
    Joystick(const JoystickHandler handler);
    XINPUT_STATE Read();
    bool read(Event &js) override;
};

Joystick::Joystick()
{
    open();
}

Joystick::Joystick(const JoystickHandler handler) : JoystickBase(handler)
{
    open();
}

void
Joystick::open()
{
    // Zeroise the state
    ZeroMemory(&_controllerState, sizeof(XINPUT_STATE));

    // Get the state
    DWORD Result = XInputGetState(_controllerNum, &_controllerState);
    if (Result != ERROR_SUCCESS) {
        throw std::runtime_error("Could not open joystick");        
    }
}

XINPUT_STATE
Joystick::Read()
{
    // Zeroise the state
    ZeroMemory(&_controllerState, sizeof(XINPUT_STATE));

    // Get the state
    XInputGetState(_controllerNum, &_controllerState);

    return _controllerState;
}

bool
Joystick::Change()
{
    // Zeroise the state
    ZeroMemory(&_controllerState, sizeof(XINPUT_STATE));

    // Check for any changes in the controller
    int state1 = Read().dwPacketNumber;
    while (true) {
        int state2 = Read().dwPacketNumber;
        if (state1 != state2) {
            state1 = state2;
            return true;
        }
    }
}

// read the buttons on the controller and report which button(s) are
// pressed/unpressed
bool
Joystick::read(Event &js)
{
    while (Change()) {
        unsigned int buttState = Read().Gamepad.wButtons;
        if (~pressed & buttState) {
            js.number = ~pressed & buttState;
            js.value = true;
            js.isAxis = false;
            pressed = buttState;
            break;
        } else {
            if (pressed & ~buttState) {
                js.number = pressed & ~buttState;
                js.value = false;
                js.isAxis = false;
                pressed = buttState;
                break;
            } else {
                int lTrigState = Read().Gamepad.bLeftTrigger;
                int rTrigState = Read().Gamepad.bRightTrigger;
                int lThumbYState2 = Read().Gamepad.sThumbLX;
                int lThumbXState2 = -Read().Gamepad.sThumbLY;
                int rThumbXState2 = Read().Gamepad.sThumbRX;
                int rThumbYState2 = -Read().Gamepad.sThumbRY;
                if (lThumbXState2 != lThumbXState1) {
                    js.number = static_cast<unsigned int>(Axis::LeftStickHorizontal);
                    js.value = lThumbXState2;
                    js.isAxis = true;
                    lThumbXState1 = lThumbXState2;
                    break;
                }
                if (lThumbYState2 != lThumbYState1) {
                    js.number = static_cast<unsigned int>(Axis::LeftStickVertical);
                    js.value = lThumbYState2;
                    js.isAxis = true;
                    lThumbYState1 = lThumbYState2;
                    break;
                }
                if (rThumbXState2 != rThumbXState1) {
                    js.number = static_cast<unsigned int>(Axis::RightStickHorizontal);
                    js.value = rThumbXState2;
                    js.isAxis = true;
                    rThumbXState1 = rThumbXState2;
                    break;
                }
                if (rThumbYState2 != rThumbYState1) {
                    js.number = static_cast<unsigned int>(Axis::RightStickVertical);
                    js.value = rThumbYState2;
                    js.isAxis = true;
                    rThumbYState1 = rThumbYState2;
                    break;
                }
                if (lTrigState >= 1) {
                    js.number = static_cast<unsigned int>(Axis::LeftTrigger);
                    js.value = lTrigState;
                    js.isAxis = true;
                    break;
                }
                if (rTrigState >= 1) {
                    js.number = static_cast<unsigned int>(Axis::RightTrigger);
                    js.value = rTrigState;
                    js.isAxis = true;
                    break;
                } else {
                    if (js.number == static_cast<unsigned int>(Axis::LeftTrigger) && lTrigState < 1) {
                        js.value = 0;
                        break;
                    }
                    if (js.number == static_cast<unsigned int>(Axis::RightTrigger) && rTrigState < 1) {
                        js.value = 0;
                        break;
                    }
                }
            }
        }
    }
    return true;
}
} // HID
} // GeNNRobotics
