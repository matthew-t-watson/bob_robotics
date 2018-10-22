
#pragma once

// BoB robotics includes
#include "../robots/tank.h"

// Third-party includes
#include "../third_party/units.h"

// Standard C includes
#include <cmath>

namespace BoBRobotics {
namespace Robots {

using namespace std::literals;
using namespace units::length;
using namespace units::time;
using namespace units::velocity;
using namespace units::literals;
using namespace units::angle;
using namespace units::constants;
using namespace units::dimensionless;
using namespace units::angular_velocity;


class RobotPositioner {

private:
    // Robot variables
    millimeter_t m_pos_X;                                           // robot's x position
    millimeter_t m_pos_Y;                                           // robot's y position
    millimeter_t m_goalPositionX;                                   // goal position x
    millimeter_t m_goalPositionY;                                   // goal position y
    millimeter_t m_distanceFromGoal;                                // Euclidean distance from goal coordinate
    degree_t m_heading;                                             // heading (angle) of the robot
    degree_t m_goalAngle;                                           // angle to turn after finding the correct location
    degree_t m_bearingFromGoal;                                     // bearing (angle) from goal coordinate

    // user variables
    millimeter_t m_stopping_distance;                               // if the robot's distance from goal < stopping dist, robot stops
    degree_t m_allowed_heading_error;                               // the amount of error allowed in the final heading
    meters_per_second_t m_max_velocity;                             // max velocity
    double m_k1;                                                    // curveness of the path to the goal
    double m_k2;                                                    // speed of turning on the curves
    double m_alpha;                                                 // causes more sharply peaked curves 
    double m_beta;                                                  // causes to drop velocity if 'k'(curveness) increases
    
    static degree_t angleWrapAround(degree_t angle) {
      
        if (angle <= 0_deg) { angle += 360_deg; } 
        if (angle > 180_deg) { angle -= 360_deg; }
        
        return angle;
    }


    // updates the range and bearing from the goal location
    void updateRangeAndBearing() {
        
        millimeter_t delta_x = m_goalPositionX - m_pos_X;
        millimeter_t delta_y = m_goalPositionY - m_pos_Y;
    
        // calculate distance
        m_distanceFromGoal =  units::math::hypot(delta_x, delta_y);

        // calculate bearing
        m_bearingFromGoal = units::math::atan2(delta_y,delta_x)-m_heading;
       
        // changing from <0,360> to <-180, 180>
        m_bearingFromGoal = angleWrapAround(m_bearingFromGoal);
        
    } 

//-----------------PUBLIC API---------------------------------------------------------------------
public:

    RobotPositioner(
    
        millimeter_t stopping_distance,                               // if the robot's distance from goal < stopping dist, robot stops
        degree_t allowed_heading_error,                               // the amount of error allowed in the final heading
        double k1,                                                    // curveness of the path to the goal
        double k2,                                                    // speed of turning on the curves
        double alpha,                                                 // causes more sharply peaked curves 
        double beta,                                                  // causes to drop velocity if 'k'(curveness) increases
        meters_per_second_t max_velocity                              // max velocity
        ) : m_stopping_distance(stopping_distance),
            m_allowed_heading_error(allowed_heading_error),
            m_k1(k1),
            m_k2(k2),
            m_alpha(alpha),
            m_beta(beta),
            m_max_velocity(max_velocity) 
    {  }

    //! sets the goal pose (x, y, angle)
    void setGoalPose(millimeter_t pos_x, millimeter_t pos_y, degree_t goal_angle) 
    {
        m_goalPositionX = pos_x;
        m_goalPositionY = pos_y;
        m_goalAngle = goal_angle;
    }


    //! updates the velocities in order to get to a goal location. This function can be used
    //! without a robot interface, where only velocities are calculated but no robot actions
    //! will be executed.
    void updateVelocities(
        const millimeter_t pos_x,         // current X position of robot
        const millimeter_t pos_y,         // current Y position of robot
        const degree_t angle,             // current heading of robot
        meters_per_second_t &v,           // velocity to update
        degrees_per_second_t &w)          // angular velocity to update

    {        

        m_pos_X = pos_x;
        m_pos_Y = pos_y;
        m_heading = angle;
        
        // updates the range and bearing
        updateRangeAndBearing();

        // orientation of Target with respect to the line of sight from the observer to the target
        degree_t theta = m_heading + m_bearingFromGoal - m_goalAngle; 
        theta = angleWrapAround(theta);


        auto k = -(((m_k2 * (m_bearingFromGoal - radian_t(atan( radian_t(-m_k1 * theta).value()) ))))
         + degree_t( 1+(m_k1/(1+pow(m_k1*theta.value(),2))) )
         * units::math::sin(m_bearingFromGoal))/m_distanceFromGoal;
     
        v = m_max_velocity/scalar_t( (1+m_beta*pow(std::abs(k.value()),m_alpha)));
        w = k*v;  
    }

    
    //! This function will update the motors so it drives towards a previously set goal location
    void updateMotors(BoBRobotics::Robots::Tank &bot, 
                      const millimeter_t pos_x,
                      const millimeter_t pos_y,       
                      const degree_t angle)
    {           
        // here we are solving simultaneous equations where we would like to
        // recover Vl (left wheel velocity) and Vr (right wheel velocity) 
        //                                       v = wheel_radius * (Vl+Vr)/2 
        //                                       w = wheel_radius * (Vr-Vl)/axis_length

        meters_per_second_t v;
        degrees_per_second_t w; 
        updateVelocities(pos_x, pos_y, angle, v, w); 

        const millimeter_t robot_wheel_radius = bot.getRobotWheelRadius();
        const millimeter_t robot_axis_length  = bot.getRobotAxisLength();
        
        const double a = (robot_wheel_radius/2).value(); 
        const double b = (robot_wheel_radius/robot_axis_length).value();
    
        auto mmps = 1_mm/1_s;
        mmps = v; // converting to mm/s
        const double c = v.value();
        const double d = w.value();    

        // determinant
        const double det = 2*(-a*b);
    
        const double Vl = ((-c*b - a*d)/det);
        const double Vr = (( a*d - c*b)/det);

        // drive robot 
        bot.tank(Vl, Vr);
    }

    //! returns true if the robot reached the goal position
    bool didReachGoal() 
    {
        return (m_distanceFromGoal < m_stopping_distance  && 
               (m_bearingFromGoal > -m_allowed_heading_error &&
                m_bearingFromGoal < m_allowed_heading_error));     
    }

};// RobotPositioner
} // Robots
} // BoBRobotics
