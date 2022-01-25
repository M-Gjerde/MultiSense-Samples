#include <iostream>

#include <LibMultiSense/include/MultiSense/MultiSenseChannel.hh>
#include <LibMultiSense/include/MultiSense/MultiSenseTypes.hh>
#include <cstring>
#include "MultiSense/details/utility/Exception.hh"
#include "opencv4/opencv2/opencv.hpp"

crl::multisense::Channel *m_channelP;
crl::multisense::image::Header m_disparityHeader;
crl::multisense::image::Header m_disparityCostHeader;
void *m_disparityCostBufferP;
pthread_mutex_t m_disparityCostMutex;

void *m_disparityBufferP;
pthread_mutex_t m_disparityMutex;

int m_sensorRows;
int m_sensorCols;
int m_grabbingRows;
int m_grabbingCols;

bool running = true;

// Data members for rectifying images and reprojecting disparities.
cv::Mat m_leftCalibrationMapX;
cv::Mat m_leftCalibrationMapY;
cv::Mat m_rightCalibrationMapX;
cv::Mat m_rightCalibrationMapY;
cv::Mat m_qMatrix;

// Data members that maintain local pointers to image data that is
// managed by libMultiSense.
crl::multisense::image::Header m_chromaLeftHeader;
crl::multisense::image::Header m_lumaLeftHeader;

// Pointers used to coordinate buffer use with libMultiSense.
// These pointers "reserve" the image data that backs each of the
// header members above.
void* m_chromaLeftBufferP;
void* m_lumaLeftBufferP;

// Additional headers and buffers that always reference data with
// matching frame IDs.  This lets member function
// copyLeftRectifiedRGB() access matching luma and chroma data
// even if the most recent incoming luma header had a different
// frameID from the most recent incoming chroma header.
crl::multisense::image::Header m_matchedChromaLeftHeader;
crl::multisense::image::Header m_matchedLumaLeftHeader;
void* m_matchedChromaLeftBufferP;
void* m_matchedLumaLeftBufferP;
bool m_chromaSupported = true;

// Mutexes to coordinate image access between the callback
// functions (above) and the copy*() functions (also above).
pthread_mutex_t m_lumaAndChromaLeftMutex;

// Simple pthread-based lock class with RAII semantics.
class ScopedLock {
public:
    ScopedLock(pthread_mutex_t *mutexP)
            : m_mutexP(mutexP) { if (m_mutexP) { pthread_mutex_lock(m_mutexP); }}

    ~ScopedLock() { if (m_mutexP) { pthread_mutex_unlock(m_mutexP); }}

private:
    pthread_mutex_t *m_mutexP;
};

float exposure = 0.1;

void SetExpThresh(float ExpThresh)
{
    printf("Setting exposure %f\n", ExpThresh);

    crl::multisense::image::Config cfg;
    crl::multisense::Status status;

    status = m_channelP->getImageConfig(cfg);
    if (crl::multisense::Status_Ok != status) {
        CRL_EXCEPTION("Failed to query image config: %d\n", status);
    }

    cfg.setAutoExposureThresh(ExpThresh);  // Can be 0.0 -> 1.0 ?
    status = m_channelP->setImageConfig(cfg);
    if (crl::multisense::Status_Ok != status) {
        CRL_EXCEPTION("Failed to configure sensor autoexposure threshold: %d\n",
                      status);
    }
}


