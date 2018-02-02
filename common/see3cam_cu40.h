#pragma once

// OpenCV includes
#include <opencv2/imgproc/imgproc.hpp>

// Common includes
#include "opencv_unwrap_360.h"
#include "v4l_camera.h"

/* NEON version of captureSuperPixel - fun but doesn't actually help performance
// Create tables to use for shuffling BGIR data into BGR
//                              b0, g0, r0| b1, g1, r1| b2, g2
const uint8x8_t blockShuffleA = {0, 1,  9,  2,  3,  11, 4,  5};
//                              r2| b3, g3, r3|
const uint8x8_t blockShuffleB = {13,6,  7,  15};

// Loop through input pixels in 8x2 blocks i.e. 4 Bayer pixels
for(unsigned int y = 0; y < inputHeight; y += 2) {
    // Get pointers to start of both rows of Bayer data and output RGB data
    const uint16_t *inBG16Start = &bayerData[y * inputWidth];
    const uint16_t *inIR16Start = &bayerData[(y + 1) * inputWidth];
    uint8_t *outRGBStart = output.ptr(y / 2);
    
    for(unsigned int x = 0; x < inputWidth; x += 8) {
        // Load 8x2 block into two 128-bit registers and advance each pointer to next block
        // b0, g0 | b1, g1 | b2, g2 | b3, g3 |
        uint16x8_t inBG16 = vld1q_u16(inBG16Start += 8);
        // i0, r0 | i1, r1 | i2, r2 | i3, r3 |
        uint16x8_t inIR16 = vld1q_u16(inIR16Start += 8);
        
        // Shift each 10-bit value right by 2
        inBG16 = vshrq_n_u16(inBG16, 2);
        inIR16 = vshrq_n_u16(inIR16, 2);
        
        // Convert to 8 bits and stack into a pair of 64-bit registers
        uint8x8x2_t outBGIR;
        outBGIR.val[0] = vmovn_u16(inBG16);
        outBGIR.val[1] = vmovn_u16(inIR16);
        
        // Use tables to shuffle each 64-bit register worth of Bayer data into RGB
        uint8x8_t outA = vtbl2_u8(outBGIR, blockShuffleA);
        uint8x8_t outB = vtbl2_u8(outBGIR, blockShuffleB);
        
        // Combine the two shuffled vectors together into one 128-bit register full of RGB data
        uint8x16_t out8 = vcombine_u8(outA, outB);
        
        // Write this back to output array and advance pointer
        //if(y != (inputHeight-1) || x != (inputWidth - 1)) {
            vst1q_u8(outRGBStart += (4 * 3), out8);
        //}
    }
}*/

//------------------------------------------------------------------------
// See3CAM_CU40
//------------------------------------------------------------------------
class See3CAM_CU40 : public Video4LinuxCamera
{
public:
    enum class Resolution : uint64_t
    {
        _672x380    = (672ULL | ( 380ULL << 32)),
        _1280x720   = (1280ULL | (720ULL << 32)),
        _1920x1080  = (1920ULL | (1080ULL << 32)),
        _2688x1520  = (2688ULL | (1520ULL << 32)),
    };

    See3CAM_CU40()
    {
    }

    See3CAM_CU40(const std::string &device, Resolution res, bool resetToDefaults = true)
    {
        if(!open(device, res, resetToDefaults)) {
            throw std::runtime_error("Cannot open See3CAM_CU40");
        }
    }

    //------------------------------------------------------------------------
    // Public API
    //------------------------------------------------------------------------
    bool open(const std::string &device, Resolution res, bool resetToDefaults = true)
    {
        // Cache resolution
        m_Resolution = res;

        // Create additional frame to hold demosaiced frames
        m_Demosaiced16.create(getHeight(), getWidth(), CV_16UC3);

        // If camera was opened successfully
        if(Video4LinuxCamera::open(device, getWidth(), getHeight(), V4L2_PIX_FMT_Y16))
        {
            // Query camera controls
            if(!queryControl(V4L2_CID_BRIGHTNESS, m_BrightnessControl)
                || !queryControl(V4L2_CID_EXPOSURE_ABSOLUTE, m_ExposureControl))
            {
                return false;
            }

            // If we should reset camera to default settings, do so
            if(resetToDefaults) {
                if(!setControlValue(V4L2_CID_BRIGHTNESS, m_BrightnessControl.default_value)
                    || !setControlValue(V4L2_CID_EXPOSURE_ABSOLUTE, m_ExposureControl.default_value))
                {
                    return false;
                }
            }

            return true;
        }
        else {
            return false;
        }
    }

