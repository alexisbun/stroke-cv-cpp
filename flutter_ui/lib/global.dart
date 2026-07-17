import 'dart:ffi';
import 'package:ffi/ffi.dart';
import 'package:jni_flutter/jni_flutter.dart';
import 'package:camera_mvp/camera_bindings.dart';
import 'package:permission_handler/permission_handler.dart';
import 'dart:ffi' as ffi;
import 'dart:io';
import 'dart:async';
import 'package:jni/jni.dart';

class PermissionService {
  static Future<bool> checkCameraPermission() async {
    var status = await Permission.camera.status;
    if (status.isDenied) {
      status = await Permission.camera.request();
    }

    if (status.isPermanentlyDenied || status.isRestricted) {
      await openAppSettings();
      status = await Permission.camera.status;
    }

    return status.isGranted;
  }
}

class InitLibrary {
  static ffi.DynamicLibrary? _instance;
  static ffi.DynamicLibrary get instance {
    if (_instance == null) {
      throw StateError('InitLibrary has not been initialized.');
    }
    return _instance!;
  }

  static void init() {
    if (_instance != null) return; // Already loaded
    _instance = _loadDynamicLibrary();
  }

  static ffi.DynamicLibrary _loadDynamicLibrary() {
    if (Platform.isAndroid) {
      // Remember to test whether you need to load libface_landmarker.so even in the
      // first place because it's already linked to libStrokeCVLib.so.
      try {
        ffi.DynamicLibrary.open('libface_landmarker.so');
      } catch (e) {
        print("Failed to load libface_landmarker.so: $e");
      }
      return ffi.DynamicLibrary.open('libStrokeCVLib.so');
    } else if (Platform.isIOS || Platform.isMacOS) {
      return ffi.DynamicLibrary.process();
    } else if (Platform.isWindows) {
      return ffi.DynamicLibrary.open('camera_mvp.dll');
    } else if (Platform.isLinux) {
      return ffi.DynamicLibrary.open('libStrokeCVLib.so');
    }
    throw UnsupportedError('Unsupported platform');
  }
}

CameraBindings bindings = CameraBindings(InitLibrary.instance);
void initializeEngine(ffi.Pointer<ffi.Void> envPointer, JObject jAssetManager) {
  final ffi.Pointer<Utf8> utf8Pointer =
      'flutter_assets/assets/face_landmarker.task'.toNativeUtf8();
  final ffi.Pointer<ffi.Char> charPointer = utf8Pointer.cast<ffi.Char>();
  bindings.initFaceMeshFromAsset(
    envPointer,
    jAssetManager.reference.pointer,
    charPointer,
  );
  calloc.free(utf8Pointer);
}
