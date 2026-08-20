#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "face_mesh.h"
#include "stroke_model_inference.h"
#include "face_mesh_triangles.h"
#include "headless_gl_manager.h"

struct BoundingBox {
    int x, y, size;
}; // for cropping the ROI

bool GenerateTriplet(const std:: string& originalPath, const std:: string& syntheticPath, 
    const std:: string& warpedPath, const std:: string& filename,  int width=256, int height=256, 
    FaceMesh facemesh, StrokeModelInference strokeInference, HeadlessGLManager glContext) 
{
    // Plan:
    // Load images using stb_image
    // Pass to OpenGL context
    // Detect landmarks
    // Predict displacement mesh
    // Crop the faces and resize to 256x256
    // Write to a directory (images will be placed in, either): original, live_portrait, gcn_warped
}

BoundingBox ComputeFaceROI(const std::vector<int>& landmarks, int width, int height) // replace int with MpNormalizedLandmark
{  
    BoundingBox x {1, 2, 3};
    return x; // dummy return value
}

void CropAndResize() 
{
}

int main() {

    return 0;
}