cmake_minimum_required(VERSION 3.1)
include(../../cmake/bob_robotics.cmake)
BoB_project(SOURCES uav_navigation.cc
            BOB_MODULES hid navigation robots/bebop)
