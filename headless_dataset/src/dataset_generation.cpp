#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <map>
#include <regex>
#include <iomanip>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "face_mesh.h"
#include "stroke_model_inference.h"
#include "face_mesh_triangles.h"
#include "headless_gl_manager.h"
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

struct BoundingBox {
    int x = 0;
    int y = 0;
    int size = 0;
}; // for cropping the ROI

BoundingBox ComputeFaceROI(const std::vector<MpNormalizedLandmark>& landmarks, int imgWidth, int imgHeight) 
{  
    if (landmarks.empty() || imgWidth <= 0 || imgHeight <= 0) {
        return BoundingBox {0, 0, 0};
    }

    // computation of boundry indicies
    float minX = 1.0f;
    float maxX = 0.0f;
    float minY = 1.0f;
    float maxY = 0.0f;

    for (const auto& lm : landmarks) {
        minX = std::min(minX, lm.x);
        maxX = std::max(maxX, lm.x);
        minY = std::min(minY, lm.y);
        maxY = std::max(maxY, lm.y);
    }

    // logic for padding the image 50%.
    float faceW = (maxX - minX) * static_cast<float>(imgWidth);
    float faceH = (maxY - minY) * static_cast<float>(imgHeight);
    int size = static_cast<int>(std::round(std::max(faceW, faceH) * 1.5f));

    size = std::min({size, imgWidth, imgHeight});
    
    float rawCenterX = (minX + maxX) * 0.5f * static_cast<float>(imgWidth);
    float rawCenterY = (minY + maxY) * 0.5f * static_cast<float>(imgHeight);

    int centerX = static_cast<int>(std::round(rawCenterX));
    int centerY = static_cast<int>(std::round(rawCenterY));

    int x = std::clamp(centerX - size / 2, 0, imgWidth - size);
    int y = std::clamp(centerY - size / 2, 0, imgHeight - size);

    return BoundingBox{x, y, size}; 
}

void CropAndResize(
    const uint8_t* srcRgba,
    int srcW,
    int srcH,
    const BoundingBox& bbox,
    uint8_t* dstRgba,
    int dstW,
    int dstH
)
{
    if (!srcRgba || !dstRgba || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0 || bbox.size <= 0) {
        return;
    }

    float scale = static_cast<float>(bbox.size) / static_cast<float>(dstW);

    for (int dstY = 0; dstY < dstH; ++dstY) {
        float srcY = static_cast<float>(bbox.y) + (static_cast<float>(dstY) + 0.5f) * scale - 0.5f;

        int y0 = static_cast<int>(std::floor(srcY));
        int y1 = y0 + 1;

        float fy = srcY - static_cast<float>(y0);

        y0 = std::clamp(y0, 0, srcH - 1);
        y1 = std::clamp(y1, 0, srcH - 1);

        for (int dstX = 0; dstX < dstW; ++dstX) {
            float srcX = static_cast<float>(bbox.x) + (static_cast<float>(dstX) + 0.5f) * scale - 0.5f;

            int x0 = static_cast<int>(std::floor(srcX));
            int x1 = x0 + 1;

            float fx = srcX - static_cast<float>(x0);

            x0 = std::clamp(x0, 0, srcW - 1);
            x1 = std::clamp(x1, 0, srcW - 1);

            const uint8_t* p00 = srcRgba + (y0 * srcW + x0) * 4;
            const uint8_t* p10 = srcRgba + (y0 * srcW + x1) * 4;
            const uint8_t* p01 = srcRgba + (y1 * srcW + x0) * 4;
            const uint8_t* p11 = srcRgba + (y1 * srcW + x1) * 4;

            uint8_t* dstPixel = dstRgba + (dstY * dstW + dstX) * 4;

            for (int c = 0; c < 4; ++c) {
                float topVal   = p00[c] * (1.0f - fx) + p10[c] * fx;
                float botVal   = p01[c] * (1.0f - fx) + p11[c] * fx;
                float finalVal = topVal * (1.0f - fy) + botVal * fy;

                dstPixel[c] = static_cast<uint8_t>(std::clamp(std::round(finalVal), 0.0f, 255.0f));
            }
        }
    }
}


