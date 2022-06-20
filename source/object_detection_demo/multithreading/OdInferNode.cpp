#include <OdInferNode.hpp>

#include <utils/args_helper.hpp>
#include <utils/config_factory.h>

ODInferNode::ODInferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const Config& config):
        hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), m_cfg(config){

}

std::shared_ptr<hva::hvaNodeWorker_t> ODInferNode::createNodeWorker() const{
    return std::shared_ptr<hva::hvaNodeWorker_t>(new ODInferNodeWorker((ODInferNode*)this, m_cfg));
}

ODInferNodeWorker::ODInferNodeWorker(hva::hvaNode_t* parentNode, const ODInferNode::Config& config):hva::hvaNodeWorker_t(parentNode){
    const auto& strAnchors = split(config.anchors, ',');
    const auto& strMasks = split(config.masks, ',');

    try {
        for (auto& str : strAnchors) {
            m_anchors.push_back(std::stof(str));
        }
    } catch (...) { throw std::runtime_error("Invalid anchors list is provided."); }

    try {
        for (auto& str : strMasks) {
            m_masks.push_back(std::stoll(str));
        }
    } catch (...) { throw std::runtime_error("Invalid masks list is provided."); }

    if (config.labelFilename != "") {
        HVA_DEBUG("ODInferNodeWorker load label file %s\n", config.labelFilename.c_str());
        m_labels = DetectionModel::loadLabels(config.labelFilename);
    } else {
        HVA_DEBUG("No label file\n");
    }

    if (config.architectureType == "centernet") {
        m_model.reset(new ModelCenterNet(config.modelFileName, static_cast<float>(config.confidenceThreshold), m_labels, config.layout));
    } else if (config.architectureType == "faceboxes") {
        m_model.reset(new ModelFaceBoxes(config.modelFileName,
                                       static_cast<float>(config.confidenceThreshold),
                                       config.autoResize,
                                       static_cast<float>(config.iouThreshold),
                                       config.layout));
    } else if (config.architectureType == "retinaface") {
        m_model.reset(new ModelRetinaFace(config.modelFileName,
                                        static_cast<float>(config.confidenceThreshold),
                                        config.autoResize,
                                        static_cast<float>(config.iouThreshold),
                                        config.layout));
    } else if (config.architectureType == "retinaface-pytorch") {
        m_model.reset(new ModelRetinaFacePT(config.modelFileName,
                                          static_cast<float>(config.confidenceThreshold),
                                          config.autoResize,
                                          static_cast<float>(config.iouThreshold),
                                          config.layout));
    } else if (config.architectureType == "ssd") {
        m_model.reset(new ModelSSD(config.modelFileName, static_cast<float>(config.confidenceThreshold), config.autoResize, m_labels, config.layout));
    } else if (config.architectureType == "yolo") {
        m_model.reset(new ModelYolo(config.modelFileName,
                                  static_cast<float>(config.confidenceThreshold),
                                  config.autoResize,
                                  config.yolo_af,
                                  static_cast<float>(config.iouThreshold),
                                  m_labels,
                                  m_anchors,
                                  m_masks,
                                  config.layout));
    } else {
        slog::err << "No model type or invalid model type (config.architectureType) provided: " + config.architectureType << slog::endl;
        return;
    }
    m_model->setInputsPreprocessing(config.reverse_input_channels, config.mean_values, config.scale_values);
    slog::info << ov::get_openvino_version() << slog::endl;

    m_pipeline = std::make_shared<AsyncPipeline>(std::move(m_model),
                               ConfigFactory::getUserConfig(config.targetDevice, config.nireq, config.nstreams, config.nthreads),
                               m_core);
}

void ODInferNodeWorker::process(std::size_t batchIdx){
    if (m_pipeline->isReadyToProcess()) {
        std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput= hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
        if(vInput.size() != 0) {
            HVA_DEBUG("DetectionNode received blob with frameid %u and streamid %u", vInput[0]->frameId, vInput[0]->streamId);
            printf("DetectionNode received blob with frameid %u and streamid %u\n", vInput[0]->frameId, vInput[0]->streamId);
            auto cvFrame = vInput[0]->get<cv::Mat, int>(0)->getPtr();
            auto startTime = std::chrono::steady_clock::now();
            m_frameNum = m_pipeline->submitData(ImageInputData(*cvFrame),
                                               std::make_shared<ImageMetaData>(*cvFrame, startTime));
            //TODO: wait and process result in another thread
            m_pipeline->waitForData();
            auto nnresult = m_pipeline->getResult();
            if (nnresult) {
                DetectionResult result = nnresult->asRef<DetectionResult>();
                slog::debug << " -------------------- Frame # " << result.frameId << "--------------------" << slog::endl;
                slog::debug << " Class ID  | Confidence | XMIN | YMIN | XMAX | YMAX " << slog::endl;
                for (auto& obj : result.objects) {
                    slog::debug << " " << std::left << std::setw(9) << obj.label << " | " << std::setw(10) << obj.confidence
                                << " | " << std::setw(4) << int(obj.x) << " | " << std::setw(4) << int(obj.y) << " | "
                                << std::setw(4) << int(obj.x + obj.width) << " | " << std::setw(4) << int(obj.y + obj.height)
                                << slog::endl;
                }
            }
            sendOutput(vInput[0], 0, ms(0));
        }
    } else {
        HVA_DEBUG("DetectionNode has no idle infer request\n");
    }
    
}

void ODInferNodeWorker::init(){
}

void ODInferNodeWorker::deinit(){
}

void ODInferNodeWorker::processByFirstRun(std::size_t batchIdx) {
}

void ODInferNodeWorker::processByLastRun(std::size_t batchIdx) {
}