void updateImage(const crl::multisense::image::Header &sourceHeader,
                 crl::multisense::image::Header &targetHeader,
                 pthread_mutex_t *mutexP,
                 void **bufferP) {
    // Lock access to the local data pointer so we don't clobber it
    // while client code is reading from it.  The lock will be
    // released when when ScopedLock::~ScopedLock() is called on exit
    // from this function.
    ScopedLock lock(mutexP);


    // Return any previously reserved image data to the libMultiSense
    // library.
    if (0 != (*bufferP)) {
        m_channelP->releaseCallbackBuffer(*bufferP);
    }

    // Reserve the data that's backing the new image header.
    *bufferP = m_channelP->reserveCallbackBuffer();

    // And make a local copy, so that client code can access the image
    // later.
    targetHeader = sourceHeader;
    if (targetHeader.source == crl::multisense::Source_Luma_Left){
        cv::Mat m = cv::Mat(272, 1024, CV_16U, (uchar*) targetHeader.imageDataP);
        if (!m.empty()){
            //cv::imshow("luma left", m);
            if (cv::waitKey(1) == 27)
                running = false;


        }
    }

    if (targetHeader.source == crl::multisense::Source_Luma_Right){
        cv::Mat m = cv::Mat(272, 1024, CV_16U, (uchar*) targetHeader.imageDataP);
        if (!m.empty()){
            //cv::imshow("luma right", m);
            if (cv::waitKey(1) == 27)
                running = false;

        }
    }

    if (targetHeader.source == crl::multisense::Source_Disparity){
        cv::Mat m;
        cv::Mat disparityMat(targetHeader.height, targetHeader.width, CV_16UC1,
                         const_cast<void*>(targetHeader.imageDataP));

        // Convert to float, as promised to the calling context.
        disparityMat.convertTo(m, CV_32FC1, 1.0/16.0);

        /*
        cv::Mat cloudMat;
        if (!disparityMat.empty()) {
            reprojectImageTo3D(disparityMat, cloudMat, m_qMatrix, true);
        }
*/

        cv::Mat matdisplay;
        m.convertTo(matdisplay, CV_8UC1, 1);

        if (!matdisplay.empty()){
            cv::normalize(matdisplay, matdisplay, 255, 1, cv::NORM_MINMAX);
            cv::applyColorMap(matdisplay, matdisplay, cv::COLORMAP_JET);
            cv::imshow("disparity", matdisplay);
            if (cv::waitKey(1) == 27)
                running = false;

        }
    }


    if (targetHeader.source == crl::multisense::Source_Disparity_Cost){
        cv::Mat m = cv::Mat(544, 1024, CV_8U, (uchar*) targetHeader.imageDataP);
        if (!m.empty()){
            //cv::imshow("disparity cost", m);
            if (cv::waitKey(1) == 27)
                running = false;

        }
    }


}


void updateLumaAndChroma(const crl::multisense::image::Header& header)
{
    // Lock access to the local data pointers so we don't clobber them
    // while client code is reading from them, or other callbacks are
    // writing to them.
    ScopedLock lock(&(m_lumaAndChromaLeftMutex));


    // Copy a reference to the data that was just passed in.  This
    // lets us buffer the incoming luma or chroma data until matching
    // chroma or luma data is available.  Passing 0 as the mutex
    // pointer prevents updateImage() from trying to lock
    // m_lumaAndChromaLeftMutex again, which would result in a
    // deadlock.
    if(crl::multisense::Source_Luma_Left == header.source) {
        updateImage(header, m_lumaLeftHeader,
                          0, &m_lumaLeftBufferP);
    } else if (crl::multisense::Source_Luma_Right == header.source) {
        updateImage(header, m_chromaLeftHeader,
                          0, &m_chromaLeftBufferP);
    } else {
        CRL_EXCEPTION("Unexpected source type in "
                      "MultiSenseWrapper::updateLumaAndChroma()\n");
    }

    if (m_chromaSupported) {

        // Now that we've buffered a reference to the incoming data
        // reference, check to see if the incoming data makes a complete
        // set of luma/chroma data for a new image.  If so, we'll copy it
        // to secondary storage.
        if (m_lumaLeftHeader.frameId == m_chromaLeftHeader.frameId) {

            // We have a matching luma/chroma pair.  Sanity check to make
            // sure we're not getting repeat frame IDs, or double-copying
            // somewhere.
            if((m_lumaLeftHeader.frameId
                == m_matchedLumaLeftHeader.frameId)
               || (m_chromaLeftHeader.frameId
                   == m_matchedChromaLeftHeader.frameId)) {


                // Sanity check: this should never happen.
                CRL_EXCEPTION(
                        "Logic exception in MultiSenseWrapper::updateLumaAndChroma():\n"
                        "frame IDs appear to be double-counted.\n");
            }

            // Release any previously saved (and now obselete) luma/chroma data.
            if (0 != m_matchedLumaLeftBufferP) {
                m_channelP->releaseCallbackBuffer(
                        m_matchedLumaLeftBufferP);
            }
            if (0 != m_matchedChromaLeftBufferP) {
                m_channelP->releaseCallbackBuffer(
                        m_matchedChromaLeftBufferP);
            }

            // Transfer the new luma and chroma component into secondary
            // storage, where they will be available to the calling
            // context.  save the matched pair.
            m_matchedLumaLeftBufferP = m_lumaLeftBufferP;
            m_matchedLumaLeftHeader = m_lumaLeftHeader;
            m_lumaLeftBufferP = 0;

            m_matchedChromaLeftBufferP = m_chromaLeftBufferP;
            m_matchedChromaLeftHeader = m_chromaLeftHeader;
            m_chromaLeftBufferP = 0;

        }
    } else {

        // Release any previously saved (and now obsolete) luma data.
        if (0 != m_matchedLumaLeftBufferP) {
            m_channelP->releaseCallbackBuffer(
                    m_matchedLumaLeftBufferP);
        }

        // Unit is monochrome, so all we need to use is transfer the new luma component
        // into secondary storage, where it will be available to the calling context.
        m_matchedLumaLeftBufferP = m_lumaLeftBufferP;
        m_matchedLumaLeftHeader = m_lumaLeftHeader;
        m_lumaLeftBufferP = 0;
    }
}

