import 'dart:ffi' as ffi;
import 'dart:io';

import 'package:flutter/material.dart';
import 'camera_bindings.dart' as bindings;

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
