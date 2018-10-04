#pragma once

// Standard C includes
#include <cmath>

// Standard C++ includes
#include <algorithm>
#include <iostream>
#include <random>
#include <tuple>
#include <vector>

// Eigen
#include <Eigen/Core>

// OpenCV
#include <opencv2/opencv.hpp>

// Third-party includes
#include "../third_party/units.h"

// Local includes
#include "insilico_rotater.h"
#include "visual_navigation_base.h"

namespace BoBRobotics {
namespace Navigation {
using namespace Eigen;
using namespace units::angle;

//------------------------------------------------------------------------
// BoBRobotics::Navigation::InfoMax
//------------------------------------------------------------------------
template<typename FloatType = float>
class InfoMax : public VisualNavigationBase
{
    using MatrixType = Matrix<FloatType, Dynamic, Dynamic>;
    using VectorType = Matrix<FloatType, Dynamic, 1>;

public:
    InfoMax(const cv::Size &unwrapRes,
            const MatrixType &initialWeights,
            FloatType learningRate = 0.0001)
      : VisualNavigationBase(unwrapRes)
      , m_LearningRate(learningRate)
      , m_Weights(initialWeights)
    {}

    InfoMax(const cv::Size &unwrapRes, FloatType learningRate = 0.0001)
      : VisualNavigationBase(unwrapRes)
      , m_LearningRate(learningRate)
      , m_Weights(getInitialWeights(unwrapRes.width * unwrapRes.height,
                                    1 + unwrapRes.width * unwrapRes.height))
    {}

    //------------------------------------------------------------------------
    // VisualNavigationBase virtuals
    //------------------------------------------------------------------------
    virtual void train(const cv::Mat &image) override
    {
        VectorType u, y;
        std::tie(u, y) = getUY(image);
        trainUY(u, y);
    }

    virtual float test(const cv::Mat &image) const override
    {
        const auto decs = m_Weights * getFloatVector(image);
        return decs.array().abs().sum();
    }

    //------------------------------------------------------------------------
    // Public API
    //------------------------------------------------------------------------
    const MatrixType &getWeights() const
    {
        return m_Weights;
    }

#ifndef EXPOSE_INFOMAX_INTERNALS
    private:
#endif
    void trainUY(const VectorType &u, const VectorType &y)
    {
        // weights = weights + lrate/N * (eye(H)-(y+u)*u') * weights;
        const auto id = MatrixType::Identity(m_Weights.rows(), m_Weights.rows());
        const auto sumYU = (y.array() + u.array()).matrix();
        const FloatType learnRate = m_LearningRate / (FloatType) u.rows();
        m_Weights.array() += (learnRate * (id - sumYU * u.transpose()) * m_Weights).array();
    }

    auto getUY(const cv::Mat &image)
    {
        assert(image.type() == CV_8UC1);

        const cv::Size &unwrapRes = getUnwrapResolution();
        assert(image.cols == unwrapRes.width);
        assert(image.rows == unwrapRes.height);

        // Convert image to vector of floats
        const auto u = m_Weights * getFloatVector(image);
        const auto y = tanh(u.array());

        return std::make_pair(std::move(u), std::move(y));
    }

    static MatrixType getInitialWeights(const int numInputs,
                                        const int numHidden,
                                        const unsigned seed = std::random_device()())
    {
        MatrixType weights(numInputs, numHidden);

        std::cout << "Seed for weights is: " << seed << std::endl;
        
        std::default_random_engine generator(seed);
        std::normal_distribution<FloatType> distribution;
        for (int i = 0; i < numInputs; i++) {
            for (int j = 0; j < numHidden; j++) {
                weights(i, j) = distribution(generator);
            }
        }

        // std::cout << "Initial weights" << std::endl
        //           << weights << std::endl;

        // Normalise mean and SD for row so mean == 0 and SD == 1
        const auto means = weights.rowwise().mean();
        // std::cout << "Means" << std::endl
        //           << means << std::endl;

        weights.colwise() -= means;
        // std::cout << "Weights after subtracting means" << std::endl << weights << std::endl;

        // const auto newmeans = weights.rowwise().mean();
        // std::cout << "New means" << std::endl
        //           << newmeans << std::endl;

        const auto sd = matrixSD(weights);
        // std::cout << "SD" << std::endl
        //           << sd << std::endl;

        weights = weights.array().colwise() / sd;
        // std::cout << "Weights after dividing by SD" << std::endl
        //           << weights << std::endl;

        // const auto newsd = matrixSD(weights);
        // std::cout << "New SD" << std::endl
        //           << newsd << std::endl;

        return weights.transpose();
    }

private:
    size_t m_SnapshotCount = 0;
    FloatType m_LearningRate;
    MatrixType m_Weights;

    static auto getFloatVector(const cv::Mat &image)
    {
        Map<Matrix<uint8_t, Dynamic, 1>> map(image.data, image.cols * image.rows);
        return map.cast<FloatType>() / 255.0;
    }

    template<class T>
    static auto matrixSD(const T &mat)
    {
        return (mat.array() * mat.array()).rowwise().mean();
    }
}; // InfoMax

//------------------------------------------------------------------------
// BoBRobotics::Navigation::InfoMaxRotater
//------------------------------------------------------------------------
template<typename Rotater = InSilicoRotater, typename FloatType = float>
class InfoMaxRotater : public InfoMax<FloatType>
{
    using MatrixType = Matrix<FloatType, Dynamic, Dynamic>;
public:
    InfoMaxRotater(const cv::Size &unwrapRes,
                   const MatrixType &initialWeights,
                   FloatType learningRate = 0.0001)
    :   InfoMax<FloatType>(unwrapRes, initialWeights, learningRate)
    {}

    InfoMaxRotater(const cv::Size &unwrapRes, FloatType learningRate = 0.0001)
    :   InfoMax<FloatType>(unwrapRes, learningRate)
    {}

    //------------------------------------------------------------------------
    // Public API
    //------------------------------------------------------------------------
    template<class... Ts>
    auto getHeading(Ts &&... args) const
    {
        Rotater rotater(this->getUnwrapResolution(), this->getMaskImage(), std::forward<Ts>(args)...);
        std::vector<FloatType> outputs;
        outputs.reserve(rotater.max());
        rotater.rotate([this, &outputs] (const cv::Mat &image, auto, auto) {
            outputs.push_back(this->test(image));
        });

        const auto el = std::min_element(outputs.begin(), outputs.end());
        size_t bestIndex = std::distance(outputs.begin(), el);
        if (bestIndex > outputs.size() / 2) {
            bestIndex -= outputs.size();
        }
        const radian_t heading = units::make_unit<turn_t>((double) bestIndex / (double) outputs.size());

        return std::make_tuple(heading, *el, std::move(outputs));
    }
};
} // Navigation
} // BoBRobotics
