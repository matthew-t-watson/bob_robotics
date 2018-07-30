#include "bebop.h"

/*
 * Little macros to make using the ARSDK's C API less verbose.
 */
#define DRONE_COMMAND(COMMAND, ARG) \
    checkError(m_Device->aRDrone3->COMMAND(m_Device->aRDrone3, ARG));

#define DRONE_COMMAND_NO_ARG(COMMAND) \
    checkError(m_Device->aRDrone3->COMMAND(m_Device->aRDrone3));

/*
 * For when a maximum speed parameter has changed
 */
#define MAX_SPEED_CHANGED(NAME, KEY)                                         \
    {                                                                        \
        bebop->m_##NAME##Limits.onChanged(                                   \
                dict,                                                        \
                ARCONTROLLER_DICTIONARY_KEY_ARDRONE3_##KEY##CHANGED_CURRENT, \
                ARCONTROLLER_DICTIONARY_KEY_ARDRONE3_##KEY##CHANGED_MIN,     \
                ARCONTROLLER_DICTIONARY_KEY_ARDRONE3_##KEY##CHANGED_MAX);    \
    }

/*
 * Throw a runtime_error if discovery device fails.
 */
inline void
checkError(eARDISCOVERY_ERROR err)
{
    if (err != ARDISCOVERY_OK) {
        throw std::runtime_error(std::string("Discovery error: ") +
                                 ARDISCOVERY_Error_ToString(err));
    }
}

