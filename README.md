# stroke-cv-app
Project includes my cpp library (cpp_lib), a sandbox UI (fluttter_ui), and my current working UI (working_ui). 

To run on a physical Android device:

1. Clone the repository (git clone https://github.com/alexisbun/stroke-cv-cpp.git to your prefered directory)
2. Build and compile the library in 'cpp_lib' with cmake --preset android-arm64-debug and then cmake --build build/arm64-v8a-debug/
3. Run 'dart run ffigen' inside of flutter_ui (sandbox UI)
5. Run the project with flutter run.

Prerequisites:
- Physical Android device with an arm64-v8a architecture
- Configured Flutter development enviroment with an Andrioid Sdk (https://docs.flutter.dev/install/quick, https://docs.flutter.dev/platform-integration/android/setup)
- The 'ANDRIOID_NDK_HOME' enviroment variable set (for example, export ANDROID_NDK_HOME="[path]/Android/Sdk/ndk/30.0.14904198")

