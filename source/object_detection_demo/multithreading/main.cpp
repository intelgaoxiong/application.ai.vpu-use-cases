#include <FrameReaderNode.hpp>
#include <DisplayNode.hpp>
#include <Windows.h>

int main(int argc, char* argv[]){
    hvaLogger.setLogLevel(hva::hvaLogger_t::LogLevel::WARNING);

    HVA_INFO("App Start:");

    hva::hvaPipeline_t pl;

    hva::hvaBatchingConfig_t batchingConfig;
    batchingConfig.batchingPolicy = hva::hvaBatchingConfig_t::BatchingWithStream;
    batchingConfig.batchSize = 1;
    batchingConfig.streamNum = 1;
    batchingConfig.threadNumPerBatch = 1;

    FrameReaderNode::Config FRConfig;
    FRConfig.input = "C:/work/sample-videos/car-detection.mp4";
    auto& FRNode = pl.setSource(std::make_shared<FrameReaderNode>(0, 1, 1, FRConfig), "FRNode");
    FRNode.configBatch(batchingConfig);

    DisplayNode::Config DispConfig;
    auto& DispNode = pl.addNode(std::make_shared<DisplayNode>(1, 0, 1, DispConfig), "DispNode");
    DispNode.configBatch(batchingConfig);

    // Link nodes
    pl.linkNode("FRNode", 0, "DispNode", 0);

    // Start pipeline
    pl.prepare();

    std::cout<<"\nPipeline Start: "<<std::endl;
    pl.start();

    // Waiting for pipeline stop
    do {
        Sleep(1);
    } while(true);

    HVA_INFO("App terminate");
    return 0;
}