// Calls non-static method updateLumaAndChroma()
void lumaChromaLeftCallback(const crl::multisense::image::Header& header,
                                               void *userDataP)
{
     updateLumaAndChroma(header);
}


// Calls non-static method updateImage()
void disparityCallback(const crl::multisense::image::Header &header,
                       void *userDataP) {

    updateImage(header, m_disparityHeader, &m_disparityMutex, &m_disparityBufferP);
    ScopedLock lock(&m_disparityMutex);

    int k = 2;
}

// Calls non-static method updateImage()
void disparityCostCallback(const crl::multisense::image::Header &header,
                       void *userDataP) {

    updateImage(header, m_disparityCostHeader, &m_disparityCostMutex, &m_disparityCostBufferP);
    ScopedLock lock(&m_disparityCostMutex);

}

// Pick an image size that is supported by the sensor, and is as
// close as possible to the requested image size.
void selectDeviceMode(int32_t RequestedWidth,
                      int32_t RequestedHeight,
                      crl::multisense::DataSource RequiredSources,
                      int32_t &SelectedWidth,
                      int32_t &SelectedHeight) {
    // Query the sensor t see what resolutions are supported.
    std::vector<crl::multisense::system::DeviceMode> modeVector;
    crl::multisense::Status status = m_channelP->getDeviceModes(modeVector);
    if (crl::multisense::Status_Ok != status) {
        CRL_EXCEPTION("Failed to query device modes: %d\n", status);
    }

    // Check each mode in turn, and pick the one that's closest to the
    // requested image size.
    typedef std::vector<crl::multisense::system::DeviceMode>::iterator Iter;
    int32_t bestResidual = -1;
    crl::multisense::system::DeviceMode bestMode;
    for (auto &iter: modeVector) {

        // Only consider modes that support the required data streams.
        if ((iter.supportedDataSources & RequiredSources) == RequiredSources) {
            auto modeWidth = static_cast<int32_t>(iter.width);
            auto modeHeight = static_cast<int32_t>(iter.height);
            int32_t residual = (std::abs(RequestedWidth - modeWidth)
                                + std::abs(RequestedHeight - modeHeight));
            if ((bestResidual < 0) || residual < bestResidual) {
                bestResidual = residual;
                bestMode = iter;
            }
        }
    }

    if (bestResidual < 0) {
        CRL_EXCEPTION("Device does not support the required data sources "
                      "(left luma, left chroma, and disparity)\n");
    }

    SelectedWidth = bestMode.width;
    SelectedHeight = bestMode.height;
}