    bool captureSuperPixel(cv::Mat &output)
    {
        return captureSuperPixel<PixelScale>(output);
    }

    bool captureSuperPixelClamp(cv::Mat &output)
    {
        return captureSuperPixel<PixelClamp>(output);
    }

    bool captureSuperPixelWBCoolWhite(cv::Mat &output)
    {
        return captureSuperPixel<WhiteBalanceCoolWhite>(output);
    }

    bool captureSuperPixelWBU30(cv::Mat &output)
    {
        return captureSuperPixel<WhiteBalanceU30>(output);
    }

    bool captureSuperPixelGreyscale(cv::Mat &output)
    {
        // Check that output size is suitable for super-pixel output i.e. a quarter input size
        const unsigned int inputWidth = getWidth();
        const unsigned int inputHeight = getHeight();
        assert(output.cols == inputWidth / 2);
        assert(output.rows == inputHeight / 2);
        assert(output.type() == CV_8UC1);

        // Read data and size (in bytes) from camera
        // **NOTE** these pointers are only valid within one frame
        void *data = nullptr;
        uint32_t sizeBytes = 0;
        if(Video4LinuxCamera::capture(data, sizeBytes)) {
            // Check frame size is correct
            assert(sizeBytes == (inputWidth * inputHeight * sizeof(uint16_t)));
            const uint16_t *bayerData = reinterpret_cast<uint16_t*>(data);

            // Loop through bayer pixels
            for(unsigned int y = 0; y < inputHeight; y += 2) {
                // Get pointers to start of both rows of Bayer data and output RGB data
                const uint16_t *inBG16Start = &bayerData[y * inputWidth];
                const uint16_t *inR16Start = &bayerData[((y + 1) * inputWidth) + 1];
                uint8_t *outStart = output.ptr(y / 2);
                for(unsigned int x = 0; x < inputWidth; x += 2) {
                    // Read Bayer pixels
                    const uint16_t b = *(inBG16Start++);
                    const uint16_t g = *(inBG16Start++);
                    const uint16_t r = *inR16Start;
                    inR16Start += 2;

                    // Add channels together and divide by 3 to take average and
                    // 4 to rescale from 10-bit per-channel to 8-bit
                    const uint32_t gray = (b + g + r) / (3 * 4);

                    // Write back to BGR
                    *(outStart++) = (uint8_t)gray;
                }
            }

            return true;
        }
        else {
            return false;
        }
    }

    bool setBrightness(int32_t brightness)
    {
        return setControlValue(V4L2_CID_BRIGHTNESS, std::max(m_BrightnessControl.minimum, std::min(m_BrightnessControl.maximum, brightness)));
    }

    bool setExposure(int32_t exposure)
    {
        return setControlValue(V4L2_CID_EXPOSURE_ABSOLUTE, std::max(m_ExposureControl.minimum, std::min(m_ExposureControl.maximum, exposure)));
    }

    unsigned int getWidth() const
    {
        return (static_cast<uint64_t>(m_Resolution) & 0xFFFFFFFF);
    }

    unsigned int getHeight() const
    {
        return ((static_cast<uint64_t>(m_Resolution) >> 32) & 0xFFFFFFFF);
    }
    
    cv::Size getSize() const{ return cv::Size(getWidth(), getHeight()); }

    unsigned int getSuperPixelWidth() const{ return getWidth() / 2; }
    unsigned int getSuperPixelHeight() const{ return getHeight() / 2; }
    cv::Size getSuperPixelSize() const{ return cv::Size(getSuperPixelWidth(), getSuperPixelHeight()); }
    
    //------------------------------------------------------------------------
    // Static API
    //------------------------------------------------------------------------
    static OpenCVUnwrap360 createUnwrapper(const cv::Size &camRes, const cv::Size &unwrapRes)
    {
        return OpenCVUnwrap360(camRes, unwrapRes,
                               0.5, 0.461111, 0.183333, 0.4, 1.570796327, true);
    }

private:
    //------------------------------------------------------------------------
    // PixelScale
    //------------------------------------------------------------------------
    // Converts 10-bit intensity values to 8-bit by dividing by 4
    class PixelScale
    {
    public:
        static uint8_t getR(uint16_t r, uint16_t, uint16_t){ return getScaled(r); }
        static uint8_t getG(uint16_t, uint16_t g, uint16_t){ return getScaled(g); }
        static uint8_t getB(uint16_t, uint16_t, uint16_t b){ return getScaled(b); }

