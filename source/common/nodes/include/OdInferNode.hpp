#ifndef OD_INFER_NODE_HPP
#define OD_INFER_NODE_HPP

#include <thread>
#include <iostream>
#include <atomic>

#include <inc/api/hvaPipeline.hpp>
// #include <inc/api/hvaBlob.hpp>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <models/detection_model.h>
#include <models/detection_model_centernet.h>
#include <models/detection_model_faceboxes.h>
#include <models/detection_model_retinaface.h>
#include <models/detection_model_retinaface_pt.h>
#include <models/detection_model_ssd.h>
#include <models/detection_model_yolo.h>
#include <models/input_data.h>
#include <models/model_base.h>
#include <models/results.h>

#include <pipelines/async_pipeline.h>
#include <pipelines/metadata.h>

#include <common.hpp>


using ms = std::chrono::milliseconds;

class ODInferNode : public hva::hvaNode_t{
public:
    struct Config{
        std::string architectureType = "";  //centernet, faceboxes, retinaface, retinaface-pytorch, ssd or yolo
        std::string modelFileName = "";
        std::string targetDevice = "CPU";
        std::string labelFilename = "";

        std::string layout = "";  //Optional. Specify inputs layouts. Ex. NCHW or input0:NCHW,input1:NC in case of more than one input.
        
        float confidenceThreshold = 0.5;
        float iouThreshold = 0.5;

        bool autoResize = false;  //Optional. Enables resizable input with support of ROI crop & auto resize.

        uint32_t nireq = 0;  //Optional. Number of infer requests. If this option is omitted, number of infer requests is determined automatically.
        uint32_t nthreads = 0;  //Optional. Number of threads.
        std::string nstreams = "";  //Optional. Number of streams to use for inference on the CPU or/and GPU in throughput mode (for HETERO and MULTI device cases use format <device1>:<nstreams1>,<device2>:<nstreams2> or just <nstreams>).

        bool yolo_af = true;
        std::string anchors = "";
        std::string masks = "";
        bool reverse_input_channels = false;
        std::string mean_values = "";
        std::string scale_values = "";
    };

    ODInferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const Config& config);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    Config m_cfg;
};

class ODInferNodeWorker : public hva::hvaNodeWorker_t{
public:
    ODInferNodeWorker(hva::hvaNode_t* parentNode, const ODInferNode::Config& config);

    virtual void process(std::size_t batchIdx) override;
    virtual void init() override;
    virtual void deinit() override;

    virtual void processByFirstRun(std::size_t batchIdx) override;
    virtual void processByLastRun(std::size_t batchIdx) override;

private:
    std::vector<float> m_anchors;
    std::vector<int64_t> m_masks;
    std::vector<std::string> m_labels;

    std::unique_ptr<ModelBase> m_model;
    std::shared_ptr<AsyncPipeline> m_pipeline;
    //std::unique_ptr<ResultBase> m_result;

    ov::Core m_core;

    std::shared_ptr<std::thread> m_resultThread;
    std::atomic_bool m_exec {true};
    std::mutex m_blobMutex;
    std::queue<std::shared_ptr<hva::hvaBlob_t>> m_inferWaitingQueue;
    int64_t m_frameNum = -1;
};
#endif
