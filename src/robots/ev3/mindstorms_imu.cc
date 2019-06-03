// BoB robotics includes
#include "common/circstat.h"
#include "robots/ev3/mindstorms_imu.h"

using namespace units::angle;
using namespace units::angular_velocity;

namespace BoBRobotics {
MindstormsIMU::MindstormsIMU(const ev3dev::address_type &address)
  : m_IMU(address)
{}

degree_t
MindstormsIMU::getYaw()
{
    const degree_t yaw{ static_cast<UNIT_LIB_DEFAULT_TYPE>(m_IMU.angle()) };
    return normaliseAngle180(yaw);
}

degrees_per_second_t
MindstormsIMU::getYawVelocity()
{
    return degrees_per_second_t{ static_cast<UNIT_LIB_DEFAULT_TYPE>(m_IMU.rate()) };
}

std::pair<degree_t, degrees_per_second_t>
MindstormsIMU::getYawAndVelocity()
{
    int angle, rate;
    std::tie(angle, rate) = m_IMU.rate_and_angle();
    degree_t angleDeg{ static_cast<UNIT_LIB_DEFAULT_TYPE>(angle) };
    return std::pair<degree_t, degrees_per_second_t>{ normaliseAngle180(angleDeg), rate };
}

} // BoBRobotics
