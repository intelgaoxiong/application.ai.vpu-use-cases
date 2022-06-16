#ifndef DISPLAY_NODE_HPP
#define DISPLAY_NODE_HPP

#include <thread>
#include <iostream>
#include <atomic>

#include <inc/api/hvaPipeline.hpp>
// #include <inc/api/hvaBlob.hpp>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

using ms = std::chrono::milliseconds;

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
};
#endif
