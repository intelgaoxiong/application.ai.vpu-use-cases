#include <Vis3DNode.hpp>
#include <pipelines/metadata.h>


Vis3DNode::Vis3DNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const Config& config) :
    hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), m_cfg(config) {

}

std::shared_ptr<hva::hvaNodeWorker_t> Vis3DNode::createNodeWorker() const {
    return std::shared_ptr<hva::hvaNodeWorker_t>(new Vis3DNodeWorker((Vis3DNode*)this, m_cfg));
}

Vis3DNodeWorker::Vis3DNodeWorker(hva::hvaNode_t* parentNode, const Vis3DNode::Config& config) :hva::hvaNodeWorker_t(parentNode) {
    size_t found = config.dispRes.find("x");
    if (found == std::string::npos) {
        m_displayResolution = cv::Size{ 0, 0 };
    }
    else {
        m_displayResolution = cv::Size{
            std::stoi(config.dispRes.substr(0, found)),
            std::stoi(config.dispRes.substr(found + 1, config.dispRes.length())) };
    }

    printf("Vis3D resolution %d x %d", m_displayResolution.width, m_displayResolution.height);
}

static const Eigen::Matrix4d flip_transformation = Eigen::Matrix4d({
        {1, 0, 0, 0},
        {0, -1, 0, 0},
        {0, 0, -1, 0},
        {0, 0, 0, 1},
    });

void Vis3DNodeWorker::process(std::size_t batchIdx) {
    std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput = hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    if (vInput.size() != 0) {
        HVA_DEBUG("Vid3DNode received blob with frameid %u and streamid %u", vInput[0]->frameId, vInput[0]->streamId);

        auto eof = vInput[0]->get<int, ImageMetaData>(1)->getPtr();
        if (!(*eof)) {
            auto cvFrame = vInput[0]->get<int, ImageMetaData>(1)->getMeta()->img;
            auto frameResolution = cvFrame.size();
            HVA_DEBUG("Vid3DNode[%u] frameResolution %d x %d", vInput[0]->streamId, frameResolution.width, frameResolution.height);

            auto ptrInferMeta = vInput[0]->get<int, InferMeta>(0)->getMeta();
            cv::Mat depthFrame;
            cv::resize(ptrInferMeta->depthMat, depthFrame, cv::Size(frameResolution.width, frameResolution.height), 0, 0, cv::INTER_CUBIC);
            
            m_vis.RemoveGeometry(m_pcd);
            auto imgColor = std::shared_ptr<open3d::geometry::Image> (new open3d::geometry::Image());
            imgColor->Prepare(frameResolution.width, frameResolution.height, 3, 1);
            memcpy(imgColor->data_.data(), cvFrame.data, frameResolution.width * frameResolution.height * 3);
            auto imgDepth = std::shared_ptr<open3d::geometry::Image>(new open3d::geometry::Image());
            imgDepth->Prepare(frameResolution.width, frameResolution.height, 1, 1);
            memcpy(imgDepth->data_.data(), depthFrame.data, frameResolution.width * frameResolution.height * 1);
            auto imgRGBD = open3d::geometry::RGBDImage::CreateFromColorAndDepth(*imgColor, *imgDepth);
            
            auto camera_intrinsic = open3d::camera::PinholeCameraIntrinsic(open3d::camera::PinholeCameraIntrinsicParameters::PrimeSenseDefault);
            m_pcd = open3d::geometry::PointCloud::CreateFromRGBDImage(*imgRGBD, camera_intrinsic);
            m_pcd->Transform(flip_transformation);
            m_vis.AddGeometry(m_pcd);
            //m_vis.UpdateGeometry(m_pcd);
            m_vis.PollEvents();
            m_vis.UpdateRender();
        }
    }
}

void Vis3DNodeWorker::init() {
}

void Vis3DNodeWorker::deinit() {
}

void Vis3DNodeWorker::processByFirstRun(std::size_t batchIdx) {
    m_vis.CreateVisualizerWindow();

    auto imgColor = open3d::geometry::Image();
    imgColor.Prepare(m_displayResolution.width, m_displayResolution.height, 3, 1);
    auto imgDepth = open3d::geometry::Image();
    imgDepth.Prepare(m_displayResolution.width, m_displayResolution.height, 1, 1);
    auto imgRGBD = open3d::geometry::RGBDImage::CreateFromColorAndDepth(imgColor, imgDepth);
    
    auto camera_intrinsic = open3d::camera::PinholeCameraIntrinsic(open3d::camera::PinholeCameraIntrinsicParameters::PrimeSenseDefault);
    m_pcd = open3d::geometry::PointCloud::CreateFromRGBDImage(*imgRGBD, camera_intrinsic);
    
    m_vis.AddGeometry(m_pcd);

}

void Vis3DNodeWorker::processByLastRun(std::size_t batchIdx) {
}