#include <MidasInferNode.hpp>

#include <utils/args_helper.hpp>
#include <utils/slog.hpp>
#include <utils/config_factory.h>
#include <utils/image_utils.h>

#include <pipelines/metadata.h>

#define SET_BLOB 0  //Disable SetBlob method, not stable currently.

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

    auto inputsInfo = m_executableNetwork.GetInputsInfo();

    if (inputsInfo.empty())
    {
        throw std::runtime_error("input info empty");
    } else if (inputsInfo.size() > 1) {
        throw std::runtime_error("Only support 1 input model for now");
    }

    m_inputName = inputsInfo.begin()->first;
    slog::info << "Midas NN input name: " << m_inputName <<slog::endl;

    auto inputInfo = inputsInfo.begin()->second;
    auto inputTensorDesc = inputInfo->getTensorDesc();
    auto inputLayout = inputTensorDesc.getLayout();

    if (inputLayout == IE::Layout::NCHW) {
        nn_width = inputTensorDesc.getDims()[3];
        nn_height = inputTensorDesc.getDims()[2];
        nn_channel = inputTensorDesc.getDims()[1];
    } else {
        throw std::runtime_error("Only support Layout::NCHW input for now");
    }

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

//interleved -> plannar (HWC -> CHW)
static void convertHWC2CHW(uint8_t * chw_dst, uint8_t * hwc_src, uint32_t width, uint32_t height, uint32_t channel) {
    for (int c = 0; c < channel; c ++) {
        for (int h =0; h < height; h ++) {
            for (int w = 0; w < width; w ++) {
                chw_dst[w + h * width + c * height * width]  = hwc_src[channel * (w + h * width) + c];
            }
        }
    }
}

