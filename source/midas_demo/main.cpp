#include <FrameReaderNode.hpp>
#include <MidasInferNode.hpp>
#include <DisplayNode.hpp>
#include <Windows.h>

int main(int argc, char* argv[]){
    hvaLogger.setLogLevel(hva::hvaLogger_t::LogLevel::WARNING);

    HVA_INFO("App Start:");

    hva::hvaPipeline_t pl;
    pl.registerEvent(hvaEvent_EOF);

    hva::hvaBatchingConfig_t batchingConfig;
    batchingConfig.batchingPolicy = hva::hvaBatchingConfig_t::BatchingWithStream;
    batchingConfig.batchSize = 1;
    batchingConfig.streamNum = 1;
    batchingConfig.threadNumPerBatch = 1;

    //Source node
    FrameReaderNode::Config FRConfig;
    //FRConfig.input = "C:/xiong/demo_dev/sample-videos/car-detection.mp4";
    FRConfig.input = "C:/xiong/demo_dev/sample-videos/worker-zone-detection.mp4";
    FRConfig.infiniteLoop = true;
    FRConfig.readType = read_type::safe;
    auto& FRNode = pl.setSource(std::make_shared<FrameReaderNode>(0, 1, 1, FRConfig), "FRNode");
    FRNode.configBatch(batchingConfig);

    //Midas node
    MidasInferNode::Config MidasConfig;
    MidasConfig.modelFileName = "C:/xiong/demo_dev/compiled-models/midas/0808-midas_512x288-ww32-4.blob";
    MidasConfig.nireq = 4;
    MidasConfig.inferMode = "async";
    MidasConfig.nn_width = 512;
    MidasConfig.nn_height = 288;
    auto& MidasNode = pl.addNode(std::make_shared<MidasInferNode>(1, 1, 1, MidasConfig), "MidasNode");
    MidasNode.configBatch(batchingConfig);

    //Sink node
    DisplayNode::Config DispConfig;
    auto& DispNode = pl.addNode(std::make_shared<DisplayNode>(1, 0, 1, DispConfig), "DispNode");
    DispNode.configBatch(batchingConfig);

    // Link nodes
    pl.linkNode("FRNode", 0, "MidasNode", 0);
    pl.linkNode("MidasNode", 0, "DispNode", 0);

    // Start pipeline
    pl.prepare();

    std::cout<<"\nPipeline Start: "<<std::endl;
    pl.start();

    //block here until EOF event is received
    pl.waitForEvent(hvaEvent_EOF);

    pl.stop();

    HVA_INFO("App terminate");
    return 0;
}
