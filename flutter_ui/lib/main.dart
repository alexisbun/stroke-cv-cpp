import 'dart:ffi' as ffi;
import 'dart:io';

import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'camera_bindings.dart' as bindings;

// Fundementally, you need to include a file that contains a function
// that will allow you to invoke a method from Java called
// TextureRegistry.SurfaceProducer, in order for you to obtain the textureID
// for the backend texture the Texture widget refers to.  This method will
// create a SurfaceTexture which is the consumer of an
// image buffer. The Surface object (from the SurfaceTexture: by invoking
// .getSurface()), once registered and "bound" to the Texture widget can be
// passed as an argument (in C++) to ANativeWindow_fromSurface().

// ANativeWindow_fromSurface() generates a raw pointer to an object called
// ANativeWindow. ANativeWindow is required to connect the raw pixel date
// produced by the camera to the OpenGL External Texture. At a fundemental
// level, ANativeWindow will serve as the connection between the Texture widget
// and the OpenGL External Texture. All of the steps explained above are
// required to initialize it.

// CAMERA CLASS (will be in seperate file when migrating to working_ui)
class Camera extends StatefulWidget {
  const Camera({super.key});

  @override
  State<Camera> createState() => _CameraState();
}

class _CameraState extends State<Camera> {
  static const _platform = MethodChannel('texture-backend');
  int? _textureID;

  @override
  void initState() {
    super.initState();
    _initTexture();
  }

  @override
  void dispose() {
    if (_textureID != null) {
      _platform.invokeMethod('disposeTexture', {'textureid': _textureID});
    }
    super.dispose();
  }

  Future<void> _initTexture() async {
    final int id = await _platform.invokeMethod('textureid');

    if (!mounted) return;
    // Previous line protects app from crashing since setState() would not work.
    // If we leave the screen (exit the app, press the "back button" or navigate)
    // before await _platform.invokeMethod('textureid') completes, then the
    // widget would be destroyed and this exception would be thrown since the
    // execution thread would still continue without textureid initialized:
    // Unhandled Exception State.setState() called after dispose().

    setState(() {
      _textureID = id;
    });
  }

  @override
  Widget build(BuildContext context) {
    if (_textureID == null) {
      return Center(child: CircularProgressIndicator());
    }
    return Texture(textureId: _textureID!);
  }
}

// MAIN
void main() {
  runApp(const MainApp());
}

class MainApp extends StatelessWidget {
  const MainApp({super.key});

  ffi.DynamicLibrary _loadDynamicLibrary() {
    if (Platform.isAndroid) {
      return ffi.DynamicLibrary.open('libStrokeCVLib.so');
    } else if (Platform.isIOS || Platform.isMacOS) {
      // On iOS/macOS, if statically linked, use process().
      return ffi.DynamicLibrary.process();
    } else if (Platform.isWindows) {
      return ffi.DynamicLibrary.open('camera_mvp.dll');
    } else if (Platform.isLinux) {
      return ffi.DynamicLibrary.open('libStrokeCVLib.so');
    }

    throw UnsupportedError('Unsupported platform');
  }

  callLib() {
    final ffi.DynamicLibrary nativeLib = _loadDynamicLibrary();
    final camera = bindings.CameraBindings(nativeLib);
    final int result = camera.sum(20, 10);
    final int multiply = camera.multiply(3, 4);
    final gpu = camera.initOpenGLExternalTexture();
    print("Sum result: ${result}");
    print("Multiply result: ${multiply}");
    print(gpu);
    print(result);
  }

  Future<void> testGetTextureID() async {
    const platform = MethodChannel('texture-backend');
    try {
      final int? textureid = await platform.invokeMethod<int>('textureid');
      print('Retrieved texture ID: $textureid');
    } catch (e) {
      print('Failed to retrieve texture ID: $e');
    }
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        body: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            crossAxisAlignment: CrossAxisAlignment.center,
            children: [
              ElevatedButton(
                onPressed: testGetTextureID,
                child: const Text("Texture ID is: "),
              ),
              AspectRatio(aspectRatio: 9 / 16, child: const Camera()),
            ],
          ),
        ),
      ),
    );
  }
}