// Get calibration parameters from the camera
void GetCalibration(float LeftM[3][3], float LeftD[8],
                    float LeftR[3][3], float LeftP[3][4],
                    float RightM[3][3], float RightD[8],
                    float RightR[3][3], float RightP[3][4]) {
    crl::multisense::image::Calibration Cal{};
    int i, j;
    float XScale, YScale;

    // Calibration is for full-size image -- scale for our image size
    XScale = (float) m_grabbingCols / (float) m_sensorCols;
    YScale = (float) m_grabbingRows / (float) m_sensorRows;

    crl::multisense::Status status = m_channelP->getImageCalibration(Cal);
    if (crl::multisense::Status_Ok != status) {
        CRL_EXCEPTION("Failed to query image config: %d\n", status);
    }

    // Scale for image size vs. imager size
    Cal.left.M[0][0] *= XScale;
    Cal.left.M[1][1] *= YScale;
    Cal.left.M[0][2] *= XScale;
    Cal.left.M[1][2] *= YScale;
    Cal.left.P[0][0] *= XScale;
    Cal.left.P[1][1] *= YScale;
    Cal.left.P[0][2] *= XScale;
    Cal.left.P[1][2] *= YScale;
    Cal.left.P[0][3] *= XScale;
    Cal.left.P[1][3] *= YScale;

    // Put everything into OpenCV-friendly formats
    for (j = 0; j < 3; j++) {
        for (i = 0; i < 3; i++) {
            LeftM[j][i] = Cal.left.M[j][i];
            RightM[j][i] = Cal.right.M[j][i];
            LeftR[j][i] = Cal.left.R[j][i];
            RightR[j][i] = Cal.right.R[j][i];
        }
    }
    for (j = 0; j < 3; j++) {
        for (i = 0; i < 4; i++) {
            LeftP[j][i] = Cal.left.P[j][i];
            RightP[j][i] = Cal.right.P[j][i];
        }
    }

    for (i = 0; i < 8; i++) {
        LeftD[i] = Cal.left.D[i];
        RightD[i] = Cal.right.D[i];
    }

    return;
}

