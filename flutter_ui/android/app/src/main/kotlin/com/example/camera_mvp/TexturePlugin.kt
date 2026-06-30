package com.example.camera_mvp

import android.view.Surface
import androidx.annotation.Keep
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.plugin.common.MethodChannel
import io.flutter.view.TextureRegistry

@Keep
class TexturePlugin : FlutterPlugin {
    private var channel: MethodChannel? = null
    private var producer: TextureRegistry.SurfaceProducer? = null
    private var nativeHandle: Long = 0

    private external fun nativeAttach(surface: Surface, width: Int, height: Int): Long
    private external fun nativeDetach(handle: Long)

    companion object {
        init {
            System.loadLibrary("StrokeCVLib")
        }
    }

    override fun onAttachedToEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        val targetWidth = 1920
        val targetHeight = 1080

        val prod = binding.textureRegistry.createSurfaceProducer()
        prod.setSize(targetWidth,targetHeight)
        producer = prod

        prod.setCallback(object: TextureRegistry.SurfaceProducer.Callback {
            override fun onSurfaceAvailable() {
                if (nativeHandle == 0L) {
                    nativeHandle = nativeAttach(prod.surface, targetWidth, targetHeight)
                }
            }
            override fun onSurfaceCleanup() {
                if (nativeHandle != 0L) {
                    nativeDetach(nativeHandle)
                    nativeHandle = 0L
                }
            }
        })

        channel = MethodChannel(binding.binaryMessenger, "texture-backend").apply {
            setMethodCallHandler { call, result ->
                when (call.method) {
                    "textureid" -> {
                        result.success(producer?.id())
                    }
                    "disposeTexture" -> {
                        if (nativeHandle != 0L) {
                            nativeDetach(nativeHandle)
                            nativeHandle = 0L
                        }
                        producer?.release()
                        producer = null
                        result.success(null)
                    }
                    else -> result.notImplemented()
                }
            }
        }
    }

    override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        if (nativeHandle != 0L) {
            nativeDetach(nativeHandle)
            nativeHandle = 0L
        }
        channel?.setMethodCallHandler(null)
        channel =  null
        producer = null
    }
}