void MidasInferNodeWorker::preprocess(const cv::Mat & img, IE::InferRequest::Ptr& request) {
    auto preProcStart = std::chrono::steady_clock::now();
    const auto& resizedImg = resizeImageExt(img, nn_width, nn_height);
    //cv::imshow("resizedImg", resizedImg);
    //cv::waitKey(1);

#if SET_BLOB
    request->SetBlob(m_inputName, wrapMat2Blob(resizedImg));
#else
    IE::Blob::Ptr inputBlob = request->GetBlob(m_inputName);
    IE::MemoryBlob::Ptr minput = IE::as<IE::MemoryBlob>(inputBlob);
    if (!minput) {
        IE_THROW() << "We expect inputBlob to be inherited from MemoryBlob in "
                      "fillBlobImage, "
                   << "but by fact we were not able to cast inputBlob to MemoryBlob";
    }
    // locked memory holder should be alive all time while access to its buffer
    // happens
    auto minputHolder = minput->wmap();
    auto inputBlobData = minputHolder.as<uint8_t*>();

    convertHWC2CHW(inputBlobData, resizedImg.data, nn_width, nn_height, nn_channel);
#endif
    preprocessMetrics.update(preProcStart);
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

static void getU8Depth(short * in, uint8_t * out, uint32_t width, short max, short min)
{
    int i;

    for (int i = 0; i < width; i++)
    {
        if (in[i] < 0) {
            out[i] = 0;
        }
        else {
            float value = (float)255 * ((float)(in[i] - min) / (float)(max - min));
            if (value > 255)
                out[i] = 255;
            else
                out[i] = (uint8_t)value;
        }
    }
}

cv::Mat MidasInferNodeWorker::postprocess_fp16(IE::InferRequest::Ptr& request) {
    if (m_executableNetwork.GetOutputsInfo().empty())
    {
        throw std::runtime_error("output info empty");
    }

    auto postProcStart = std::chrono::steady_clock::now();

    auto outputBlobName = m_executableNetwork.GetOutputsInfo().begin()->first;
    //slog::info << "Midas NN output name: " << outputBlobName <<slog::endl;
    auto ptrOutputBlob = request->GetBlob(outputBlobName);

    auto tdesc = ptrOutputBlob->getTensorDesc();
    //printf("output %d %d %d, element size %d\n", ptrOutputBlob->getTensorDesc().getDims()[0], ptrOutputBlob->getTensorDesc().getDims()[1], ptrOutputBlob->getTensorDesc().getDims()[2], ptrOutputBlob->size());

    auto precision = tdesc.getPrecision();
    if (precision != IE::Precision::FP16)
        throw std::runtime_error("Error: expect FP16 output");

    //short *outRaw = ptrOutputBlob->buffer().as<short *>();

    int img_width =ptrOutputBlob->getTensorDesc().getDims()[2];
    int img_height = ptrOutputBlob->getTensorDesc().getDims()[1];

    cv::Mat mat = cv::Mat(img_height, img_width, CV_8UC1);
    cv::Mat outMat(img_height, img_width, CV_16SC1, ptrOutputBlob->buffer().as<short*>());
    cv::normalize(outMat, mat, 0, 255, cv::NORM_MINMAX, CV_8UC1);

    postprocessMetrics.update(postProcStart);

    return mat;

}

void MidasInferNodeWorker::putInferRequest(IE::InferRequest::Ptr ptrInferRequest){
    ptrInferRequest->SetCompletionCallback([] {});
    m_ptrInferRequestPool->put(ptrInferRequest);
    return;
}

void MidasInferNodeWorker::enqueueAsyncTask(AsyncTaskInProcess::Ptr ptrTask)
{
    {
        std::lock_guard<std::mutex> guard(m_taskMutex);
        m_asyncTaskQueue.push(ptrTask);
    }
    m_asyncTaskQueueNotEmpty.notify_one();
}

AsyncTaskInProcess::Ptr MidasInferNodeWorker::dequeueAsyncTask()
{
    std::lock_guard<std::mutex> guard(m_taskMutex);

    while (m_exec && m_asyncTaskQueue.empty()) {
        std::unique_lock<std::mutex> lock(m_taskMutex, std::defer_lock);
        m_asyncTaskQueueNotEmpty.wait(lock);
    }
    if (!m_exec)
        return {};

    auto taskPtr = m_asyncTaskQueue.front();
    m_asyncTaskQueue.pop();

    return taskPtr;
}

AsyncTaskInProcess::Ptr MidasInferNodeWorker::makeAsyncTask(IE::InferRequest::Ptr ptrReq, std::shared_ptr<hva::hvaBlob_t> hvaBlob, std::chrono::steady_clock::time_point startTime)
{
    auto asyncTaskPtr = std::make_shared<AsyncTaskInProcess>(ptrReq, hvaBlob, startTime);
    if (!asyncTaskPtr) {
        std::cerr << "Error, fail to create  AsyncTaskInProcess ptr" << std::endl;
        return nullptr;
    }
    return asyncTaskPtr;
}

void MidasInferNodeWorker::process(std::size_t batchIdx){
    auto vecBlobInput = hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t>{0});
    if (vecBlobInput.size() != 0)
    {
        HVA_DEBUG("MidasInferNode received blob with frameid %u and streamid %u", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId);

        if ((vecBlobInput[0]->frameId % 100) == 0) {
            logLatencyPerStage(preprocessMetrics.getLast().latency,
                           inferenceMetrics.getLast().latency,
                           postprocessMetrics.getLast().latency);
        }

        auto cvFrame = vecBlobInput[0]->get<int, ImageMetaData>(0)->getMeta()->img;
        auto startTime = vecBlobInput[0]->get<int, ImageMetaData>(0)->getMeta()->timeStamp;

        auto ptrInferRequest = getInferRequest();

        //pre-proc
        preprocess(cvFrame, ptrInferRequest);

        if (async_infer) {
            //infernece async
            auto asyncInferStart = std::chrono::steady_clock::now();
            ptrInferRequest->StartAsync();

            auto taskPtr = makeAsyncTask(ptrInferRequest, vecBlobInput[0], asyncInferStart);
            enqueueAsyncTask(taskPtr);
        } else {
            //infernece sync
            auto inferSyncStart = std::chrono::steady_clock::now();
            ptrInferRequest->Infer();
            inferenceMetrics.update(inferSyncStart);
            //post-proc
            cv::Mat depthMat = postprocess_fp16(ptrInferRequest);
            putInferRequest(ptrInferRequest);

            std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
            InferMeta *ptrInferMeta = new InferMeta;
            ptrInferMeta->depthMat = depthMat;
            ptrInferMeta->frameId = vecBlobInput[0]->frameId;
            ptrInferMeta->timeStamp = inferSyncStart;
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

void MidasInferNodeWorker::logLatencyPerStage(double preprocLat, double inferLat, double postprocLat) {
    slog::info << "\t>>>>>Latency per stage<<<<<" << slog::endl;
    slog::info << "\tPreprocessing:\t" << preprocLat << " ms" << slog::endl;
    slog::info << "\tInference:\t" << inferLat << " ms" << slog::endl;
    slog::info << "\tPostprocessing:\t" << postprocLat << " ms" << slog::endl;
}

void MidasInferNodeWorker::processByFirstRun(std::size_t batchIdx) {
    m_resultThread = std::make_shared<std::thread> ([&]() {
        while (m_exec) {

            auto task = dequeueAsyncTask();
            if (!task) {
                continue;
            }

            std::shared_ptr<hva::hvaBlob_t> pendingBlob = task->m_hvaBlob;
            IE::InferRequest::Ptr ptrPendingReq = task->m_ptrInferReq;

            ptrPendingReq->Wait();
            //Got result
            inferenceMetrics.update(task->m_asyncStartTime);

            cv::Mat depthMat = postprocess_fp16(ptrPendingReq);

            putInferRequest(ptrPendingReq);
            std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
            InferMeta *ptrInferMeta = new InferMeta;
            ptrInferMeta->depthMat = depthMat;
            ptrInferMeta->frameId = pendingBlob->frameId;
            ptrInferMeta->timeStamp = task->m_asyncStartTime;
            blob->emplace<int, InferMeta>(nullptr, 0, ptrInferMeta, [](int *payload, InferMeta * meta) {
                        if (payload!=nullptr) {
                            delete payload;
                        }
                        delete meta;
                    });
            blob->push(pendingBlob->get<int, ImageMetaData>(0));
            blob->frameId = pendingBlob->frameId;
            blob->streamId = pendingBlob->streamId;

            //cv::imshow("Video", cvFrame);
            //cv::imshow("Depth", depthMat);
            //cv::waitKey(1);

            sendOutput(blob, 0, ms(0));
        }
    });
}

void MidasInferNodeWorker::processByLastRun(std::size_t batchIdx) {
    if (m_resultThread && m_resultThread->joinable()) {
        m_exec = false;
        m_resultThread->join();
    }
}
