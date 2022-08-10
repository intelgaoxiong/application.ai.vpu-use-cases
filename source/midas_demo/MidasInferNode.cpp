#include <MidasInferNode.hpp>

#include <utils/args_helper.hpp>
#include <utils/slog.hpp>
#include <utils/config_factory.h>
#include <utils/image_utils.h>

#include <pipelines/metadata.h>

//#define raw_output 1

MidasInferNode::MidasInferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const Config& config):
        hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), m_cfg(config){

}

std::shared_ptr<hva::hvaNodeWorker_t> MidasInferNode::createNodeWorker() const{
    return std::shared_ptr<hva::hvaNodeWorker_t>(new MidasInferNodeWorker((MidasInferNode*)this, m_cfg));
}

InferRequestPool_t::InferRequestPool_t(IE::ExecutableNetwork &executableNetwork, int32_t numInferRequest)
    : _cntInferRequest{numInferRequest}, _maxSize{static_cast<size_t>(numInferRequest)}
{
    for (int32_t i = 0; i < numInferRequest; i++)
    {
        IE::InferRequest::Ptr ptrInferRequest;
        try
        {
            ptrInferRequest = executableNetwork.CreateInferRequestPtr();
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
            continue;
        }
        catch(...)
        {
            continue;
        }
        _vecInferRequest.push_back(ptrInferRequest);
        _queue.push(ptrInferRequest);
    }
    if (_vecInferRequest.size() == 0)
    {
        std::cerr << "Error, no infer request created" << std::endl;
        throw std::runtime_error("Error, no infer request created");
    }
}

InferRequestPool_t::~InferRequestPool_t()
{
    for (auto ptrInferRequest : _vecInferRequest)
    {
        ptrInferRequest->Wait(1000000);
        ptrInferRequest->SetCompletionCallback([] {});
    }
#if 0
    while (_queue.size() != _vecInferRequest.size())
    {
        usleep(10000);
    }
#endif
}

InferRequestPool_t::Type InferRequestPool_t::get()
{
    Type ptr = nullptr;
    pop(ptr);
    return ptr;
}

void InferRequestPool_t::put(const InferRequestPool_t::Type ptrInferRequest)
{
    push(ptrInferRequest);
}

bool InferRequestPool_t::push(InferRequestPool_t::Type value)
{
    std::unique_lock<std::mutex> lk(_mutex);
    _cv_full.wait(lk, [this] { return (_queue.size() <= _maxSize || true == _close); });
    if (true == _close)
    {
        return false;
    }
    _queue.push(std::move(value));
    _cv_empty.notify_one();
    return true;
}

bool InferRequestPool_t::pop(InferRequestPool_t::Type &value)
{
    std::unique_lock<std::mutex> lk(_mutex);
    _cv_empty.wait(lk, [this] { return (!_queue.empty() || true == _close); });
    if (true == _close)
    {
        return false;
    }
    value = _queue.front();
    _queue.pop();
    _cv_full.notify_one();
    return true;
}

bool InferRequestPool_t::empty() const
{
    std::lock_guard<std::mutex> lk(_mutex);
    return _queue.empty();
}

void InferRequestPool_t::close()
{
    std::lock_guard<std::mutex> lk(_mutex);
    _close = true;
    _cv_empty.notify_all();
    _cv_full.notify_all();
    return;
}

MidasInferNodeWorker::MidasInferNodeWorker(hva::hvaNode_t* parentNode, const MidasInferNode::Config& config):hva::hvaNodeWorker_t(parentNode){
    slog::info << ov::get_openvino_version() << slog::endl;

    if (config.targetDevice != "VPUX") {
        slog::err << "Target device of midas is supposed to be VPUX" << slog::endl;
        return;
    }

    slog::info << "Loading model files:" << slog::endl << config.modelFileName << slog::endl;
    std::filebuf blobFile;
    if (!blobFile.open(config.modelFileName, std::ios::in | std::ios::binary))
    {
        blobFile.close();
        throw std::runtime_error("Could not open file");
    }
    std::istream graphBlob(&blobFile);
    m_executableNetwork = m_ie.ImportNetwork(graphBlob, config.targetDevice, {});

    // ---- Create infer request
    m_ptrInferRequestPool = std::make_shared<InferRequestPool_t>(m_executableNetwork, config.nireq);

    if (m_executableNetwork.GetInputsInfo().empty())
    {
        throw std::runtime_error("input info empty");
    }

    auto inputsInfo = m_executableNetwork.GetInputsInfo();
    m_inputName = m_executableNetwork.GetInputsInfo().begin()->first;
    slog::info << "Midas NN input name: " << m_inputName <<slog::endl;

    nn_width = config.nn_width;
    nn_height = config.nn_height;
    slog::info << "Midas NN input shape: " << nn_width << "x" << nn_height <<slog::endl;

    if (config.inferMode != "async")
        async_infer = 0;
    else
        async_infer = 1;

    if (async_infer)
        slog::info << "Midas NN inference in async mode" <<slog::endl;
    else
        slog::info << "Midas NN inference in sync mode" <<slog::endl;
}

