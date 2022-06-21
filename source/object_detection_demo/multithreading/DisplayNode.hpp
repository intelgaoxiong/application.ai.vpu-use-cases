#ifndef DISPLAY_NODE_HPP
#define DISPLAY_NODE_HPP

#include <thread>
#include <iostream>
#include <atomic>
#include <random>

#include <inc/api/hvaPipeline.hpp>
// #include <inc/api/hvaBlob.hpp>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <utils/ocv_common.hpp>

#include <OdInferNode.hpp>

using ms = std::chrono::milliseconds;

class ColorPalette {
private:
    std::vector<cv::Scalar> palette;

    static double getRandom(double a = 0.0, double b = 1.0) {
        static std::default_random_engine e;
        std::uniform_real_distribution<> dis(a, std::nextafter(b, std::numeric_limits<double>::max()));
        return dis(e);
    }

    static double distance(const cv::Scalar& c1, const cv::Scalar& c2) {
        auto dh = std::fmin(std::fabs(c1[0] - c2[0]), 1 - fabs(c1[0] - c2[0])) * 2;
        auto ds = std::fabs(c1[1] - c2[1]);
        auto dv = std::fabs(c1[2] - c2[2]);

        return dh * dh + ds * ds + dv * dv;
    }

    static cv::Scalar maxMinDistance(const std::vector<cv::Scalar>& colorSet,
                                     const std::vector<cv::Scalar>& colorCandidates) {
        std::vector<double> distances;
        distances.reserve(colorCandidates.size());
        for (auto& c1 : colorCandidates) {
            auto min =
                *std::min_element(colorSet.begin(), colorSet.end(), [&c1](const cv::Scalar& a, const cv::Scalar& b) {
                    return distance(c1, a) < distance(c1, b);
                });
            distances.push_back(distance(c1, min));
        }
        auto max = std::max_element(distances.begin(), distances.end());
        return colorCandidates[std::distance(distances.begin(), max)];
    }

    static cv::Scalar hsv2rgb(const cv::Scalar& hsvColor) {
        cv::Mat rgb;
        cv::Mat hsv(1, 1, CV_8UC3, hsvColor);
        cv::cvtColor(hsv, rgb, cv::COLOR_HSV2RGB);
        return cv::Scalar(rgb.data[0], rgb.data[1], rgb.data[2]);
    }

public:
    explicit ColorPalette(size_t n) {
        palette.reserve(n);
        std::vector<cv::Scalar> hsvColors(1, {1., 1., 1.});
        std::vector<cv::Scalar> colorCandidates;
        size_t numCandidates = 100;

        hsvColors.reserve(n);
        colorCandidates.resize(numCandidates);
        for (size_t i = 1; i < n; ++i) {
            std::generate(colorCandidates.begin(), colorCandidates.end(), []() {
                return cv::Scalar{getRandom(), getRandom(0.8, 1.0), getRandom(0.5, 1.0)};
            });
            hsvColors.push_back(maxMinDistance(hsvColors, colorCandidates));
        }

        for (auto& hsv : hsvColors) {
            // Convert to OpenCV HSV format
            hsv[0] *= 179;
            hsv[1] *= 255;
            hsv[2] *= 255;

            palette.push_back(hsv2rgb(hsv));
        }
    }

    const cv::Scalar& operator[](size_t index) const {
        return palette[index % palette.size()];
    }
};

class DisplayNode : public hva::hvaNode_t{
public:
    struct Config{
        unsigned reserved1;
        unsigned reserved2;
    };

    DisplayNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const Config& config);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    unsigned m_reserved1;
    unsigned m_reserved2;
};

class DisplayNodeWorker : public hva::hvaNodeWorker_t{
public:
    DisplayNodeWorker(hva::hvaNode_t* parentNode, unsigned reserved1, unsigned reserved2);

    virtual void process(std::size_t batchIdx) override;
    virtual void init() override;
    virtual void deinit() override;

    virtual void processByFirstRun(std::size_t batchIdx) override;
    virtual void processByLastRun(std::size_t batchIdx) override;

private:
    unsigned m_reserved1;
    unsigned m_reserved2;

    cv::Mat curr_frame;
    std::shared_ptr<ColorPalette> m_palettePtr;
    std::shared_ptr<OutputTransform> m_outputTransform;

    cv::Mat renderDetectionData(DetectionResult& result, const ColorPalette& palette, OutputTransform& outputTransform);
};
#endif
