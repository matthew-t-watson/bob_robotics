// BoB robotics includes
#include "libantworld/agent.h"
#include "libantworld/route_continuous.h"
#include "navigation/image_database.h"
#include "navigation/perfect_memory.h"

// Third-party includes
#include "third_party/path.h"
#include "third_party/units.h"

// GLFW
#include <GLFW/glfw3.h>

// OpenCV
#include <opencv2/opencv.hpp>

// Standard C++ includes
#include <iostream>
#include <tuple>
#include <vector>

using namespace BoBRobotics;
using namespace units::angle;
using namespace units::length;
using namespace units::literals;

int main(int, char **argv)
{
    const filesystem::path routePath = filesystem::path(argv[0]).parent_path() / ".." / ".." / "libantworld" / "ant1_route1.bin";
    const cv::Size renderSize{ 360, 75 };
    const auto window = AntWorld::AntAgent::initialiseWindow(renderSize);
    constexpr meter_t routeSep = 1_m;
    constexpr meter_t height = 10_cm;

    AntWorld::Renderer renderer(256, 0.001, 1000.0, 360_deg);
    const auto path = (filesystem::path(argv[0]).parent_path() / ".." / ".." / "libantworld" / "world5000_gray.bin").str();
    std::cout << "Path: " << path << std::endl;
    auto &world = renderer.getWorld();
    world.load(path,
                             { 0.0f, 1.0f, 0.0f },
                             { 0.898f, 0.718f, 0.353f });
    AntWorld::AntAgent agent(window.get(), renderer, renderSize.width, renderSize.height);
    AntWorld::RouteContinuous route(0.2f, 800, routePath.str());

    Navigation::PerfectMemoryRotater<> pm(renderSize);
    pm.trainRoute("route");

    std::cout << "Doing testing..." << std::endl;
    cv::Mat fr;
    cv::FileStorage fs("logfile.yaml", cv::FileStorage::WRITE);
    fs << "data"
       << "[";
    for (meter_t dist = 5_cm; dist < route.getLength() && !glfwWindowShouldClose(window.get()); dist += routeSep) {
        meter_t x, y;
        degree_t theta;
        std::tie(x, y, theta) = route.getPosition(dist);
        agent.setPosition(x, y, height);
        agent.setAttitude(theta, 0_deg, 0_deg);
        agent.readGreyscaleFrame(fr);

        fs << pm.getHeading(fr);
    }
    fs << "]";
}
