/*
This  file  contains  declarations  for  external  functions  
to be  implemented  in  my  C++  library that will allow for 
the invokation  of  TextureRegistry.SurfaceProducer.  This  
will allow  me  to  obtain  the  textureID  for the backend 
texture which  a  Texture  widget  refers  to.  nativeAttach 
creates  a  Surface  which  is  the  consumer  of  an  
image buffer.  The  Surface  object  (by  invoking .getSurface()),
once  registered  and  "bound"  to  the  Texture  widget can
be  passed  as  an  argument  to  ANativeWindow_fromSurface()
inside my C++ library.

ANativeWindow_fromSurface()  generates  a  raw pointer to an
object   called  ANativeWindow.  ANativeWindow  is  required
to  connect  the  raw  pixel  date  produced  by  the camera
to  the  OpenGL  External  Texture.  At  a fundemental level,
ANativeWindow  will  serve  as  the  connection  between the
Texture widget and the OpenGL External Texture.
*/


package com.example.camera_mvp

import android.util.Log
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

    // A companion object can be viewed conceptually as declaring static members for the TexturePlugin class.
    companion object {
        init {
            System.loadLibrary("StrokeCVLib")
        }
    }

    // The binding parameter is a helper object provided by the FlutterEngine which unlocks necessary hooks to interact with textureRegistry.
    override fun onAttachedToEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        val targetWidth = 1280
        val targetHeight = 720

        val prod = binding.textureRegistry.createSurfaceProducer()
        prod.setSize(targetWidth,targetHeight)
        producer = prod

        // Pass in a state-dependent anonymous callback object (TextureRegistry.SurfaceProducer.Callback).
        // The flutter engine will then invoke either onSurfaceAvailable() or onSurfaceCleanup().
        prod.setCallback(object: TextureRegistry.SurfaceProducer.Callback {
            override fun onSurfaceAvailable() {
                Log.d("CameraMVP", "onSurfaceAvailable triggered! Surface status: ${prod.surface.isValid}")
                if (nativeHandle == 0L) {
                    Log.d("CameraMVP", "Calling nativeAttach...")
                    nativeHandle = nativeAttach(prod.surface, targetWidth, targetHeight)
                    Log.d("CameraMVP", "nativeAttach completed. Handle returned: $nativeHandle")
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