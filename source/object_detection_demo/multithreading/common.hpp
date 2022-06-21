#ifndef COMMON_HPP
#define COMMON_HPP

struct InferMeta{
    int totalROI;           //Total ROI number
    int frameId;            //Frame id
    DetectionResult    detResult;
    float inferFps;         //Inference FPS

    int cntDetection;       //Detection count
    int cntClassification;  //Classification count
    float detectionFps;     //detection FPS
    float classificationFps;//classification FPS
    InferMeta():totalROI(0),frameId(0),inferFps(0.0),cntDetection(0),cntClassification(0),detectionFps(0.0),classificationFps(0.0) {}
};

#endif