namespace BoBRobotics {
namespace Robots {

/** BEGIN PUBLIC MEMBERS **/

// We also have to give declarations for these variables, because c++ is weird
constexpr degree_t Bebop::DefaultMaximumTilt;
constexpr degrees_per_second_t Bebop::DefaultMaximumYawSpeed;
constexpr meters_per_second_t Bebop::DefaultMaximumVerticalSpeed;

/*!
 * \brief Search for the drone on the network and connect to it.
 */
Bebop::Bebop(degrees_per_second_t maxYawSpeed,
             meters_per_second_t maxVerticalSpeed,
             degree_t maxTilt)
{
    // silence annoying messages printed by library
    ARSAL_Print_SetCallback(printCallback);

    // object to interface with drone
    createControllerDevice();

    // to handle changes in state, incoming commands
    addEventHandlers();

    // initialise video stream object
    m_VideoStream = std::make_unique<VideoStream>(*this);

    // store speed limits for later
    m_YawSpeedLimits.m_UserMaximum = maxYawSpeed;
    m_VerticalSpeedLimits.m_UserMaximum = maxVerticalSpeed;
    m_TiltLimits.m_UserMaximum = maxTilt;

    // connect to drone
    connect();
}

/*!
 * \brief Land the drone and disconnect properly when the object is destroyed.
 */
Bebop::~Bebop()
{
    land();
    stopStreaming();
    disconnect();
}

/*!
 * \brief Start controlling this drone with a joystick.
 */
void
Bebop::addJoystick(HID::Joystick &joystick)
{
    joystick.addHandler([this](HID::JAxis axis, float value) {
        return onAxisEvent(axis, value);
    });
    joystick.addHandler([this](HID::JButton button, bool pressed) {
        return onButtonEvent(button, pressed);
    });
}

/*!
 * \brief Send take-off command.
 */
void
Bebop::takeOff()
{
    std::cout << "Taking off..." << std::endl;
    DRONE_COMMAND_NO_ARG(sendPilotingTakeOff);

    if (m_FlightEventHandler) {
        m_FlightEventHandler(true);
    }
}

/*!
 * \brief Send land command.
 */
void
Bebop::land()
{
    std::cout << "Landing..." << std::endl;
    DRONE_COMMAND_NO_ARG(sendPilotingLanding);

    if (m_FlightEventHandler) {
        m_FlightEventHandler(false);
    }
}

/*!
 * \brief Return the an object allowing access to the drone's onboard camera.
 */
Bebop::VideoStream &
Bebop::getVideoStream()
{
    startStreaming();
    return *m_VideoStream;
}

/*!
 * \brief Return the current maximum tilt setting.
 * 
 * This affects pitch and roll.
 */
degree_t
Bebop::getMaximumTilt() const
{
    return m_TiltLimits.getCurrent();
}

/*!
 * \brief Return the minimum and maximum permitted values for the maximum tilt
 *        setting.
 */
Limits<degree_t> &
Bebop::getTiltLimits()
{
    return m_TiltLimits.getLimits();
}

/*!
 * \brief Return the current maximum vertical speed setting.
 */
meters_per_second_t
Bebop::getMaximumVerticalSpeed() const
{
    return m_VerticalSpeedLimits.getCurrent();
}

/*!
 * \brief Return the minimum and maximum permitted values for the maximum
 *        vertical speed setting.
 */
Limits<meters_per_second_t> &
Bebop::getVerticalSpeedLimits()
{
    return m_VerticalSpeedLimits.getLimits();
}

/*!
 * \brief Return the current maximum yaw speed setting.
 */
degrees_per_second_t
Bebop::getMaximumYawSpeed() const
{
    return m_YawSpeedLimits.getCurrent();
}

/*!
 * \brief Return the minimum and maximum permitted values for the maximum yaw
 *        speed setting.
 */
Limits<degrees_per_second_t> &
Bebop::getYawSpeedLimits()
{
    return m_YawSpeedLimits.getLimits();
}

/*!
 * \brief Set drone's pitch, for moving forwards and backwards.
 */
void
Bebop::setPitch(float pitch)
{
    pitch = std::min(1.0f, std::max(-1.0f, pitch)); // cap value
    DRONE_COMMAND(setPilotingPCMDPitch, round(pitch * 100.0f));
    DRONE_COMMAND(setPilotingPCMDFlag, 1);
}

/*!
 * \brief Set drone's roll, for banking left and right.
 */
void
Bebop::setRoll(float right)
{
    right = std::min(1.0f, std::max(-1.0f, right)); // cap value
    DRONE_COMMAND(setPilotingPCMDRoll, round(right * 100.0f));
    DRONE_COMMAND(setPilotingPCMDFlag, 1);
}

/*!
 * \brief Set drone's vertical speed for ascending/descending.
 */
void
Bebop::setVerticalSpeed(float up)
{
    up = std::min(1.0f, std::max(-1.0f, up)); // cap value
    DRONE_COMMAND(setPilotingPCMDGaz, round(up * 100.0f));
}

/*!
 * \brief Set drone's yaw speed.
 */
void
Bebop::setYawSpeed(float right)
{
    right = std::min(1.0f, std::max(-1.0f, right)); // cap value
    DRONE_COMMAND(setPilotingPCMDYaw, round(right * 100.0f));
}

/*!
 * \brief Stop the drone from moving along all axes.
 */
void
Bebop::stopMoving()
{
    setPitch(0);
    setRoll(0);
    setYawSpeed(0);
    setVerticalSpeed(0);
}

/*!
 * \brief Tell the drone to take a photo and store it.
 */
void
Bebop::takePhoto()
{
    DRONE_COMMAND(sendMediaRecordPicture, 0);
}

/*!
 * \brief Add an event handler for when the drone is taking off or landing.
 */
void
Bebop::setFlightEventHandler(FlightEventHandler handler)
{
    m_FlightEventHandler = handler;
}

/** END PUBLIC MEMBERS **/
/** BEGIN PRIVATE MEMBERS **/

/*
 * Try to make connection to drone.
 */
void
Bebop::connect()
{
    // send start signal
    checkError(ARCONTROLLER_Device_Start(m_Device.get()));

    // wait for start
    eARCONTROLLER_DEVICE_STATE state;
    do {
        state = getStateUpdate();
    } while (state == ARCONTROLLER_DEVICE_STATE_STARTING);

    if (state != ARCONTROLLER_DEVICE_STATE_RUNNING) {
        throw std::runtime_error("Could not connect to drone");
    }

    // set speed limits
    setMaximumYawSpeed(m_YawSpeedLimits.m_UserMaximum);
    setMaximumVerticalSpeed(m_VerticalSpeedLimits.m_UserMaximum);
    setMaximumTilt(m_TiltLimits.m_UserMaximum);
}

/*
 * Try to disconnect from drone.
 */
void
Bebop::disconnect()
{
    if (m_Device) {
        // send stop signal
        checkError(ARCONTROLLER_Device_Stop(m_Device.get()));

        // wait for stop
        eARCONTROLLER_DEVICE_STATE state;
        do {
            state = getStateUpdate();
        } while (state == ARCONTROLLER_DEVICE_STATE_STOPPING);

        if (state != ARCONTROLLER_DEVICE_STATE_STOPPED) {
            std::cerr << "Warning: Could not disconnect from drone" << std::endl;
        }
    }
}

/*
 * Create the struct used by the ARSDK to interface with the drone.
 */
inline void
Bebop::createControllerDevice()
{
    // create discovery device
    static const auto deleter = [](ARDISCOVERY_Device_t *discover) {
        ARDISCOVERY_Device_Delete(&discover);
    };
    auto derr = ARDISCOVERY_OK;
    const std::unique_ptr<ARDISCOVERY_Device_t, decltype(deleter)> discover(ARDISCOVERY_Device_New(&derr), deleter);
    checkError(derr);

    // try to discover device on network
    checkError(ARDISCOVERY_Device_InitWifi(discover.get(),
                                           ARDISCOVERY_PRODUCT_BEBOP_2,
                                           "bebop",
                                           BEBOP_IP_ADDRESS,
                                           BEBOP_DISCOVERY_PORT));

    // create controller object
    auto err = ARCONTROLLER_OK;
    m_Device = ControllerPtr(ARCONTROLLER_Device_New(discover.get(), &err),
                             [](ARCONTROLLER_Device_t *dev) {
                                 ARCONTROLLER_Device_Delete(&dev);
                             });
    checkError(err);
}

/*
 * Add callbacks for when connection state changes or a command is received
 * from the drone.
 */
inline void
Bebop::addEventHandlers()
{
    checkError(ARCONTROLLER_Device_AddStateChangedCallback(
            m_Device.get(), stateChanged, this));
    checkError(ARCONTROLLER_Device_AddCommandReceivedCallback(
            m_Device.get(), commandReceived, this));
}

inline void
Bebop::setMaximumTilt(degree_t newValue)
{
    DRONE_COMMAND(sendPilotingSettingsMaxTilt, newValue.value());
}

inline void
Bebop::setMaximumVerticalSpeed(meters_per_second_t newValue)
{
    DRONE_COMMAND(sendSpeedSettingsMaxVerticalSpeed, newValue.value());
}

inline void
Bebop::setMaximumYawSpeed(degrees_per_second_t newValue)
{
    DRONE_COMMAND(sendSpeedSettingsMaxRotationSpeed, newValue.value());
}

/*
 * Send the command to start video streaming.
 */
void
Bebop::startStreaming()
{
    DRONE_COMMAND(sendMediaStreamingVideoEnable, 1);
}

/*
 * Send the command to stop video streaming.
 */
void
Bebop::stopStreaming()
{
    DRONE_COMMAND(sendMediaStreamingVideoEnable, 0);
}

/*
 * Wait for the drone to send a state-update command and return the new
 * state.
 */
inline eARCONTROLLER_DEVICE_STATE
Bebop::getStateUpdate()
{
    m_Semaphore.wait();
    return getState();
}

/*
 * Return the drone's connection state.
 */
inline eARCONTROLLER_DEVICE_STATE
Bebop::getState()
{
    auto err = ARCONTROLLER_OK;
    const auto state = ARCONTROLLER_Device_GetState(m_Device.get(), &err);
    checkError(err);
    return state;
}

/*
 * Invoked by commandReceived().
 *
 * Prints the battery state whenever it changes.
 */
inline void
Bebop::onBatteryChanged(const ARCONTROLLER_DICTIONARY_ELEMENT_t *dict) const
{
    // which command was received?
    ARCONTROLLER_DICTIONARY_ELEMENT_t *elem = nullptr;
    HASH_FIND_STR(dict, ARCONTROLLER_DICTIONARY_SINGLE_KEY, elem);
    if (!elem) {
        return;
    }

    // get associated value
    ARCONTROLLER_DICTIONARY_ARG_t *val = nullptr;
    HASH_FIND_STR(
            elem->arguments,
            ARCONTROLLER_DICTIONARY_KEY_COMMON_COMMONSTATE_BATTERYSTATECHANGED_PERCENT,
            val);

    if (val) {
        // print battery status
        std::cout << "Battery: " << (int) val->value.U8 << "%" << std::endl;
    }
}

/*
 * Empty function used to suppress default ARSDK console messages.
 */
int
Bebop::printCallback(eARSAL_PRINT_LEVEL level,
                     const char *tag,
                     const char *format,
                     va_list va)
{
    // do nothing
    return 0;
}

/*
 * Invoked when the drone's connection state changes.
 */
void
Bebop::stateChanged(eARCONTROLLER_DEVICE_STATE newstate,
                    eARCONTROLLER_ERROR err,
                    void *data)
{
    Bebop *bebop = reinterpret_cast<Bebop *>(data);
    bebop->m_Semaphore.notify(); // trigger semaphore used by get_state_update()

    switch (newstate) {
    case ARCONTROLLER_DEVICE_STATE_STOPPED:
        std::cout << "Drone stopped" << std::endl;
        break;
    case ARCONTROLLER_DEVICE_STATE_STARTING:
        std::cout << "Drone starting..." << std::endl;
        break;
    case ARCONTROLLER_DEVICE_STATE_RUNNING:
        std::cout << "Drone started" << std::endl;
        break;
    case ARCONTROLLER_DEVICE_STATE_PAUSED:
        std::cout << "Drone is paused" << std::endl;
        break;
    case ARCONTROLLER_DEVICE_STATE_STOPPING:
        std::cout << "Drone stopping..." << std::endl;
        break;
    default:
        std::cerr << "Unknown state!" << std::endl;
    }
}

/*
 * Invoked when a command is received from drone.
 */
void
Bebop::commandReceived(eARCONTROLLER_DICTIONARY_KEY key,
                       ARCONTROLLER_DICTIONARY_ELEMENT_t *dict,
                       void *data)
{
    if (!dict) {
        return;
    }

    Bebop *bebop = reinterpret_cast<Bebop *>(data);
    switch (key) {
    case ARCONTROLLER_DICTIONARY_KEY_COMMON_COMMONSTATE_BATTERYSTATECHANGED:
        bebop->onBatteryChanged(dict);
        break;
    case ARCONTROLLER_DICTIONARY_KEY_ARDRONE3_PILOTINGSETTINGSSTATE_MAXTILTCHANGED:
        MAX_SPEED_CHANGED(Tilt, PILOTINGSETTINGSSTATE_MAXTILT);
        break;
    case ARCONTROLLER_DICTIONARY_KEY_ARDRONE3_SPEEDSETTINGSSTATE_MAXROTATIONSPEEDCHANGED:
        MAX_SPEED_CHANGED(YawSpeed, SPEEDSETTINGSSTATE_MAXROTATIONSPEED);
        break;
    case ARCONTROLLER_DICTIONARY_KEY_ARDRONE3_SPEEDSETTINGSSTATE_MAXVERTICALSPEEDCHANGED:
        MAX_SPEED_CHANGED(VerticalSpeed, SPEEDSETTINGSSTATE_MAXVERTICALSPEED);
        break;
    default:
        break;
    }
}

/*
 * Handle joystick axis events.
 */
bool
Bebop::onAxisEvent(HID::JAxis axis, float value)
{
    /* 
     * setRoll/Pitch etc. all take values between -1 and 1. We cap these 
     * values for the joystick code to make the drone more controllable. 
     */
    switch (axis) {
    case HID::JAxis::RightStickHorizontal:
        setRoll(value);
        return true;
    case HID::JAxis::RightStickVertical:
        setPitch(-value);
        return true;
    case HID::JAxis::LeftStickVertical:
        setVerticalSpeed(-value);
        return true;
    case HID::JAxis::LeftTrigger:
        setYawSpeed(-value);
        return true;
    case HID::JAxis::RightTrigger:
        setYawSpeed(value);
        return true;
    default:
        // otherwise signal that we haven't handled event
        return false;
    }
}

/*
 * Handle joystick button events.
 */
bool
Bebop::onButtonEvent(HID::JButton button, bool pressed)
{
    // we only care about button presses
    if (!pressed) {
        return false;
    }

    // A = take off; B = land
    switch (button) {
    case HID::JButton::A:
        takeOff();
        return true;
    case HID::JButton::B:
        land();
        return true;
    default:
        // otherwise signal that we haven't handled event
        return false;
    }
}

/** END PRIVATE MEMBERS **/
} // Robots
} // BoBRobotics
