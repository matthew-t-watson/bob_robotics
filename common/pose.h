#pragma once

// Third-party includes
#include "../third_party/units.h"

// Standard C++ includes
#include <array>
#include <tuple>

namespace BoBRobotics {

//! A generic template for 2D unit arrays
template<typename T>
using Vector2 = std::array<T, 2>;

//! A generic template for 3D unit arrays
template<typename T>
using Vector3 = std::array<T, 3>;

template<typename LengthUnit, size_t N>
class PositionBase
{
    static_assert(units::traits::is_length_unit<LengthUnit>::value,
                  "LengthUnit is not a unit of length");

public:
    PositionBase() = default;

    template<typename... Ts>
    PositionBase(Ts &&... args)
      : m_Array({ std::forward<Ts>(args)... })
    {}

    operator const std::array<LengthUnit, N> &() const
    {
        return m_Array;
    }

    LengthUnit &operator[](size_t i) { return m_Array[i]; }
    const LengthUnit &operator[](size_t i) const { return m_Array[i]; }
    static constexpr size_t size() { return N; }

    auto begin() { return m_Array.begin(); }
    auto begin() const { return m_Array.begin(); }
    auto end() { return m_Array.end(); }
    auto end() const { return m_Array.end(); }
    auto cbegin() const { return m_Array.cbegin(); }
    auto cend() const { return m_Array.end(); }

private:
    std::array<LengthUnit, N> m_Array;
};

template<typename LengthUnit>
class Position2
  : public PositionBase<LengthUnit, 2>
{
public:
    Position2() = default;

    Position2(LengthUnit x, LengthUnit y)
      : PositionBase<LengthUnit, 2>(x, y)
    {}

    Position2(const std::array<LengthUnit, 2> &array)
      : Position2(array[0], array[1])
    {}

    LengthUnit &x() { return (*this)[0]; }
    const LengthUnit &x() const { return (*this)[0]; }
    LengthUnit &y() { return (*this)[1]; }
    const LengthUnit &y() const { return (*this)[1]; }
    static constexpr LengthUnit z() { return LengthUnit(0); }
};

template<typename LengthUnit>
class Position3
  : public PositionBase<LengthUnit, 3>
{
public:
    Position3() = default;

    Position3(LengthUnit x, LengthUnit y, LengthUnit z)
      : PositionBase<LengthUnit, 3>(x, y, z)
    {}

    Position3(const std::array<LengthUnit, 3> &array)
      : Position3(array[0], array[1], array[2])
    {}

    LengthUnit &x() { return (*this)[0]; }
    const LengthUnit &x() const { return (*this)[0]; }
    LengthUnit &y() { return (*this)[1]; }
    const LengthUnit &y() const { return (*this)[1]; }
    LengthUnit &z() { return (*this)[2]; }
    const LengthUnit &z() const { return (*this)[2]; }
};

template<typename LengthUnit, typename AngleUnit>
class Pose3;

//! A two-dimensional pose
template<typename LengthUnit, typename AngleUnit>
class Pose2
  : public std::tuple<Position2<LengthUnit>, AngleUnit>
{
    static_assert(units::traits::is_length_unit<LengthUnit>::value,
                  "LengthUnit is not a unit of length");
    static_assert(units::traits::is_angle_unit<AngleUnit>::value,
                  "AngleUnit is not a unit of angle");

public:
    Pose2() = default;

    Pose2(LengthUnit x, LengthUnit y, AngleUnit angle)
      : std::tuple<Position2<LengthUnit>, AngleUnit>({ x, y }, angle)
    {}

    template<typename LengthUnit2, typename AngleUnit2>
    operator Pose2<LengthUnit2, AngleUnit2>() const
    {
        return Pose2<LengthUnit2, AngleUnit2>{ x(), y(), yaw() };
    }

    template<typename LengthUnit2, typename AngleUnit2>
    operator Pose3<LengthUnit2, AngleUnit2>() const
    {
        return Pose3<LengthUnit2, AngleUnit2>{ { x(), y(), z() }, { yaw(), pitch(), roll() } };
    }

    Position2<LengthUnit> &position() { return std::get<0>(*this); }
    const Position2<LengthUnit> &position() const { return std::get<0>(*this); }
    LengthUnit &x() { return std::get<0>(*this)[0]; }
    const LengthUnit &x() const { return std::get<0>(*this)[0]; }
    LengthUnit &y() { return std::get<0>(*this)[1]; }
    const LengthUnit &y() const { return std::get<0>(*this)[1]; }
    static constexpr LengthUnit z() { return LengthUnit(0); }

    Vector3<AngleUnit> attitude() const { return { yaw(), AngleUnit(0), AngleUnit(0) }; }
    AngleUnit &yaw() { return std::get<1>(*this); }
    const AngleUnit &yaw() const { return std::get<1>(*this); }
    static constexpr AngleUnit pitch() { return AngleUnit(0); }
    static constexpr AngleUnit roll() { return AngleUnit(0); }
};

//! A three-dimensional pose
template<typename LengthUnit, typename AngleUnit>
class Pose3
  : public std::tuple<Position3<LengthUnit>, Vector3<AngleUnit>>
{
    static_assert(units::traits::is_length_unit<LengthUnit>::value,
                  "LengthUnit is not a unit of length");
    static_assert(units::traits::is_angle_unit<AngleUnit>::value,
                  "AngleUnit is not a unit of angle");

public:
    Pose3() = default;

    Pose3(const Position3<LengthUnit> &position, const Vector3<AngleUnit> &attitude)
      : std::tuple<Position3<LengthUnit>, Vector3<AngleUnit>>(position, attitude)
    {}

    template<typename LengthUnit2, typename AngleUnit2>
    operator Pose2<LengthUnit2, AngleUnit2>() const
    {
        return Pose2<LengthUnit2, AngleUnit2>{ x(), y(), yaw() };
    }

    template<typename LengthUnit2, typename AngleUnit2>
    operator Pose3<LengthUnit2, AngleUnit2>() const
    {
        return Pose3<LengthUnit2, AngleUnit2>{ { x(), y(), z() }, { yaw(), pitch(), roll() } };
    }

    Position3<LengthUnit> &position() { return std::get<0>(*this); }
    const Position3<LengthUnit> &position() const { return std::get<0>(*this); }
    LengthUnit &x() { return std::get<0>(*this)[0]; }
    const LengthUnit &x() const { return std::get<0>(*this)[0]; }
    LengthUnit &y() { return std::get<0>(*this)[1]; }
    const LengthUnit &y() const { return std::get<0>(*this)[1]; }
    LengthUnit &z() { return std::get<0>(*this)[2]; }
    const LengthUnit &z() const { return std::get<0>(*this)[2]; }

    Vector3<AngleUnit> &attitude() { return std::get<1>(*this); }
    const Vector3<AngleUnit> &attitude() const { return std::get<1>(*this); }
    AngleUnit &yaw() { return std::get<1>(*this)[0]; }
    const AngleUnit &yaw() const { return std::get<1>(*this)[0]; }
    AngleUnit &pitch() { return std::get<1>(*this)[1]; }
    const AngleUnit &pitch() const { return std::get<1>(*this)[1]; }
    AngleUnit &roll() { return std::get<1>(*this)[2]; }
    const AngleUnit &roll() const { return std::get<1>(*this)[2]; }
};

//! Converts the input array to a unit-type of OutputUnit
template<typename OutputUnit, typename ArrayType>
inline constexpr Vector3<OutputUnit>
convertUnitArray(const ArrayType &values)
{
    return { static_cast<OutputUnit>(values[0]),
             static_cast<OutputUnit>(values[1]),
             static_cast<OutputUnit>(values[2]) };
}
} // BoBRobotics
