-keep class io.flutter.embedding.android.FlutterActivity {
    public io.flutter.embedding.engine.FlutterEngine getFlutterEngine();
}
-keep class io.flutter.embedding.engine.FlutterEngine {
    public io.flutter.embedding.engine.renderer.FlutterRenderer getRenderer();
}
-keep class io.flutter.embedding.engine.renderer.FlutterRenderer {
    *;
}
-keep class io.flutter.plugins.** { *; }
-keep class io.flutter.embedding.** { *; }
-dontwarn com.google.android.play.core.**

-keep class io.flutter.view.TextureRegistry { *; }
-keep interface io.flutter.view.TextureRegistry { *; }
-keep class io.flutter.view.TextureRegistry$SurfaceTextureEntry { *; }
-keep interface io.flutter.view.TextureRegistry$SurfaceTextureEntry { *; }

-keep class io.flutter.view.** { *; }
-keep interface io.flutter.view.** { *; }