IE::InferRequest::Ptr MidasInferNodeWorker::getInferRequest(){
    return m_ptrInferRequestPool->get();
}

/**
 * @brief Wraps data stored inside of a passed cv::Mat object by new Blob pointer.
 * @note: No memory allocation is happened. The blob just points to already existing
 *        cv::Mat data.
 * @param mat - given cv::Mat object with an image data.
 * @return resulting Blob pointer.
 */
static UNUSED InferenceEngine::Blob::Ptr wrapMat2Blob(const cv::Mat& mat) {
    size_t channels = mat.channels();
    size_t height = mat.size().height;
    size_t width = mat.size().width;

    size_t strideH = mat.step.buf[0];
    size_t strideW = mat.step.buf[1];

    bool is_dense = strideW == channels && strideH == channels * width;

    if (!is_dense)
        IE_THROW() << "Doesn't support conversion from not dense cv::Mat";

    InferenceEngine::TensorDesc tDesc(InferenceEngine::Precision::U8,
                                      {1, channels, height, width},
                                      InferenceEngine::Layout::NHWC);

    return InferenceEngine::make_shared_blob<uint8_t>(tDesc, mat.data);
}

void MidasInferNodeWorker::preprocess(const cv::Mat & img, IE::InferRequest::Ptr& request) {
    const auto& resizedImg = resizeImageExt(img, nn_width, nn_height);
    //cv::imshow("resizedImg", resizedImg);
    //cv::waitKey(1);

    request->SetBlob(m_inputName, wrapMat2Blob(resizedImg));
}

static void convertFp16ToU8(short * in, uint8_t * out, uint32_t width, float scale)
{
    int i;

    for (int i = 0; i < width; i++)
    {
        if (in[i] < 0) {
            out[i] = 0;
        }
        else {
            out[i] = (uint8_t)(((float)in[i] * scale));
        }
    }
}

cv::Mat MidasInferNodeWorker::postprocess_fp16(IE::InferRequest::Ptr& request) {
    if (m_executableNetwork.GetOutputsInfo().empty())
    {
        throw std::runtime_error("output info empty");
    }

    auto outputBlobName = m_executableNetwork.GetOutputsInfo().begin()->first;
    //slog::info << "Midas NN output name: " << outputBlobName <<slog::endl;
    auto ptrOutputBlob = request->GetBlob(outputBlobName);

    auto tdesc = ptrOutputBlob->getTensorDesc();
    //printf("output %d %d %d, element size %d\n", ptrOutputBlob->getTensorDesc().getDims()[0], ptrOutputBlob->getTensorDesc().getDims()[1], ptrOutputBlob->getTensorDesc().getDims()[2], ptrOutputBlob->size());

    auto precision = tdesc.getPrecision();
    if (precision != IE::Precision::FP16)
        throw std::runtime_error("Error: expect FP16 output");

    short *outRaw = ptrOutputBlob->buffer().as<short *>();

    int img_width =ptrOutputBlob->getTensorDesc().getDims()[2];
    int img_height = ptrOutputBlob->getTensorDesc().getDims()[1];

    cv::Mat mat = cv::Mat(img_height, img_width, CV_8UC1);

    short max = 0;
    for (int i = 0; i < img_width * img_height; i++)
    {
        if (outRaw[i] > max)
            max = outRaw[i];
    }
    if (max != 0) {
        float scale = (float)255.0 / (float)max;
        convertFp16ToU8(outRaw, (uint8_t *)mat.data, img_width * img_height, scale);
    }
    else
        printf("error, max is zero\n");

    return mat;

}

void MidasInferNodeWorker::putInferRequest(IE::InferRequest::Ptr ptrInferRequest){
    ptrInferRequest->SetCompletionCallback([] {});
    m_ptrInferRequestPool->put(ptrInferRequest);
    return;
}

