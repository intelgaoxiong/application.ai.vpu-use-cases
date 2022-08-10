#ifndef MIDAS_INFER_NODE_HPP
#define MIDAS_INFER_NODE_HPP

#include <thread>
#include <iostream>
#include <atomic>
#include <condition_variable>
#include <queue>

#include <inc/api/hvaPipeline.hpp>
// #include <inc/api/hvaBlob.hpp>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <inference_engine.hpp>

#include <utils/performance_metrics.hpp>

namespace IE = InferenceEngine;

//#include <models/detection_model.h>
//#include <models/detection_model_centernet.h>
//#include <models/detection_model_faceboxes.h>
//#include <models/detection_model_retinaface.h>
//#include <models/detection_model_retinaface_pt.h>
//#include <models/detection_model_ssd.h>
//#include <models/detection_model_yolo.h>
//#include <models/input_data.h>
//#include <models/model_base.h>
//#include <models/results.h>

//#include <pipelines/async_pipeline.h>
//#include <pipelines/metadata.h>

#include <common.hpp>


using ms = std::chrono::milliseconds;

class MidasInferNode : public hva::hvaNode_t{
public:
    struct Config{
        std::string modelFileName = "";
        std::string targetDevice = "VPUX";

        std::string inferMode = "async";  //Optional. async or sync inference.

        std::string layout = "";  //Optional. Specify inputs layouts. Ex. NCHW or input0:NCHW,input1:NC in case of more than one input.

        bool autoResize = false;  //Optional. Enables resizable input with support of ROI crop & auto resize.

        uint32_t nireq = 0;  //Optional. Number of infer requests. If this option is omitted, number of infer requests is determined automatically.
        uint32_t nthreads = 0;  //Optional. Number of threads.
        std::string nstreams = "";  //Optional. Number of streams to use for inference on the CPU or/and GPU in throughput mode (for HETERO and MULTI device cases use format <device1>:<nstreams1>,<device2>:<nstreams2> or just <nstreams>).

        bool reverse_input_channels = false;
        std::string mean_values = "";
        std::string scale_values = "";
        uint32_t nn_width = 0;  //Todo: parsed from model
        uint32_t nn_height = 0; //Todo: parsed from model
    };

    MidasInferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const Config& config);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    Config m_cfg;
};

 /**
 * Infer request pool
 */
class InferRequestPool_t
{
public:
    InferRequestPool_t() = delete;

    /**
     * @brief Constructor of infer request pool
     * @param executableNetwork IE executable network
     * @param numInferRequest Infer request pool size
     */
    explicit InferRequestPool_t(IE::ExecutableNetwork &executableNetwork, int32_t numInferRequest);
    ~InferRequestPool_t();

    InferRequestPool_t(const InferRequestPool_t &) = delete;
    InferRequestPool_t &operator=(const InferRequestPool_t &) = delete;

private:
    using Type = IE::InferRequest::Ptr;

public:
    /**
     * @brief Get idle infer request pointer
     * @return Infer request pointer
     */
    Type get();

    /**
     * @brief Put back used infer request pointer
     * @param ptrInferRequest Infer Request pointer
     */
    void put(const Type ptrInferRequest);

private:
    bool push(Type value);

    bool pop(Type &value);

    bool empty() const;

    void close();

private:
    mutable std::mutex _mutex;
    std::queue<Type> _queue;
    std::condition_variable _cv_empty;
    std::condition_variable _cv_full;
    std::size_t _maxSize; //max queue size
    bool _close{false};

private:
    std::vector<Type> _vecInferRequest;

    int32_t _cntInferRequest{0};

public:
    using Ptr = std::shared_ptr<InferRequestPool_t>;
};

class MidasInferNodeWorker : public hva::hvaNodeWorker_t{
public:
    MidasInferNodeWorker(hva::hvaNode_t* parentNode, const MidasInferNode::Config& config);

    virtual void process(std::size_t batchIdx) override;
    virtual void init() override;
    virtual void deinit() override;

    virtual void processByFirstRun(std::size_t batchIdx) override;
    virtual void processByLastRun(std::size_t batchIdx) override;

    /**
     * @brief Get idle infer request
     * @return Pointer to infer request
     */
    IE::InferRequest::Ptr getInferRequest();

    /**
     * @brief Put back used infer request
     * @param ptrInferRequest Pointer to infer request
     */
    void putInferRequest(IE::InferRequest::Ptr ptrInferRequest);

    void preprocess(const cv::Mat & img, IE::InferRequest::Ptr& request);
    cv::Mat postprocess_fp16(IE::InferRequest::Ptr& request);
    void logLatencyPerStage(double preprocLat, double inferLat, double postprocLat);

private:
    IE::Core m_ie;
    IE::ExecutableNetwork m_executableNetwork;
    InferRequestPool_t::Ptr m_ptrInferRequestPool;
    std::string m_inputName;

    std::shared_ptr<std::thread> m_resultThread;
    std::atomic_bool m_exec {true};
    std::mutex m_blobMutex;
    std::queue<std::shared_ptr<hva::hvaBlob_t>> m_inferWaitingQueue;
    int64_t m_frameNum = -1;
    uint32_t nn_width = 0;  //Todo: parsed from model
    uint32_t nn_height = 0; //Todo: parsed from model

    uint32_t async_infer = 0;

    PerformanceMetrics inferenceMetrics;
    PerformanceMetrics preprocessMetrics;
    PerformanceMetrics postprocessMetrics;
};
#endif
