import 'dart:ffi';
import 'package:camera_mvp/camera_bindings.dart';
import 'camera_bindings.dart' as bindings;
import 'package:permission_handler/permission_handler.dart';

import 'dart:ffi' as ffi;
import 'dart:io';

import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

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
