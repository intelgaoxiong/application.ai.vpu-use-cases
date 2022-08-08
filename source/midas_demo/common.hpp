#ifndef COMMON_HPP
#define COMMON_HPP

struct InferMeta{
    int frameId;            //Frame id
    cv::Mat depthMat;
    float inferFps;         //Inference FPS

    InferMeta():frameId(0),inferFps(0.0) {}
};

#endif
