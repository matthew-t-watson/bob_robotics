#pragma once

namespace GeNNRobotics {
namespace Robots {
//----------------------------------------------------------------------------
// Motor
//----------------------------------------------------------------------------
// Interface for driving tank-like wheeled robots
class Motor
{
public:
    virtual ~Motor()
    {}

    //----------------------------------------------------------------------------
    // Declared virtuals
    //----------------------------------------------------------------------------
    virtual void tank(float left, float right) = 0;
}; // Motor
} // Robots
} // GeNNRobotics
