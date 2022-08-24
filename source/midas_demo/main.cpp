#include <nodes/include/FrameReaderNode.hpp>
#include <nodes/include/MidasInferNode.hpp>
#include <nodes/include/DisplayNode.hpp>
#include <nodes/include/OdInferNode.hpp>
#include <Windows.h>
#include <gflags/gflags.h>

static const char help_message[] = "Print a usage message.";
//Source Node config
static const char input_message[] = "Required. An input to process. The input must be a single image, a folder of "
    "images, video file or camera id.";
static const char loop_message[] = "Optional. Infinite loop source if true";
static const char readtype_message[] = "Optional. Readtype:safe or efficient";
static const char masfps_message[] = "Optional. Max source frame rate";

DEFINE_bool(h, false, help_message);
DEFINE_string(i, "", input_message);
DEFINE_bool(loop, true, loop_message);
DEFINE_string(rt, "safe", readtype_message);
DEFINE_string(maxFPS, "", masfps_message);

//Infer Node config
static const char model_message[] = "Required. Path to a blob file.";
static const char nireq_message[] = "Optional. Number of infer requests. If this option is omitted, number of infer "
                                    "requests is determined automatically.";
static const char api_message[] = "Optional. Infer mode: async or sync. Async is by default.";

DEFINE_string(m, "", model_message);
DEFINE_uint32(nireq, 4, nireq_message);
DEFINE_string(api, "async", api_message);

//Display Node config
static const char res_message[] = "Optional. Display resolution: (width_value)x(height_value).";
DEFINE_string(displayRes, "", res_message);

static void showUsage() {
    std::cout << std::endl;
    std::cout << "Midas_demo [OPTION]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << std::endl;
    std::cout << "    -h                        " << help_message << std::endl;
    std::cout << "    -i                        " << input_message << std::endl;
    std::cout << "    -loop                     " << loop_message << std::endl;
    std::cout << "    -rt                        " << readtype_message << std::endl;
    std::cout << "    -maxFPS                        " << masfps_message << std::endl;

    std::cout << "    -m \"<path>\"               " << model_message << std::endl;
    std::cout << "    -nireq \"<integer>\"        " << nireq_message << std::endl;
    std::cout << "    -api                     " << api_message << std::endl;

    std::cout << "    -displayRes                     " << res_message << std::endl;
}

bool ParseAndCheckCommandLine(int argc, char* argv[]) {
    // ---------------------------Parsing and validation of input args--------------------------------------
    gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);
    if (FLAGS_h) {
        showUsage();
        showAvailableDevices();
        return false;
    }

    if (FLAGS_i.empty()) {
        throw std::logic_error("Parameter -i is not set");
    }

    if (FLAGS_m.empty()) {
        throw std::logic_error("Parameter -m is not set");
    }
    return true;
}


int main(int argc, char* argv[]){
    bool parse;
    try{
        parse = ParseAndCheckCommandLine(argc, argv);
        if (!parse)
            return 0;
    }
    catch (const std::exception& error)
    {
        slog::err << error.what() << slog::endl;
        return 1;
    }

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
    //FRConfig.input = "0"; //Camera source
    //FRConfig.input = "C:/xiong/demo_dev/sample-videos/car-detection.mp4";
    FRConfig.input = FLAGS_i;
    FRConfig.infiniteLoop = FLAGS_loop;
    FRConfig.readType = (FLAGS_rt == "safe") ? read_type::safe : read_type::efficient;
    FRConfig.maxFPS = std::stod(FLAGS_maxFPS);
    auto& FRNode = pl.setSource(std::make_shared<FrameReaderNode>(0, 1, 1, FRConfig), "FRNode");
    FRNode.configBatch(batchingConfig);

    //Midas node
    MidasInferNode::Config MidasConfig;
    MidasConfig.modelFileName = FLAGS_m;
    MidasConfig.nireq = FLAGS_nireq;
    MidasConfig.inferMode = FLAGS_api;
    auto& MidasNode = pl.addNode(std::make_shared<MidasInferNode>(1, 1, 1, MidasConfig), "MidasNode");
    MidasNode.configBatch(batchingConfig);

    //Detection node
    ODInferNode::Config ODConfig;
    ODConfig.modelFileName = "C:/work/yolo-v2-tiny/FP16-INT8/yolo-v2-tiny-ava-0001.xml";
    ODConfig.architectureType = "yolo";
    ODConfig.nstreams = "1";
    ODConfig.nireq = FLAGS_nireq;
    auto& OdNode = pl.addNode(std::make_shared<ODInferNode>(1, 1, 1, ODConfig), "OdNode");
    OdNode.configBatch(batchingConfig);

    //Sink node
    DisplayNode::Config DispConfig;
    DispConfig.dispRes = FLAGS_displayRes;
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
