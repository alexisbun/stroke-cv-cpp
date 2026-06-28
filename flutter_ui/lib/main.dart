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

void main() {
  runApp(const MainApp());
}

class MainApp extends StatelessWidget {
  const MainApp({super.key});

  ffi.DynamicLibrary _loadDynamicLibrary() {
    if (Platform.isAndroid) {
      return ffi.DynamicLibrary.open('libStrokeCVLib.so');
    } else if (Platform.isIOS || Platform.isMacOS) {
      // On iOS/macOS, if statically linked, use process()
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

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        body: Center(
          child: OutlinedButton(onPressed: callLib, child: Text("Hello")),
        ),
      ),
    );
  }
}
