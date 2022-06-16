#include <FrameReaderNode.hpp>

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
    printf("FrameReaderNodeWorker input %s\n", m_input.c_str());

}

void FrameReaderNodeWorker::process(std::size_t batchIdx){
    //--- Capturing frame
    cv::Mat curr_frame = m_cap->read();

    if (curr_frame.empty()) {
        //Should handle this case
        HVA_WARNING("EOF!!!\n");
        return;
    }
    auto outputResolution = curr_frame.size();

    std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
    blob->emplace<cv::Mat, int>(new cv::Mat(curr_frame), outputResolution.width * outputResolution.height * 3, new int(1));
    blob->frameId = m_frame_index;
    blob->streamId = m_streamId;
    m_frame_index ++;
    sendOutput(blob, 0, ms(0));
}

void FrameReaderNodeWorker::init(){
        //Should be configurable
        //m_input
        m_loop = true;
        m_rt = read_type::safe;
}

void FrameReaderNodeWorker::deinit(){
}

void FrameReaderNodeWorker::processByFirstRun(std::size_t batchIdx) {
    m_cap = openImagesCapture(m_input, m_loop, m_rt);
}

void FrameReaderNodeWorker::processByLastRun(std::size_t batchIdx) {
}