    private:
        static uint8_t getScaled(uint16_t v)
        {
            return (uint8_t)(v >> 2);
        }
    };

    //------------------------------------------------------------------------
    // PixelClamp
    //------------------------------------------------------------------------
    // Converts 10-bit intensity values to 8-bit by clamping at 255
    // **NOTE** this is dubious but a)Is what the qtcam example does and b)Can LOOK nicer than PixelScale
    class PixelClamp
    {
    public:
        static uint8_t getR(uint16_t r, uint16_t, uint16_t){ return getClamped(r); }
        static uint8_t getG(uint16_t, uint16_t g, uint16_t){ return getClamped(g); }
        static uint8_t getB(uint16_t, uint16_t, uint16_t b){ return getClamped(b); }

    private:
        static uint8_t getClamped(uint16_t v)
        {
            return (uint8_t)std::min<uint16_t>(255, v);
        }
    };

    //------------------------------------------------------------------------
    // WhiteBalanceCoolWhite
    //------------------------------------------------------------------------
    class WhiteBalanceCoolWhite
    {
    public:
        static uint8_t getR(uint16_t r, uint16_t, uint16_t)
        {
            // 0.96 (15729)
            return (uint8_t)(((uint32_t)r * 15729) >> 16);
        }

        static uint8_t getG(uint16_t, uint16_t g, uint16_t)
        {
            return (uint8_t)(g >> 2);
        }

        static uint8_t getB(uint16_t, uint16_t, uint16_t b)
        {
            // 1.74 (28508)
            return (uint8_t)std::min<uint32_t>(((uint32_t)b * 28508) >> 16, 255);
        }
    };

    //------------------------------------------------------------------------
    // WhiteBalanceU30
    //------------------------------------------------------------------------
    class WhiteBalanceU30
    {
    public:
        static uint8_t getR(uint16_t r, uint16_t, uint16_t)
        {
            // 0.92 (15073)
            return (uint8_t)(((uint32_t)r * 15073) >> 16);
        }

        static uint8_t getG(uint16_t, uint16_t g, uint16_t)
        {
            return (uint8_t)(g >> 2);
        }

        static uint8_t getB(uint16_t, uint16_t, uint16_t b)
        {
            // 1.53 (25068)
            return (uint8_t)std::min<uint32_t>(((uint32_t)b * 25068) >> 16, 255);
        }
    };

    //------------------------------------------------------------------------
    // Private API
    //------------------------------------------------------------------------
    template<typename T>
    bool captureSuperPixel(cv::Mat &output)
    {
        // Check that output size is suitable for super-pixel output i.e. a quarter input size
        const unsigned int inputWidth = getWidth();
        const unsigned int inputHeight = getHeight();
        assert(output.cols == inputWidth / 2);
        assert(output.rows == inputHeight / 2);
        assert(output.type() == CV_8UC3);

        // Read data and size (in bytes) from camera
        // **NOTE** these pointers are only valid within one frame
        void *data = nullptr;
        uint32_t sizeBytes = 0;
        if(Video4LinuxCamera::capture(data, sizeBytes)) {
            // Check frame size is correct
            assert(sizeBytes == (inputWidth * inputHeight * sizeof(uint16_t)));
            const uint16_t *bayerData = reinterpret_cast<uint16_t*>(data);

            // Loop through bayer pixels
            for(unsigned int y = 0; y < inputHeight; y += 2) {
                // Get pointers to start of both rows of Bayer data and output RGB data
                const uint16_t *inBG16Start = &bayerData[y * inputWidth];
                const uint16_t *inR16Start = &bayerData[((y + 1) * inputWidth) + 1];
                uint8_t *outRGBStart = output.ptr(y / 2);
                for(unsigned int x = 0; x < inputWidth; x += 2) {
                    // Read Bayer pixels
                    const uint16_t b = *(inBG16Start++);
                    const uint16_t g = *(inBG16Start++);
                    const uint16_t r = *inR16Start;
                    inR16Start += 2;

                    // Write back to BGR
                    *(outRGBStart++) = T::getB(r, g, b);
                    *(outRGBStart++) = T::getG(r, g, b);
                    *(outRGBStart++) = T::getR(r, g, b);
                }
            }

            return true;
        }
        else {
            return false;
        }
    }

    //------------------------------------------------------------------------
    // Members
    //------------------------------------------------------------------------
    Resolution m_Resolution;
    v4l2_queryctrl m_BrightnessControl;
    v4l2_queryctrl m_ExposureControl;

    cv::Mat m_Demosaiced16;
};
