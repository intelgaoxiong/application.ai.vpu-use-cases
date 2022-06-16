#include <DisplayNode.hpp>

DisplayNode::DisplayNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const Config& config):
        hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), m_reserved1(config.reserved1), m_reserved2(config.reserved2){

}

std::shared_ptr<hva::hvaNodeWorker_t> DisplayNode::createNodeWorker() const{
    return std::shared_ptr<hva::hvaNodeWorker_t>(new DisplayNodeWorker((DisplayNode*)this, m_reserved1, m_reserved2));
}

DisplayNodeWorker::DisplayNodeWorker(hva::hvaNode_t* parentNode, unsigned reserved1, unsigned reserved2):hva::hvaNodeWorker_t(parentNode), 
        m_reserved1(reserved1), m_reserved2(reserved2){

}

void DisplayNodeWorker::process(std::size_t batchIdx){
    std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput= hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    if(vInput.size() != 0){
        HVA_DEBUG("DispalyNode received blob with frameid %u and streamid %u", vInput[0]->frameId, vInput[0]->streamId);

        auto cvFrame = vInput[0]->get<cv::Mat, int>(0)->getPtr();
        auto outputResolution = cvFrame->size();
        HVA_DEBUG("DispalyNode[%u] outputResolution %d x %d", vInput[0]->streamId, outputResolution.width, outputResolution.height);

        cv::imshow("Detection Results", *cvFrame);
        cv::waitKey(1);
    }
}

void DisplayNodeWorker::init(){
}

void DisplayNodeWorker::deinit(){
}

void DisplayNodeWorker::processByFirstRun(std::size_t batchIdx) {
}

void DisplayNodeWorker::processByLastRun(std::size_t batchIdx) {
}