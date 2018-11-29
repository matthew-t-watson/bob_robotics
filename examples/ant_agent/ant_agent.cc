// BoB robotics includes
#include "hid/joystick.h"
#include "libantworld/agent.h"

// OpenCV
#include <opencv2/opencv.hpp>

// Standard C++ includes
#include <chrono>
#include <iostream>
#include <thread>
#include <tuple>

using namespace BoBRobotics;
using namespace std::literals;
using namespace units::length;
using namespace units::angle;

int
main()
{
    const cv::Size RenderSize{ 720, 150 };
    const meter_t AntHeight = 1_cm;

    HID::Joystick joystick;

    auto window = AntWorld::AntAgent::initialiseWindow(RenderSize);

    // Create renderer
    AntWorld::Renderer renderer(256, 0.001, 1000.0, 360_deg);
    auto &world = renderer.getWorld();
    world.load("../../libantworld/world5000_gray.bin",
               { 0.0f, 1.0f, 0.0f },
               { 0.898f, 0.718f, 0.353f });
    const auto minBound = world.getMinBound();
    const auto maxBound = world.getMaxBound();

    // Create agent and put in the centre of the world
    AntWorld::AntAgent agent(window.get(), renderer, RenderSize);
    agent.setPosition((maxBound[0] - minBound[0]) / 2, (maxBound[1] - minBound[1]) / 2, AntHeight);

    // Control the agent with a joystick
    agent.addJoystick(joystick);

    std::cout << "Press the B button to quit" << std::endl;
    std::tuple<meter_t, meter_t, degree_t> lastPose;
    while (!glfwWindowShouldClose(window.get()) && !joystick.isDown(HID::JButton::B)) {
        joystick.update();

        const auto position = agent.getPosition<>();
        const Vector3<degree_t> attitude = agent.getAttitude<>();
        auto pose = std::make_tuple(position[0], position[1], attitude[0]);
        if (pose != lastPose) {
            std::cout << "Pose: " << position[0] << ", " << position[1] << ", " << attitude[0] << std::endl;
            lastPose = pose;
        }

        // Clear colour and depth buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render first person
        renderer.renderPanoramicView(position[0], position[1], position[2],
                                     attitude[0], attitude[1], attitude[2],
                                     0, 0, RenderSize.width, RenderSize.height);

        // Swap front and back buffers
        glfwSwapBuffers(window.get());
    }
}