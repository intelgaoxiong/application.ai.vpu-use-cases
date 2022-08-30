#include <FrameReaderNode.hpp>
#include <pipelines/metadata.h>

#include <windows.h>

static NTSTATUS(__stdcall* NtDelayExecution)(BOOL Alertable, PLARGE_INTEGER DelayInterval) = (NTSTATUS(__stdcall*)(BOOL, PLARGE_INTEGER)) GetProcAddress(GetModuleHandle("ntdll.dll"), "NtDelayExecution");
static NTSTATUS(__stdcall* ZwSetTimerResolution)(IN ULONG RequestedResolution, IN BOOLEAN Set, OUT PULONG ActualResolution) = (NTSTATUS(__stdcall*)(ULONG, BOOLEAN, PULONG)) GetProcAddress(GetModuleHandle("ntdll.dll"), "ZwSetTimerResolution");

static void high_resolution_sleep(const std::chrono::nanoseconds& sleep_time)
{
    static bool is_timer_res_set = false;
    if (!is_timer_res_set)
    {
        ULONG actualResolution;
        ZwSetTimerResolution(1, true, &actualResolution);
        is_timer_res_set = false;
    }

    LARGE_INTEGER interval;
    interval.QuadPart = -1 * (int)((float)(sleep_time.count() / 100.));
    NtDelayExecution(false, &interval);
}

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
        m_intervalNs = std::chrono::nanoseconds((unsigned long long)(1000000000.0 / m_maxFPS));
    }
    HVA_DEBUG("FrameReaderNodeWorker[%d] input %s\n", m_streamId, m_input.c_str());

}

void FrameReaderNodeWorker::process(std::size_t batchIdx){
    while (1) {
        if (m_currDepth >= m_maxDepth)
            high_resolution_sleep(std::chrono::nanoseconds(5 * 1000 * 1000));
        else
            break;
    }

    //--- Capturing frame
    cv::Mat curr_frame = m_cap->read();

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

    if (m_frame_index > 1) {
        auto sendDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - m_lastSendTime);
        if (sendDuration < m_intervalNs) {
            high_resolution_sleep(m_intervalNs - sendDuration);
        }
    }
    m_lastSendTime = std::chrono::steady_clock::now();
    sendOutput(blob, 0, ms(0));
}

void FrameReaderNodeWorker::init(){
}

void FrameReaderNodeWorker::deinit(){
}

void FrameReaderNodeWorker::processByFirstRun(std::size_t batchIdx) {
    m_cap = openImagesCapture(m_input, m_loop, m_rt);

    m_mepThread = std::make_shared<std::thread> ([&]() {
        high_resolution_sleep(std::chrono::nanoseconds(1000 * 1000 * 1000));
        mep_cap = openImagesCapture("0", m_loop, m_rt);
        while (m_exec) {
            cv::Mat curr_mep_frame = mep_cap->read();
            mep_cap->getMetrics().paintMetrics(curr_mep_frame,
                    {10, 22},
                    cv::FONT_HERSHEY_COMPLEX,
                    0.65);
            cv::imshow("MEP", curr_mep_frame);
            cv::waitKey(1);
        }
    });
}

void FrameReaderNodeWorker::processByLastRun(std::size_t batchIdx) {
    auto readLat = m_cap->getMetrics().getTotal().latency;
    slog::info << "\tDecoding:\t" << std::fixed << std::setprecision(1) <<
        readLat << " ms" << slog::endl;

    if (m_mepThread && m_mepThread->joinable()) {
        m_exec = false;
        m_mepThread->join();
    }
}
