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

cv::Mat DisplayNodeWorker::renderDetectionData(DetectionResult& result, const ColorPalette& palette, OutputTransform& outputTransform) {
    if (!result.metaData) {
        throw std::invalid_argument("Renderer: metadata is null");
    }

    auto outputImg = result.metaData->asRef<ImageMetaData>().img;

    if (outputImg.empty()) {
        throw std::invalid_argument("Renderer: image provided in metadata is empty");
    }
    outputTransform.resize(outputImg);

    for (auto& obj : result.objects) {
        outputTransform.scaleRect(obj);
        std::ostringstream conf;
        conf << ":" << std::fixed << std::setprecision(1) << obj.confidence * 100 << '%';
        const auto& color = palette[obj.labelID];
        putHighlightedText(outputImg,
                           obj.label + conf.str(),
                           cv::Point2f(obj.x, obj.y - 5),
                           cv::FONT_HERSHEY_COMPLEX_SMALL,
                           1,
                           color,
                           2);
        cv::rectangle(outputImg, obj, color, 2);
    }

    try {
        for (auto& lmark : result.asRef<RetinaFaceDetectionResult>().landmarks) {
            outputTransform.scaleCoord(lmark);
            cv::circle(outputImg, lmark, 2, cv::Scalar(0, 255, 255), -1);
        }
    } catch (const std::bad_cast&) {}

    return outputImg;
}

void DisplayNodeWorker::process(std::size_t batchIdx){
    std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput= hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    if(vInput.size() != 0){
        HVA_DEBUG("DispalyNode received blob with frameid %u and streamid %u", vInput[0]->frameId, vInput[0]->streamId);

        auto eof = vInput[0]->get<int, ImageMetaData>(1)->getPtr();
        if (!(*eof)) {
            auto cvFrame = vInput[0]->get<int, ImageMetaData>(1)->getMeta()->img;
            auto outputResolution = cvFrame.size();
            HVA_DEBUG("DispalyNode[%u] outputResolution %d x %d", vInput[0]->streamId, outputResolution.width, outputResolution.height);

            auto timeStamp = vInput[0]->get<int, ImageMetaData>(1)->getMeta()->timeStamp;

            auto ptrInferMeta = vInput[0]->get<int, InferMeta>(0)->getMeta();
            cv::Mat outFrame = renderDetectionData(ptrInferMeta->detResult, *m_palettePtr.get(), *m_outputTransform.get());
            m_metrics->update(timeStamp,
                               outFrame,
                               {10, 22},
                               cv::FONT_HERSHEY_COMPLEX,
                               0.65);
            cv::imshow("Detection Results", outFrame);
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
    m_outputTransform.reset(new OutputTransform());
    m_metrics.reset(new PerformanceMetrics());
}

void DisplayNodeWorker::processByLastRun(std::size_t batchIdx) {
}