void MidasInferNodeWorker::process(std::size_t batchIdx){
    auto vecBlobInput = hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t>{0});
    if (vecBlobInput.size() != 0)
    {
        HVA_DEBUG("MidasInferNode received blob with frameid %u and streamid %u", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId);
        auto cvFrame = vecBlobInput[0]->get<int, ImageMetaData>(0)->getMeta()->img;
        auto startTime = vecBlobInput[0]->get<int, ImageMetaData>(0)->getMeta()->timeStamp;

        auto ptrInferRequest = getInferRequest();

        if (async_infer) {
            std::function<void(InferenceEngine::InferRequest, InferenceEngine::StatusCode code)> callback = [=](InferenceEngine::InferRequest, InferenceEngine::StatusCode code) mutable
            {
                cv::Mat depthMat;
                //auto start = std::chrono::steady_clock::now();
                if (code != InferenceEngine::StatusCode::OK) {
                    std::string msg = "Inference request completion callback failed with InferenceEngine::StatusCode: " +
                                                                          std::to_string(code) + "\n\t";
                    HVA_WARNING("Midas callback skipped for one frame frame id %d, stream id is %d\n error msg %s\n", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId, msg.c_str());
               }
               else {
                    HVA_DEBUG("Midas callback start, frame id is: %d, stream id is: %d\n", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId);
                    //post-proc
                    depthMat = postprocess_fp16(ptrInferRequest);
               }

               //auto end = std::chrono::steady_clock::now();
               //auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
               //HVA_DEBUG("Postproc duration is %ld, mode is %s\n", duration, m_mode.c_str());

               putInferRequest(ptrInferRequest);
               std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
               InferMeta *ptrInferMeta = new InferMeta;
               ptrInferMeta->depthMat = depthMat;
               ptrInferMeta->frameId = vecBlobInput[0]->frameId;
               blob->emplace<int, InferMeta>(nullptr, 0, ptrInferMeta, [](int *payload, InferMeta * meta) {
                           if (payload!=nullptr) {
                               delete payload;
                           }
                           delete meta;
                       });
               blob->push(vecBlobInput[0]->get<int, ImageMetaData>(0));
               blob->frameId = vecBlobInput[0]->frameId;
               blob->streamId = vecBlobInput[0]->streamId;

               //cv::imshow("Video", cvFrame);
               //cv::imshow("Depth", depthMat);
               //cv::waitKey(1);

               sendOutput(blob, 0, ms(0));
               return;
            };

            ptrInferRequest->Wait(1000000);
            ptrInferRequest->SetCompletionCallback(callback);
        }
        //pre-proc
        preprocess(cvFrame, ptrInferRequest);
        if (async_infer) {
            //infernece async
            ptrInferRequest->StartAsync();
        }
        else {
            //infernece
            ptrInferRequest->Infer();
            //post-proc
            cv::Mat depthMat = postprocess_fp16(ptrInferRequest);
            putInferRequest(ptrInferRequest);

            std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
            InferMeta *ptrInferMeta = new InferMeta;
            ptrInferMeta->depthMat = depthMat;
            ptrInferMeta->frameId = vecBlobInput[0]->frameId;
            blob->emplace<int, InferMeta>(nullptr, 0, ptrInferMeta, [](int *payload, InferMeta * meta) {
                        if (payload!=nullptr) {
                            delete payload;
                        }
                        delete meta;
                    });
            blob->push(vecBlobInput[0]->get<int, ImageMetaData>(0));
            blob->frameId = vecBlobInput[0]->frameId;
            blob->streamId = vecBlobInput[0]->streamId;

            //cv::imshow("Video", cvFrame);
            //cv::imshow("Depth", depthMat);
            //cv::waitKey(1);
            sendOutput(blob, 0, ms(0));
        }
    }
}

void MidasInferNodeWorker::init(){
}

void MidasInferNodeWorker::deinit(){
}

void MidasInferNodeWorker::processByFirstRun(std::size_t batchIdx) {
#if 0
    m_resultThread = std::make_shared<std::thread> ([&]() {
        while (m_exec) {
            //m_pipeline->waitForData();
            m_pipeline->waitForResult();

            std::shared_ptr<hva::hvaBlob_t> pendingBlob;
            {
                std::unique_lock<std::mutex> lock(m_blobMutex);
                pendingBlob = m_inferWaitingQueue.front();
                m_inferWaitingQueue.pop();
            }

            std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
            InferMeta *ptrInferMeta = new InferMeta;
            //Post-process
            auto nnresult = m_pipeline->getResult();
            if (nnresult) {
                DetectionResult result = nnresult->asRef<DetectionResult>();
#if raw_output
                // Visualizing result data over source image
                slog::debug << " -------------------- Frame # " << result.frameId << "--------------------" << slog::endl;
                slog::debug << " Class ID  | Confidence | XMIN | YMIN | XMAX | YMAX " << slog::endl;
                for (auto& obj : result.objects) {
                    slog::debug << " " << std::left << std::setw(9) << obj.label << " | " << std::setw(10) << obj.confidence
                                << " | " << std::setw(4) << int(obj.x) << " | " << std::setw(4) << int(obj.y) << " | "
                                << std::setw(4) << int(obj.x + obj.width) << " | " << std::setw(4) << int(obj.y + obj.height)
                                << slog::endl;
                }
#endif
                ptrInferMeta->detResult = result;
            } else {
                HVA_WARNING("No NN results, should not happen\n");
                return;
            }

            ptrInferMeta->frameId = pendingBlob->frameId;
            blob->emplace<int, InferMeta>(nullptr, 0, ptrInferMeta, [](int *payload, InferMeta * meta) {
                    if (payload!=nullptr) {
                        delete payload;
                    }
                    delete meta;
                });
            blob->push(pendingBlob->get<int, ImageMetaData>(0));
            blob->frameId = pendingBlob->frameId;
            blob->streamId = pendingBlob->streamId;

            sendOutput(blob, 0, ms(0));
        }
    });
#endif
}

void MidasInferNodeWorker::processByLastRun(std::size_t batchIdx) {
#if 0
    if (m_resultThread && m_resultThread->joinable()) {
        m_exec = false;
        m_resultThread->join();
    }
#endif
}