bool GenerateTriplet(
    const std::string& originalPath, 
    const std::string& syntheticPath, // Synthetic image via Live Portrait (which needs to be cropped as well)
    const std::string& outOriginalPath,
    const std::string& outWarpedPath, 
    const std::string& outSyntheticPath,
    const std::string& filename, 
    FaceMesh& faceMesh, 
    StrokeModelInference& strokeInference, 
    HeadlessGLManager& glContext, 
    int cropDim=256) 
{
    // Load images using stb_image
    int origWidth = 0;
    int origHeight = 0;
    int origChannels = 0; 

    uint8_t* origData = stbi_load(originalPath.c_str(), &origWidth, &origHeight, &origChannels, 4);
    if (!origData) {
        spdlog::info("Failed to load image {}", originalPath);
        return false;
    }

    int synthWidth = 0;
    int synthHeight = 0;
    int synthChannels = 0;

    uint8_t* synthData = stbi_load(syntheticPath.c_str(), &synthWidth, &synthHeight, &synthChannels, 4);
    if (!synthData) {
        spdlog::info("Failed to load image {}", syntheticPath);
        return false;
    }
    // Detect landmarks
    std::vector<MpNormalizedLandmark> rawLandmarks;
    if (!faceMesh.DetectLandmarks(origData, origWidth, origHeight, rawLandmarks)) {
        spdlog::info("Skipping {}: FaceMesh landmark detection failed", filename);
        stbi_image_free(origData);
        stbi_image_free(synthData);
        return false;
    }

    std::vector<MpNormalizedLandmark> synthLandmarks;
    if (!faceMesh.DetectLandmarks(synthData, synthWidth, synthHeight, synthLandmarks)) {
        spdlog::warn("Skipping {}: Synthetic FaceMesh detection failed", filename);
        stbi_image_free(origData);
        stbi_image_free(synthData);
        return false;
    }

    // Predict displacement mesh via GCN
    std::vector<MpNormalizedLandmark> strokeLandmarks;
    if (!strokeInference.PredictStrokeLandmarks(rawLandmarks, strokeLandmarks)) {
        spdlog::error("GCN landmark inference failed for: {}", filename);
        stbi_image_free(origData);
        stbi_image_free(synthData);
        return false;
    }

    // Pass to OpenGL context
    std::vector<float> meshVertexData(478 * 4);
    for (size_t i = 0; i < 478; ++i) {
        float displacedNdcX = strokeLandmarks[i].x * 2.0f - 1.0f;
        float displacedNdcY = 1.0f - strokeLandmarks[i].y * 2.0f; 
        float origU = rawLandmarks[i].x;
        float origV = rawLandmarks[i].y;

        meshVertexData[i * 4 + 0] = displacedNdcX;
        meshVertexData[i * 4 + 1] = displacedNdcY;
        meshVertexData[i * 4 + 2] = origU;
        meshVertexData[i * 4 + 3] = origV;
    }
    
    std::vector<uint8_t> warpedImage;
    if(!glContext.RenderWarpedImage(origData, origWidth, origHeight, meshVertexData, warpedImage)) {
        stbi_image_free(origData);
        stbi_image_free(synthData);
        return false;
    }

    // Crop the faces and resize to 256x256
    BoundingBox bboxOrig = ComputeFaceROI(rawLandmarks, origWidth, origHeight);
    BoundingBox bboxSynth = ComputeFaceROI(synthLandmarks, synthWidth, synthHeight);

    std::vector<uint8_t> cropOrig(cropDim * cropDim * 4);
    std::vector<uint8_t> cropWarp(cropDim * cropDim * 4);
    std::vector<uint8_t> cropSynth(cropDim * cropDim * 4);
    
    CropAndResize(origData, origWidth, origHeight, bboxOrig, cropOrig.data(), cropDim, cropDim);
    CropAndResize(warpedImage.data(), origWidth, origHeight, bboxOrig, cropWarp.data(), cropDim, cropDim);
    CropAndResize(synthData, synthWidth, synthHeight, bboxSynth, cropSynth.data(), cropDim, cropDim);

    stbi_image_free(origData);
    stbi_image_free(synthData);

    std::string outOrigFile  = (fs::path(outOriginalPath)  / (filename + ".png")).string();
    std::string outWarpFile  = (fs::path(outWarpedPath)    / (filename + ".png")).string();
    std::string outSynthFile = (fs::path(outSyntheticPath) / (filename + ".png")).string();

    stbi_write_png(outOrigFile.c_str(),  cropDim, cropDim, 4, cropOrig.data(),  cropDim * 4);
    stbi_write_png(outWarpFile.c_str(),  cropDim, cropDim, 4, cropWarp.data(),  cropDim * 4);
    stbi_write_png(outSynthFile.c_str(), cropDim, cropDim, 4, cropSynth.data(), cropDim * 4);

    return true;
}



