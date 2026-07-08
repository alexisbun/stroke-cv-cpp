import 'dart:ffi';

import 'package:camera_mvp/camera_bindings.dart';

CameraBindings bindings = CameraBindings(
  DynamicLibrary.open('libStrokeCVLib.so'),
);