// Load calibration information from S-7 camera and calculate
// transform matrices
void InitializeTransforms() {
    float LeftM[3][3], LeftD[8], LeftR[3][3], LeftP[3][4];
    float RightM[3][3], RightD[8], RightR[3][3], RightP[3][4];

    cv::Mat M1, D1, M2, D2;
    cv::Mat R, T;
    cv::Mat R1, R2, P1, P2;

    int i, j;

    crl::multisense::image::Config c;

    crl::multisense::Status status;

    status = m_channelP->getImageConfig(c);
    if (status != crl::multisense::Status_Ok) {
        CRL_EXCEPTION("Failed to getImageConfig() in "
                      "MultiSenseWrapper::InitializeTransforms()\n");
    }

    uint32_t ImgRows = c.height();
    uint32_t ImgCols = c.width();

    m_leftCalibrationMapX = cv::Mat(ImgRows, ImgCols, CV_32F);
    m_leftCalibrationMapY = cv::Mat(ImgRows, ImgCols, CV_32F);
    m_rightCalibrationMapX = cv::Mat(ImgRows, ImgCols, CV_32F);
    m_rightCalibrationMapY = cv::Mat(ImgRows, ImgCols, CV_32F);

    // Allocate space for matricies
    // Mat takes Rows, Cols
    // One-D matricies are setup with 1 row and N columns
    M1 = cv::Mat(3, 3, CV_32F);
    M2 = cv::Mat(3, 3, CV_32F);
    D1 = cv::Mat(1, 8, CV_32F);
    D2 = cv::Mat(1, 8, CV_32F);
    R1 = cv::Mat(3, 3, CV_32F);
    R2 = cv::Mat(3, 3, CV_32F);
    P1 = cv::Mat(3, 4, CV_32F);
    P2 = cv::Mat(3, 4, CV_32F);
    T = cv::Mat(1, 3, CV_32F);
    m_qMatrix = cv::Mat(4, 4, CV_32F, 0.0);

    // Load values from camera
    // This routine also scales the camera values.
    GetCalibration(LeftM, LeftD, LeftR, LeftP,
                   RightM, RightD, RightR, RightP);

    // Copy camera values into cvMats
    for (j = 0; j < 3; j++) {
        for (i = 0; i < 3; i++) {
            M1.at<float>(j, i) = LeftM[j][i];
            M2.at<float>(j, i) = RightM[j][i];
            R1.at<float>(j, i) = LeftR[j][i];
            R2.at<float>(j, i) = RightR[j][i];
        }
    }
    for (j = 0; j < 3; j++) {
        for (i = 0; i < 4; i++) {
            P1.at<float>(j, i) = LeftP[j][i];
            P2.at<float>(j, i) = RightP[j][i];
        }
    }
    for (i = 0; i < 8; i++) {
        // OpenCV only wants 5 elements for D1 but camera returns 8
        D1.at<float>(0, i) = LeftD[i];
        D2.at<float>(0, i) = RightD[i];
    }

    //
    // Compute the Q reprojection matrix for non square pixels. Setting
    // fx = fy will result in the traditional Q matrix
    m_qMatrix.at<float>(0, 0) = c.fy() * c.tx();
    m_qMatrix.at<float>(1, 1) = c.fx() * c.tx();
    m_qMatrix.at<float>(0, 3) = -c.fy() * c.cx() * c.tx();
    m_qMatrix.at<float>(1, 3) = -c.fx() * c.cy() * c.tx();
    m_qMatrix.at<float>(2, 3) = c.fx() * c.fy() * c.tx();
    m_qMatrix.at<float>(3, 2) = -c.fy();
    m_qMatrix.at<float>(3, 3) = c.fy() * (0.0);

    // Compute rectification maps
    initUndistortRectifyMap(M1, D1, R1, P1, m_leftCalibrationMapX.size(), CV_32FC1,
                            m_leftCalibrationMapX, m_leftCalibrationMapY);
    initUndistortRectifyMap(M2, D2, R2, P2, m_rightCalibrationMapX.size(), CV_32FC1,
                            m_rightCalibrationMapX, m_rightCalibrationMapY);
}

// Set camera frames per second
void SetFPS(float FPS) {
    crl::multisense::image::Config cfg;
    // This routine also scales the camera values.
    crl::multisense::Status status;


    status = m_channelP->getImageConfig(cfg);
    if (crl::multisense::Status_Ok != status) {
        CRL_EXCEPTION("Failed to query image config: %d\n", status);
    }

    cfg.setFps(FPS);  // Can be 1.0 -> 30.0
    status = m_channelP->setImageConfig(cfg);
    if (crl::multisense::Status_Ok != status) {
        CRL_EXCEPTION("Failed to configure sensor framerate: %d\n", status);
    }
}




