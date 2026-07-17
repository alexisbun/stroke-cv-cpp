# stroke-cv-app
Project includes my cpp library (cpp_lib), a sandbox UI (fluttter_ui), and my current working UI (working_ui). 

To run on a physical Android device:

1. Clone the repository (git clone https://github.com/alexisbun/stroke-cv-cpp.git to your prefered directory)
2. Build and compile the library in 'cpp_lib' with cmake --preset android-arm64-debug and then cmake --build build/arm64-v8a-debug/
3. Run 'dart run ffigen' inside of flutter_ui (sandbox UI)
4. Run the project with flutter run.

Prerequisites:
- Physical Android device with an arm64-v8a architecture and at least API level 30.
- Configured Flutter development enviroment with an Andrioid Sdk (https://docs.flutter.dev/install/quick, https://docs.flutter.dev/platform-integration/android/setup)
- The 'ANDRIOID_NDK_HOME' enviroment variable set (for example, export ANDROID_NDK_HOME="[path]/Android/Sdk/ndk/30.0.14904198")

# build face_landmarker

1. Apply patch in patches/mediapipe.patch
2. 
```
bazel build --config=android_arm64   --repo_env=HERMETIC_PYTHON_VERSION=3.12   --sandbox_writable_path=/home/alexis/.cache/ccache   -c opt --linkopt -s --strip always   //mediapipe/tasks/c/vision/face_landmarker:libface_landmarker.so
```
