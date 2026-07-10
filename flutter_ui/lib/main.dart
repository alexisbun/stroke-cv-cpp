import 'dart:ffi' as ffi;
import 'dart:io';

import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:permission_handler/permission_handler.dart';
import 'camera_bindings.dart' as bindings;

import 'global.dart';

import 'package:jnigen/jnigen.dart';
import 'package:jni/jni.dart';
import 'generated/android_graphics.g.dart';

import 'dart:ui' as ui;
import 'package:jni_flutter/jni_flutter.dart';

typedef NativeAttachCXX =
    ffi.Int64 Function(
      ffi.Pointer<ffi.Void> env,
      ffi.Pointer<ffi.Void> surface,
      ffi.Int32 width,
      ffi.Int32 height,
    );
typedef NativeAttachDart =
    int Function(
      ffi.Pointer<ffi.Void> env,
      ffi.Pointer<ffi.Void> surface,
      int width,
      int height,
    );
typedef NativeDetachCXX = ffi.Void Function(ffi.Int64 engineHandle);
typedef NativeDetachDart = void Function(int engineHandle);
final NativeAttachDart nativeAttach = InitLibrary.instance
    .lookupFunction<NativeAttachCXX, NativeAttachDart>('nativeAttach');
final NativeDetachDart nativeDetach = InitLibrary.instance
    .lookupFunction<NativeDetachCXX, NativeDetachDart>('nativeDetach');

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  try {
    InitLibrary.init();
    print("Native C++ Library loaded successfully.");
  } catch (e) {
    print("Failed to load native library: $e");
  }
  runApp(const MainApp());
}

// CAMERA CLASS (will be in seperate file when migrating to working_ui)
class Camera extends StatefulWidget {
  const Camera({super.key});

  @override
  State<Camera> createState() => _CameraState();
}

class _CameraState extends State<Camera> {
  int? _textureId;
  TextureRegistry$SurfaceTextureEntry? _textureEntry;
  int? _engineHandle;
  var _hasPermission = false;
  var _isLoading = true;
  bool _isPipelineInitialized = false;

  final _bindings = bindings.CameraBindings(InitLibrary.instance);

  Timer? _fpsTimer;
  double _fps = 0.0;

  @override
  void initState() {
    super.initState();
    _checkPermissionAndStartCamera();
  }

  Future<void> _checkPermissionAndStartCamera() async {
    final granted = await PermissionService.checkCameraPermission();
    if (!mounted) return;
    setState(() {
      _hasPermission = granted;
      _isLoading = false;
    });
    if (granted) {
      _initializeRenderPipeline();
    }
  }

  void _initializeRenderPipeline() {
    if (_isPipelineInitialized) return;
    _isPipelineInitialized = true;
    int engineId = 0;
    try {
      engineId = (ui.PlatformDispatcher.instance as dynamic).engineId ?? 0;
    } catch (_) {}
    final activity = androidActivity(engineId);
    if (activity == null) {
      print("Error: Could not retrieve current Android Activity.");
      return;
    }
    final flutterActivityClass = JClass.forName(
      'io/flutter/embedding/android/FlutterActivity',
    );
    final getEngineMethod = flutterActivityClass.instanceMethodId(
      'getFlutterEngine',
      '()Lio/flutter/embedding/engine/FlutterEngine;',
    );
    final engine = getEngineMethod.call(activity, JObject.type, []);
    final engineClass = JClass.forName(
      'io/flutter/embedding/engine/FlutterEngine',
    );
    final getRendererMethod = engineClass.instanceMethodId(
      'getRenderer',
      '()Lio/flutter/embedding/engine/renderer/FlutterRenderer;',
    );
    final renderer = getRendererMethod.call(engine, JObject.type, []);
    final registry = renderer as TextureRegistry;
    final entry = registry.createSurfaceTexture();
    final surfaceTexture = entry.surfaceTexture();

    setState(() {
      _textureEntry = entry;
      _textureId = entry.id();
    });
    final surface = Surface(surfaceTexture);
    final ffi.Pointer<ffi.Void> rawSurfacePointer = surface.reference.pointer;
    final dartJniLib = ffi.DynamicLibrary.open('libdartjni.so');
    final getJniEnv = dartJniLib
        .lookup<ffi.NativeFunction<ffi.Pointer<ffi.Void> Function()>>(
          'GetJniEnv',
        )
        .asFunction<ffi.Pointer<ffi.Void> Function()>();

    final ffi.Pointer<ffi.Void> envPointer = getJniEnv();
    _engineHandle = nativeAttach(envPointer, rawSurfacePointer, 1920, 1080);
    print("CameraEngine initialized in C++ with handle: $_engineHandle");

    if (_engineHandle != null && _engineHandle != 0) {
      _fpsTimer = Timer.periodic(const Duration(milliseconds: 500), (timer) {
        if (_engineHandle != null && _engineHandle != 0) {
          final currentFps = _bindings.getEngineFps(_engineHandle!);
          setState(() {
            _fps = currentFps;
          });
        }
      });
    }
    surface.release();
    surfaceTexture.release();
    activity.release();
  }

  @override
  void dispose() {
    if (_engineHandle != null && _engineHandle != 0) {
      nativeDetach(_engineHandle!);
      _engineHandle = null;
    }
    if (_textureEntry != null) {
      _textureEntry!.release();
      _textureEntry = null;
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    if (_isLoading) {
      return const Scaffold(body: Center(child: CircularProgressIndicator()));
    }
    if (!_hasPermission) {
      return const Scaffold(
        body: Center(child: Text("Camera permission is required.")),
      );
    }
    if (_textureId == null) {
      return const Scaffold(
        body: Center(child: Text("Initializing texture...")),
      );
    }
    return Scaffold(
      body: Stack(
        children: [
          Positioned.fill(child: Texture(textureId: _textureId!)),
          Positioned(
            top: 50,
            left: 20,
            child: Text(
              "FPS: ${_fps.toStringAsFixed(1)}",
              style: const TextStyle(
                color: Colors.white,
                fontSize: 14,
                fontWeight: FontWeight.bold,
              ),
            ),
          ),
        ],
      ),
    );
  }
}

// MAIN
class MainApp extends StatelessWidget {
  const MainApp({super.key});

  Future<void> testGetTextureID() async {}

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        body: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            crossAxisAlignment: CrossAxisAlignment.center,
            children: [AspectRatio(aspectRatio: 9 / 16, child: const Camera())],
          ),
        ),
      ),
    );
  }
}
