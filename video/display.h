/*
 * This is a simple class for displaying a VideoInput (e.g. a webcam) on screen.
 * An example of its use is given in simpledisplay_test.cc (build with make).
 * The user can quit by pressing escape.
 *
 * You can optionally run the display on a separate thread by invoking the
 * startThread() method.
 *
 * Should be built with OpenCV and -pthread.
 */

// C++ includes
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

// OpenCV
#include <opencv2/opencv.hpp>

// BoB robotics includes
#include "../common/threadable.h"
#include "../imgproc/opencv_unwrap_360.h"
#include "../os/keycodes.h"

// local includes
#include "input.h"

namespace BoBRobotics {
namespace Video {
class Display : public Threadable
{
#define WINDOW_NAME "OpenCV display"

public:
    /*
     * Create a new display with unwrapping disabled.
     */
    Display(Input &videoInput)
      : m_VideoInput(&videoInput)
    {}

    virtual ~Display()
    {
        close();
    }

    /*
     * Create a new display with unwrapping enabled if the Video::Input supports
     * it.
     */
    template<typename... Ts>
    Display(Input &videoInput, Ts &&... unwrapRes)
      : m_VideoInput(&videoInput)
    {
        if (videoInput.needsUnwrapping()) {
            m_ShowUnwrapped = true;
            auto unwrapper = videoInput.createUnwrapper(std::forward<Ts>(unwrapRes)...);
            m_Unwrapper.reset(new ImgProc::OpenCVUnwrap360(std::move(unwrapper)));
        }
    }

    /*
     * Create a new display from a std::unique_ptr to Input (e.g. from
     * getPanoramicCamera()).
     */
    template<typename... Ts>
    Display(std::unique_ptr<Input> &videoInput, Ts &&... unwrapRes)
      : Display(*videoInput.get(), std::forward<Ts>(unwrapRes)...)
    {}

    bool isOpen() const
    {
        return m_Open;
    }

    /*
     * Run the display on the main thread.
     */
    void run() override
    {
        while (m_DoRun) {
            // poll the camera until we get a new frame
            while (!update()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        }
    }

    bool update()
    {
        if (!m_Open) {
            // set opencv window to display full screen
            cv::namedWindow(WINDOW_NAME, CV_WINDOW_NORMAL);
            setWindowProperty(WINDOW_NAME, cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);
            m_Open = true;
        }

        cv::Mat frame;
        bool newFrame = readFrame(frame);
        if (newFrame) {
            // display the frame on screen
            cv::imshow(WINDOW_NAME, frame);
        }

        // get keyboard input
        switch (cv::waitKeyEx(1)) {
        case 'u': // toggle unwrapping
            m_ShowUnwrapped = !m_ShowUnwrapped;
            break;
        case OS::KeyCodes::Escape:
            close();
        }

        return newFrame;
    }

    virtual void close()
    {
        if (m_Open) {
            cv::destroyWindow(WINDOW_NAME);
            m_DoRun = false;
            m_Open = false;
        }
    }

protected:
    bool m_Open = false;
    cv::Mat m_Frame, m_Unwrapped;
    bool m_ShowUnwrapped = false;
    std::unique_ptr<ImgProc::OpenCVUnwrap360> m_Unwrapper;
    Input *m_VideoInput;

    virtual bool readFrame(cv::Mat &frame)
    {
        if (!m_VideoInput->readFrame(m_Frame)) {
            return false;
        }
        
        // unwrap frame if required
        if (m_Unwrapper && m_ShowUnwrapped) {
            m_Unwrapper->unwrap(m_Frame, m_Unwrapped);
            frame = m_Unwrapped;
        } else {
            frame = m_Frame;
        }
        return true;
    }
}; // Display
} // Video
} // BoBRobotics