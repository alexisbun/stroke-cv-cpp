import 'dart:ffi';
import 'package:ffi/ffi.dart';
import 'package:jni_flutter/jni_flutter.dart';
import 'package:camera_mvp/camera_bindings.dart';
import 'camera_bindings.dart' as bindings;
import 'package:permission_handler/permission_handler.dart';

import 'dart:ffi' as ffi;
import 'dart:io';

import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
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

  // Remember to test whether you need to load libface_landmarker.so even in the
  // first place because it's already linked to libStrokeCVLib.so.
  static void init() {
    if (_instance != null) return; // Already loaded
    _instance = _loadDynamicLibrary();
  }

  static ffi.DynamicLibrary _loadDynamicLibrary() {
    if (Platform.isAndroid) {
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

typedef InitFaceMeshFromAssetCXX =
    ffi.Void Function(
      ffi.Pointer<ffi.Void> env,
      ffi.Pointer<ffi.Void> assetManager,
      ffi.Pointer<Utf8> assetName,
    );
typedef InitFaceMeshFromAssetDart =
    void Function(
      ffi.Pointer<ffi.Void> env,
      ffi.Pointer<ffi.Void> assetManager,
      ffi.Pointer<Utf8> assetName,
    );
final InitFaceMeshFromAssetDart initFaceMeshFromAsset = InitLibrary.instance
    .lookupFunction<InitFaceMeshFromAssetCXX, InitFaceMeshFromAssetDart>(
      'initFaceMeshFromAsset',
    );

void initializeEngine(
  ffi.Pointer<ffi.Void> envPointer,
  JObject jAssetManager,
) async {
  final ffi.Pointer<Utf8> nativeAssetName = 'flutter_assets/assets/face_landmarker.task'
      .toNativeUtf8();
  initFaceMeshFromAsset(
    envPointer,
    jAssetManager.reference.pointer,
    nativeAssetName,
  );
  calloc.free(nativeAssetName);
}
