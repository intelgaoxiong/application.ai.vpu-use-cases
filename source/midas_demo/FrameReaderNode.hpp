#ifndef FRAME_READER_NODE_HPP
#define FRAME_READER_NODE_HPP

#include <thread>
#include <iostream>
#include <atomic>

#include <inc/api/hvaPipeline.hpp>
// #include <inc/api/hvaBlob.hpp>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <utils/images_capture.h>

using ms = std::chrono::milliseconds;

#define hvaEvent_EOF 0x3ull

class FrameReaderNode : public hva::hvaNode_t{
public:
    struct Config{
        std::string input;
        bool infiniteLoop;
        read_type readType;  //read_type::efficient or read_type::safe
    };

    FrameReaderNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const Config& config);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    Config m_cfg;
    mutable std::atomic<unsigned> m_workerIdx;
};

class FrameReaderNodeWorker : public hva::hvaNodeWorker_t{
public:
    FrameReaderNodeWorker(hva::hvaNode_t* parentNode, int streamId, const FrameReaderNode::Config& config);

    /**
    * @brief Main frame decoding component
    * 
    * @param batchIdx batch index assigned by framework
    * @return void
    * 
    */
    virtual void process(std::size_t batchIdx) override;
    virtual void init() override;
    virtual void deinit() override;

    virtual void processByFirstRun(std::size_t batchIdx) override;
    virtual void processByLastRun(std::size_t batchIdx) override;

private:
    int m_streamId;
    int m_frame_index = 0;

    std::string m_input;
    bool m_loop;
    read_type m_rt;  //read_type::efficient or read_type::safe

    std::unique_ptr<ImagesCapture> m_cap;

    std::atomic_uint m_currDepth = 0;
    unsigned int m_maxDepth =16;
};
#endif
