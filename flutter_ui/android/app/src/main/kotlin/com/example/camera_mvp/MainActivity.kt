package com.example.camera_mvp
import android.util.Log

import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine

class MainActivity : FlutterActivity() {
    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        // Log.d("TAG", "configureFlutterEngine WORKS")
        flutterEngine.plugins.add(TexturePlugin())
        super.configureFlutterEngine(flutterEngine)
    }
}