#include <FrameReaderNode.hpp>
#include <pipelines/metadata.h>

#include <windows.h>

FrameReaderNode::FrameReaderNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const Config& config):
        m_workerIdx(0), hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), m_cfg(config){

}

std::shared_ptr<hva::hvaNodeWorker_t> FrameReaderNode::createNodeWorker() const{
    unsigned workerIdx = m_workerIdx.fetch_add(1);
    return std::shared_ptr<hva::hvaNodeWorker_t>(new FrameReaderNodeWorker((FrameReaderNode*)this, static_cast<int>(workerIdx), m_cfg));
}

FrameReaderNodeWorker::FrameReaderNodeWorker(hva::hvaNode_t* parentNode, int streamId, const FrameReaderNode::Config& config):hva::hvaNodeWorker_t(parentNode),
    m_streamId(streamId){
    m_input = config.input;
    m_loop = config.infiniteLoop;
    m_rt = config.readType;
    m_maxFPS = config.maxFPS;
    if (m_maxFPS != 0) {
        m_intervalMs = (double) (1000.0f) / m_maxFPS;
    }
    HVA_DEBUG("FrameReaderNodeWorker[%d] input %s\n", m_streamId, m_input.c_str());

}

void FrameReaderNodeWorker::process(std::size_t batchIdx){
    while (1) {
        if (m_currDepth >= m_maxDepth)
            Sleep(5);
        else
            break;
    }
    //--- Capturing frame
    auto capStartTime = std::chrono::steady_clock::now();
    cv::Mat curr_frame = m_cap->read();
    auto capEndTime = std::chrono::steady_clock::now();
    auto capDuration = std::chrono::duration_cast<std::chrono::milliseconds>(capEndTime - capStartTime).count();

    if (m_maxFPS != 0) {
        if (m_intervalMs > (double)capDuration) {
            HVA_DEBUG("Throttle frame source rate by sleep %f ms, read cost %ld ms\n", m_intervalMs - (double)capDuration, capDuration);
            Sleep(m_intervalMs - capDuration);
        }
    }

    bool pipe_stop_event = false;
    FrameReaderNode  * m_FRNode = dynamic_cast<FrameReaderNode*>(hva::hvaNodeWorker_t::getParentPtr());

    if (curr_frame.empty()) {
        if (!m_loop) {
            //Should handle this case
            HVA_WARNING("EOF!");
            pipe_stop_event = true;
        } else {
            m_cap = openImagesCapture(m_input, m_loop, m_rt);
            return;
        }
    }
    auto outputResolution = curr_frame.size();

    std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());

    int *eof;
    if (pipe_stop_event)
        eof = new int(1);
    else
        eof = new int(0);

    auto startTime = std::chrono::steady_clock::now();
    blob->emplace<int, ImageMetaData>(eof, 8u, new ImageMetaData{curr_frame, startTime},
            [this, pipe_stop_event, m_FRNode](int * m, ImageMetaData * m_meta) {
                if(pipe_stop_event) {
                    m_FRNode->emitEvent(hvaEvent_EOF, nullptr);
                    HVA_WARNING("Emit hvaEvent_EOF!");
                }
                delete m;
                delete m_meta;
                this->m_currDepth --;
            });
    blob->frameId = m_frame_index;
    blob->streamId = m_streamId;
    m_frame_index ++;
    m_currDepth ++;
    sendOutput(blob, 0, ms(0));
}

void FrameReaderNodeWorker::init(){
}

void FrameReaderNodeWorker::deinit(){
}

void FrameReaderNodeWorker::processByFirstRun(std::size_t batchIdx) {
    m_cap = openImagesCapture(m_input, m_loop, m_rt);
}

void FrameReaderNodeWorker::processByLastRun(std::size_t batchIdx) {
    auto readLat = m_cap->getMetrics().getTotal().latency;
    slog::info << "\tDecoding:\t" << std::fixed << std::setprecision(1) <<
        readLat << " ms" << slog::endl;
}
