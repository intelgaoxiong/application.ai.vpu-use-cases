#ifndef COMMON_HPP
#define COMMON_HPP

struct InferMeta{
    int frameId;            //Frame id
    cv::Mat depthMat;
    float inferFps;         //Inference FPS
    std::chrono::steady_clock::time_point timeStamp;  //Infer start time

    InferMeta():frameId(0),inferFps(0.0) {}
};

#endif
