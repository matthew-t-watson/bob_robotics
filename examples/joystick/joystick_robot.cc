// BoB robotics includes
#include "hid/joystick.h"
#include "robots/robot_type.h"

using namespace BoBRobotics;

int main()
{
    constexpr float joystickDeadzone = 0.25f;

    // Create joystick interface
    HID::Joystick joystick(joystickDeadzone);

    // Create motor interface
    Robots::ROBOT_TYPE robot;

    do {
        // Read joystick
        joystick.update();

        // Use joystick to drive motor
        robot.drive(joystick);

    } while(!joystick.isDown(HID::JButton::B));

    return 0;
}
