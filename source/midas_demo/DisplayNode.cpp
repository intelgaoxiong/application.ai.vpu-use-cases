#include <DisplayNode.hpp>
#include <pipelines/metadata.h>

DisplayNode::DisplayNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const Config& config):
        hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), m_cfg(config){

}

std::shared_ptr<hva::hvaNodeWorker_t> DisplayNode::createNodeWorker() const{
    return std::shared_ptr<hva::hvaNodeWorker_t>(new DisplayNodeWorker((DisplayNode*)this, m_cfg));
}

DisplayNodeWorker::DisplayNodeWorker(hva::hvaNode_t* parentNode, const DisplayNode::Config& config):hva::hvaNodeWorker_t(parentNode){
        size_t found = config.dispRes.find("x");
        if (found == std::string::npos) {
            m_displayResolution = cv::Size{0, 0};
        } else {
            m_displayResolution = cv::Size{
                std::stoi(config.dispRes.substr(0, found)),
                std::stoi(config.dispRes.substr(found + 1, config.dispRes.length()))};
        }

        printf("Display resolution %d x %d", m_displayResolution.width, m_displayResolution.height);
}

void DisplayNodeWorker::process(std::size_t batchIdx){
    std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput= hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    if(vInput.size() != 0){
        HVA_DEBUG("DispalyNode received blob with frameid %u and streamid %u", vInput[0]->frameId, vInput[0]->streamId);

        auto eof = vInput[0]->get<int, ImageMetaData>(1)->getPtr();
        if (!(*eof)) {
            auto cvFrame = vInput[0]->get<int, ImageMetaData>(1)->getMeta()->img;
            auto frameResolution = cvFrame.size();
            HVA_DEBUG("DispalyNode[%u] frameResolution %d x %d", vInput[0]->streamId, frameResolution.width, frameResolution.height);

            if (vInput[0]->frameId == 0){
                if (m_displayResolution.width == 0 || m_displayResolution.height == 0)
                    m_displayResolution = frameResolution;

                m_outputTransform.reset(new OutputTransform(frameResolution, m_displayResolution));
                m_outputTransform->computeResolution();
            }

#if 1
            //Latency data does not include the time cost of frame scaling.
            auto timeStamp = vInput[0]->get<int, InferMeta>(0)->getMeta()->timeStamp;
            m_metrics->update(timeStamp);
            m_outputTransform->resize(cvFrame);
            m_metrics->paintMetrics(cvFrame,
                    {10, 22},
                    cv::FONT_HERSHEY_COMPLEX,
                    0.65);
#else
            m_outputTransform->resize(cvFrame);
            auto timeStamp = vInput[0]->get<int, InferMeta>(0)->getMeta()->timeStamp;
            m_metrics->update(timeStamp,
                    cvFrame,
                    {10, 22},
                    cv::FONT_HERSHEY_COMPLEX,
                    0.65);
#endif
#if 1
            auto ptrInferMeta = vInput[0]->get<int, InferMeta>(0)->getMeta();

            cv::Mat depthFrame;
            depthFrame = ptrInferMeta->depthMat;
            //cv::resize(depthFrame, depthFrame, cv::Size(frameResolution.width, frameResolution.height), 0, 0, cv::INTER_CUBIC);
            //depthFrame = depthFrame * 255;
            cv::Mat colorDepth;
            cv::applyColorMap(depthFrame, colorDepth, cv::COLORMAP_MAGMA);
            cv::imshow("Color Depth", colorDepth);

            //cv::reprojectImageTo3D(depthFrame, points_3D, Q, false);
            //cv::imshow("Reproject", points_3D);
            //cv::imwrite("./depth.png", depthFrame);
#endif
            cv::imshow("Video", cvFrame);
            cv::waitKey(1);
        }
    }
}

void DisplayNodeWorker::init(){
}

void DisplayNodeWorker::deinit(){
}

void DisplayNodeWorker::processByFirstRun(std::size_t batchIdx) {
    m_palettePtr.reset(new ColorPalette(100));
    m_metrics.reset(new PerformanceMetrics());
}

void DisplayNodeWorker::processByLastRun(std::size_t batchIdx) {
}
