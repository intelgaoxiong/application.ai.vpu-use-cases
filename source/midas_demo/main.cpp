#include <FrameReaderNode.hpp>
#include <OdInferNode.hpp>
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
    FRConfig.input = "C:/work/sample-videos/car-detection.mp4";
    FRConfig.infiniteLoop = true;
    FRConfig.readType = read_type::safe;
    auto& FRNode = pl.setSource(std::make_shared<FrameReaderNode>(0, 1, 1, FRConfig), "FRNode");
    FRNode.configBatch(batchingConfig);

    //Detection node
    ODInferNode::Config ODConfig;
    ODConfig.modelFileName = "C:/work/yolo-v2-tiny/FP16-INT8/yolo-v2-tiny-ava-0001.xml";
    ODConfig.architectureType = "yolo";
    ODConfig.nstreams = "1";
    ODConfig.nireq = 4;
    auto& OdNode = pl.addNode(std::make_shared<ODInferNode>(1, 1, 1, ODConfig), "OdNode");
    OdNode.configBatch(batchingConfig);

    //Sink node
    DisplayNode::Config DispConfig;
    auto& DispNode = pl.addNode(std::make_shared<DisplayNode>(1, 0, 1, DispConfig), "DispNode");
    DispNode.configBatch(batchingConfig);

    // Link nodes
    pl.linkNode("FRNode", 0, "OdNode", 0);
    pl.linkNode("OdNode", 0, "DispNode", 0);

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
