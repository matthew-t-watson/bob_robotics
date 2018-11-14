// BoB robotics includes
#include "robots/robot_positioner.h"
#include "common/main.h"
#include "hid/joystick.h"
#include "robots/norbot.h"
#include "vicon/udp.h"

// Third-party includes
#include "third_party/units.h"

// Standard C includes
#include <cstdlib>

// Standard C++ includes
#include <chrono>
#include <iostream>
#include <thread>

using namespace BoBRobotics;
using namespace units::length;
using namespace units::literals;
using namespace units::angle;
using namespace units::angular_velocity;
using namespace units::velocity;
using namespace std::literals;

int
bob_main(int, char **)
{
    // Connect to Vicon system
    Vicon::UDPClient<> vicon(51001);
    while (vicon.getNumObjects() == 0) {
        std::this_thread::sleep_for(1s);
        std::cout << "Waiting for object" << std::endl;
    }

    BoBRobotics::Robots::Norbot bot;

    // setup parameters
    constexpr millimeter_t stopping_distance = 10_cm; // if the robot's distance from goal < stopping dist, robot stops
    constexpr degree_t allowed_heading_error = 5_deg; // the amount of error allowed in the final heading
    double k1 = 1.51;                                 // curveness of the path to the goal
    double k2 = 4.4;                                  // speed of turning on the curves
    double alpha = 1.03;                              // causes more sharply peaked curves
    double beta = 0.02;                               // causes to drop velocity if 'k'(curveness) increases
    meters_per_second_t max_velocity{ 0.05 };         // will limit the maximum velocity to this value
    degrees_per_second_t max_turning_velocity{ 13 };

    BoBRobotics::Robots::RobotPositioner robp(
            stopping_distance,
            allowed_heading_error,
            k1,
            k2,
            alpha,
            beta,
            max_velocity,
            max_turning_velocity);

    // set goal pose
    constexpr millimeter_t goalX = 0_mm, goalY = 0_mm;
    constexpr degree_t goalHeading = 15_deg;
    std::cout << "Goal: (" << goalX << ", " << goalY << ") at " << goalHeading << std::endl;
    robp.setGoalPose(goalX, goalY, goalHeading);

    // drive robot with joystick
    HID::Joystick joystick;
    bot.addJoystick(joystick);

    bool runPositioner = false;
    while (!joystick.isPressed(HID::JButton::B)) {
        const bool joystickUpdate = joystick.update();
        if (joystick.isPressed(HID::JButton::Y)) {
            runPositioner = !runPositioner;
            if (runPositioner) {
                std::cout << "Starting positioner" << std::endl;
            } else {
                std::cout << "Stopping positioner" << std::endl;
            }
        }

        if (runPositioner) {
            const auto objectData = vicon.getObjectData(0);
            const Vector3<millimeter_t> position = objectData.getPosition();
            const Vector3<radian_t> attitude = objectData.getAttitude();
            robp.updateMotors(bot, position[0], position[1], attitude[0]);
        } else if (!joystickUpdate) {
            std::this_thread::sleep_for(5ms);
        }
    }

    return EXIT_SUCCESS;
}