int main() {

    std::cout << "Hello, World!" << std::endl;

    std::string currentAddress = "10.66.171.21";
    int Cols = 1024;
    int Rows = 512;
    float FPS = 30.0;
    // Set up control structures for coordinating threads.
    if (0 != pthread_mutex_init(&m_disparityMutex, NULL)) {
        CRL_EXCEPTION("pthread_mutex_init() failed: %s", strerror(errno));
    }
    if (0 != pthread_mutex_init(&m_disparityCostMutex, NULL)) {
        CRL_EXCEPTION("pthread_mutex_init() failed: %s", strerror(errno));
    }
    // Initialize communications.
    m_channelP = crl::multisense::Channel::Create(currentAddress);
    if (NULL == m_channelP) {
        std::cerr << "Could not start communications with MultiSense sensor.\n";
        std::cerr << "Check network connections and settings?\n";
        std::cerr << "Consult ConfigureNetwork.sh script for hints.\n";
        exit(1);
    }

    // Query version
    crl::multisense::Status status;
    crl::multisense::system::VersionInfo versionInfo;
    status = m_channelP->getVersionInfo(versionInfo);
    if (crl::multisense::Status_Ok != status) {
        CRL_EXCEPTION("failed to query sensor version: %d\n", status);
    }

    crl::multisense::system::DeviceInfo deviceInfo;
    status = m_channelP->getDeviceInfo(deviceInfo);
    if (crl::multisense::Status_Ok != status) {
        CRL_EXCEPTION("failed to query device info: %d\n", status);
    }
    m_sensorRows = deviceInfo.imagerHeight;
    m_sensorCols = deviceInfo.imagerWidth;

    // Choose an image resolution that is a similar as possible to the
    // requested resolution.
    crl::multisense::DataSource RequiredSources =
            crl::multisense::Source_Disparity | crl::multisense::Source_Luma_Left | crl::multisense::Source_Chroma_Left;

    if (deviceInfo.imagerType == crl::multisense::system::DeviceInfo::IMAGER_TYPE_CMV2000_GREY ||
        deviceInfo.imagerType == crl::multisense::system::DeviceInfo::IMAGER_TYPE_CMV4000_GREY) {
        RequiredSources = crl::multisense::Source_Disparity | crl::multisense::Source_Luma_Left;
        m_chromaSupported = false;
    }

    selectDeviceMode(Cols, Rows, RequiredSources, m_grabbingCols, m_grabbingRows);

    // Configure the sensor.
    crl::multisense::image::Config cfg;
    status = m_channelP->getImageConfig(cfg);
    if (crl::multisense::Status_Ok != status) {
        CRL_EXCEPTION("Failed to query image config: %d\n", status);
    }

    cfg.setResolution(m_grabbingCols, m_grabbingRows);
    status = m_channelP->setImageConfig(cfg);
    if (crl::multisense::Status_Ok != status) {
        CRL_EXCEPTION("Failed to configure sensor resolution and framerate\n");
    }

    // Change MTU
    if (crl::multisense::Status_Ok != m_channelP->setMtu(7200))
        fprintf(stderr, "failed to set MTU to 7200\n");

    // Change trigger source
    status = m_channelP->setTriggerSource(crl::multisense::Trigger_Internal);
    if (crl::multisense::Status_Ok != status)
        fprintf(stderr, "Failed to set trigger source, Error %d\n", status);

    // Set framerate.
    SetFPS(FPS);

    // Read calibration data and compute rectification maps.
    InitializeTransforms();

    // Initialize frameId's so image data can be copied properly
    // on startup. I.e. prevent the case where the image frame ids and
    // the matched frame ids are both 0.
    m_lumaLeftHeader.frameId = -1;
    m_chromaLeftHeader.frameId = -1;
    m_matchedLumaLeftHeader.frameId = -1;
    m_matchedChromaLeftHeader.frameId = -1;

    status = m_channelP->startStreams(crl::multisense::Source_Disparity) != crl::multisense::Status_Ok;
    if (status != crl::multisense::Status_Ok)
        CRL_EXCEPTION("Unable to start Source_Disparity stream: %s",
                      strerror(errno));

    status = m_channelP->startStreams(crl::multisense::Source_Luma_Left);
    if (status != crl::multisense::Status_Ok)
        CRL_EXCEPTION("Unable to start Source_Chroma_Left stream: %s",
                      strerror(errno));

    status = m_channelP->startStreams(crl::multisense::Source_Luma_Right);
    if (status != crl::multisense::Status_Ok)
        CRL_EXCEPTION("Unable to start Source_Chroma_Left stream: %s",
                      strerror(errno));
    status = m_channelP->startStreams(crl::multisense::Source_Disparity_Cost);
    if (status != crl::multisense::Status_Ok)
        CRL_EXCEPTION("Unable to start Source_Chroma_Left stream: %s",
                      strerror(errno));


    m_channelP->addIsolatedCallback(disparityCallback, crl::multisense::Source_Disparity);
    m_channelP->addIsolatedCallback(disparityCostCallback, crl::multisense::Source_Disparity_Cost);

    m_channelP->addIsolatedCallback(lumaChromaLeftCallback, crl::multisense::Source_Luma_Left | crl::multisense::Source_Luma_Right);

    while (running);

    cv::destroyAllWindows();

    return 0;
}