int main() {
    spdlog::info("Starting headless dataset pipeline");

    const std::string inputDir = "/home/alexis/Desktop/synthetic-dataset/ComfyUI/output/cfd_target";
    const std::string outputDir = "/home/alexis/Desktop/synthetic-dataset/headless_dataset";

    const std::string facelandmarkerPath = "/home/alexis/git/stroke-cv-cpp/flutter_ui/assets/face_landmarker.task";
    const std::string onnxModelPath = "/home/alexis/git/stroke-cv-cpp/flutter_ui/assets/landmark_displacement_model.onnx";

    const std::string outOrigDir = outputDir + "/original_cropped";
    const std::string outWarpDir = outputDir + "/warped_cropped";
    const std::string outSynthDir = outputDir + "/liveportrait_cropped";

    fs::create_directories(outOrigDir);
    fs::create_directories(outWarpDir);
    fs::create_directories(outSynthDir);

    spdlog::info("Initializing MediaPipe Face Landmarker...");
    FaceMesh faceMesh;

    if (!faceMesh.InitializeFaceLandmarker(facelandmarkerPath)) {
        spdlog::error("MediaPipe initialization failed: {}", faceMesh.GetError());
        return -1;
    }

    spdlog::info("Initializing ONNX GCN Inference Engine...");
    StrokeModelInference strokeInference;
    if (!strokeInference.InitializeModel(onnxModelPath)) {
        spdlog::error("ONNX model initialization failed: {}", strokeInference.GetError());
        return -1;
    }

    spdlog::info("Initializing Headless EGL & OpenGL Context...");
    HeadlessGLManager glManager;
    if (!glManager.InitGLManager()) {
        spdlog::error("Headless OpenGL initialization failed.");
        return -1;
    }

    spdlog::info("Scanning input directory for paired images: {}", inputDir);
    std::regex filePattern(R"(result_(\d+)_.*)");
    std::map<int, fs::path> indexedFiles;

    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (!entry.is_regular_file()) continue;
        std::string filename = entry.path().filename().string();
        std::smatch match;
        if (std::regex_match(filename, match, filePattern)) {
            int idx = std::stoi(match[1].str());
            indexedFiles[idx] = entry.path();
        }
    }

    spdlog::info("Found {} total indexed files in {}", indexedFiles.size(), inputDir);

    spdlog::info("Starting batch triplet generation...");
    int processedCount = 0;
    int failureCount = 0;
    auto startTime = std::chrono::high_resolution_clock::now();

    for (const auto& [idx, origPath] : indexedFiles) {
        if (idx % 2 == 0) continue; // Process pairs starting from odd indices (original images)

        int synthIdx = idx + 1;
        auto it = indexedFiles.find(synthIdx);
        if (it == indexedFiles.end()) {
            spdlog::warn("Missing synthetic pair for index {} (expected {})", idx, synthIdx);
            failureCount++;
            continue;
        }

        int pairIndex = (idx + 1) / 2;
        std::ostringstream oss;
        oss << "sample_" << std::setw(5) << std::setfill('0') << pairIndex;
        std::string filename = oss.str();

        std::string origFile = origPath.string();
        std::string synthFile = it->second.string();

        bool success = GenerateTriplet(
            origFile,
            synthFile,
            outOrigDir,
            outWarpDir,
            outSynthDir,
            filename,
            faceMesh,
            strokeInference,
            glManager,
            256
        );

        if (success) {
            processedCount++;
            if (processedCount % 25 == 0) {
                spdlog::info("Progress: Generated {} triplets...", processedCount);
            }
        } else {
            failureCount++;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;
    spdlog::info("=========================================");
    spdlog::info("Dataset Generation Complete");
    spdlog::info("Successfully Generated: {} triplets", processedCount);
    spdlog::info("Failed / Skipped:       {} samples", failureCount);
    spdlog::info("Total Elapsed Time:     {:.2f} seconds", elapsed.count());
    spdlog::info("=========================================");

    return 0;
}