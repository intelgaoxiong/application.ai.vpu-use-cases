#pragma once
#ifndef VIS3D_NODE_HPP
#define VIS3D_NODE_HPP

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
#include <utils/performance_metrics.hpp>
#include <common.hpp>
#include "open3d/Open3D.h"

class Vis3DNode : public hva::hvaNode_t {
public:
    struct Config {
        std::string dispRes;
    };

    Vis3DNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const Config& config);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    Config m_cfg;
};

class Vis3DNodeWorker : public hva::hvaNodeWorker_t {
public:
    Vis3DNodeWorker(hva::hvaNode_t* parentNode, const Vis3DNode::Config& config);

    virtual void process(std::size_t batchIdx) override;
    virtual void init() override;
    virtual void deinit() override;

    virtual void processByFirstRun(std::size_t batchIdx) override;
    virtual void processByLastRun(std::size_t batchIdx) override;

private:
    cv::Size m_displayResolution;
    open3d::visualization::Visualizer m_vis;
    std::shared_ptr<open3d::geometry::PointCloud> m_pcd;

    cv::Mat curr_frame;
    std::shared_ptr<PerformanceMetrics> m_metrics;
};

#endif // VIS3D_NODE_HPP