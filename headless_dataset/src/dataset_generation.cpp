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

#include "mediapipe_face_mesh.h"
#include "stroke_model_inference.h"
#include "face_mesh_triangles.h"
#include "headless_gl_manager.h"

bool GenerateTriplet(const std:: string& originalPath, const std:: string& syntheticPath, 
    const std:: string& warpedPath, const std:: string& filename,  int width=256, int height=256, 
    FaceMesh facemesh, StrokeModelInference strokeInference, HeadlessGLManager glContext) 
{

}

int main() {

    return 0;
}