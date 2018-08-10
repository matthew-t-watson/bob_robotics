// Standard C includes
#include <cmath>
#include <cstdlib>

// Standard C++ includes
#include <algorithm>
#include <vector>

// OpenCV
#include <opencv2/opencv.hpp>

namespace BoBRobotics {
namespace Navigation {
class AbsDiff
{
public:
    AbsDiff(const int size)
      : m_Size(size)
    {}

    inline void calculateDifference(const cv::Mat &image1, const cv::Mat &image2, cv::Mat &differenceImage)
    {
        cv::absdiff(image1, image2, differenceImage);
        m_Ptr = differenceImage.data;
    }

    inline uint8_t *begin()
    {
        return m_Ptr;
    }

    inline uint8_t *end()
    {
        return m_Ptr + m_Size;
    }

    static inline float mean(const float sum, const float n)
    {
        return sum / n;
    }

private:
    const int m_Size;
    uint8_t *m_Ptr;

};

class RMSDiff
{
    public:
    RMSDiff(const int size) : m_Differences(size)
    {
        m_Differences.resize(size);
    }

    void calculateDifference(const cv::Mat &image1, const cv::Mat &image2, cv::Mat &differenceImage)
    {
        cv::absdiff(image1, image2, differenceImage);
        const uint8_t *ptr = differenceImage.data;
        std::transform(ptr, &ptr[image1.rows * image1.cols], m_Differences.begin(), [](const uint8_t diff) {
            const float fdiff = (float) diff;
            return fdiff * fdiff;
        });
    }

    inline auto begin()
    {
        return m_Differences.begin();
    }

    inline auto end()
    {
        return m_Differences.end();
    }

    static inline float mean(const float sum, const float n)
    {
        return sqrt(sum / n);
    }

    private:
    std::vector<float> m_Differences;
};
} // Navigation
} // BoBRobotics
