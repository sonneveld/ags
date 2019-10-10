package com.bigbluecup.android;

import android.content.Context;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import javax.microedition.khronos.opengles.GL;
import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;

import static android.opengl.EGL14.EGL_ALPHA_MASK_SIZE;
import static android.opengl.EGL14.EGL_ALPHA_SIZE;
import static android.opengl.EGL14.EGL_BIND_TO_TEXTURE_RGB;
import static android.opengl.EGL14.EGL_BIND_TO_TEXTURE_RGBA;
import static android.opengl.EGL14.EGL_BLUE_SIZE;
import static android.opengl.EGL14.EGL_BUFFER_SIZE;
import static android.opengl.EGL14.EGL_COLOR_BUFFER_TYPE;
import static android.opengl.EGL14.EGL_CONFIG_CAVEAT;
import static android.opengl.EGL14.EGL_CONFIG_ID;
import static android.opengl.EGL14.EGL_CONFORMANT;
import static android.opengl.EGL14.EGL_DEPTH_SIZE;
import static android.opengl.EGL14.EGL_GREEN_SIZE;
import static android.opengl.EGL14.EGL_LEVEL;
import static android.opengl.EGL14.EGL_LUMINANCE_SIZE;
import static android.opengl.EGL14.EGL_MAX_PBUFFER_HEIGHT;
import static android.opengl.EGL14.EGL_MAX_PBUFFER_PIXELS;
import static android.opengl.EGL14.EGL_MAX_PBUFFER_WIDTH;
import static android.opengl.EGL14.EGL_MAX_SWAP_INTERVAL;
import static android.opengl.EGL14.EGL_MIN_SWAP_INTERVAL;
import static android.opengl.EGL14.EGL_NATIVE_RENDERABLE;
import static android.opengl.EGL14.EGL_NATIVE_VISUAL_ID;
import static android.opengl.EGL14.EGL_NATIVE_VISUAL_TYPE;
import static android.opengl.EGL14.EGL_RED_SIZE;
import static android.opengl.EGL14.EGL_RENDERABLE_TYPE;
import static android.opengl.EGL14.EGL_SAMPLE_BUFFERS;
import static android.opengl.EGL14.EGL_SAMPLES;
import static android.opengl.EGL14.EGL_STENCIL_SIZE;
import static android.opengl.EGL14.EGL_SURFACE_TYPE;
import static android.opengl.EGL14.EGL_TRANSPARENT_BLUE_VALUE;
import static android.opengl.EGL14.EGL_TRANSPARENT_GREEN_VALUE;
import static android.opengl.EGL14.EGL_TRANSPARENT_RED_VALUE;
import static android.opengl.EGL14.EGL_TRANSPARENT_TYPE;

import static android.opengl.EGL14.EGL_CONTEXT_CLIENT_VERSION;
import static android.opengl.EGL14.EGL_TRUE;
import static javax.microedition.khronos.egl.EGL10.EGL_NONE;

class CustomGlSurfaceView extends SurfaceView implements SurfaceHolder.Callback
{
	public interface Renderer
	{
		void onSurfaceChanged(int width, int height);
	}

	@SuppressWarnings("unused")
	private GL gl;
	private EGL10 egl;
	private EGLDisplay display;
	private EGLSurface surface;
	private EGLConfig config;
	private EGLContext eglContext;

	private boolean hasSurface;

	private Renderer renderer;

	private SurfaceHolder surfaceHolder;

	public boolean created;

	CustomGlSurfaceView(Context context)
	{
		super(context);
		surfaceHolder = getHolder();
		surfaceHolder.setType(SurfaceHolder.SURFACE_TYPE_GPU);
		surfaceHolder.addCallback(this);
	}

	public void surfaceCreated(SurfaceHolder holder)
	{
		created = true;
	}

	public void surfaceDestroyed(SurfaceHolder holder)
	{
		created = false;
	}

	public void surfaceChanged(SurfaceHolder holder, int format, int w, int h)
	{
		hasSurface = false;

		// Wait here till the engine thread calls swapBuffers() and the surface is recreated
		while (!hasSurface)
		{
			try
			{
				Thread.sleep(100, 0);
			}
			catch (InterruptedException e) {}
		}
		
		renderer.onSurfaceChanged(w, h);
	}

	public void initialize(int[] configSpec, Renderer rendererInterface)
	{
		renderer = rendererInterface;
		
		egl = (EGL10)EGLContext.getEGL();

		display = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);

		egl.eglInitialize(display, null);


		EGLConfig[] allconfigs = new EGLConfig[128];
		int[] numconfigs_a = new int[1];
		egl.eglGetConfigs(display, allconfigs, 128, numconfigs_a);
		int numconfigs = numconfigs_a[0];

		for (int i = 0; i < numconfigs; i++) {
			int[] value_a = new int[1];
			Log.d("AGS", "");
			Log.d("AGS", "");
			Log.d("AGS", "Config index:"+i);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_ALPHA_MASK_SIZE, value_a);
			Log.d("AGS", "EGL_ALPHA_MASK_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_ALPHA_SIZE, value_a);
			Log.d("AGS", "EGL_ALPHA_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_BIND_TO_TEXTURE_RGB, value_a);
			Log.d("AGS", "EGL_BIND_TO_TEXTURE_RGB:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_BIND_TO_TEXTURE_RGBA, value_a);
			Log.d("AGS", "EGL_BIND_TO_TEXTURE_RGBA:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_BLUE_SIZE, value_a);
			Log.d("AGS", "EGL_BLUE_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_BUFFER_SIZE, value_a);
			Log.d("AGS", "EGL_BUFFER_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_COLOR_BUFFER_TYPE, value_a);
			Log.d("AGS", "EGL_COLOR_BUFFER_TYPE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_CONFIG_CAVEAT, value_a);
			Log.d("AGS", "EGL_CONFIG_CAVEAT:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_CONFIG_ID, value_a);
			Log.d("AGS", "EGL_CONFIG_ID:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_CONFORMANT, value_a);
			Log.d("AGS", "EGL_CONFORMANT:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_DEPTH_SIZE, value_a);
			Log.d("AGS", "EGL_DEPTH_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_GREEN_SIZE, value_a);
			Log.d("AGS", "EGL_GREEN_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_LEVEL, value_a);
			Log.d("AGS", "EGL_LEVEL:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_LUMINANCE_SIZE, value_a);
			Log.d("AGS", "EGL_LUMINANCE_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_MAX_PBUFFER_HEIGHT, value_a);
			Log.d("AGS", "EGL_MAX_PBUFFER_HEIGHT:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_MAX_PBUFFER_PIXELS, value_a);
			Log.d("AGS", "EGL_MAX_PBUFFER_PIXELS:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_MAX_PBUFFER_WIDTH, value_a);
			Log.d("AGS", "EGL_MAX_PBUFFER_WIDTH:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_MAX_SWAP_INTERVAL, value_a);
			Log.d("AGS", "EGL_MAX_SWAP_INTERVAL:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_MIN_SWAP_INTERVAL, value_a);
			Log.d("AGS", "EGL_MIN_SWAP_INTERVAL:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_NATIVE_RENDERABLE, value_a);
			Log.d("AGS", "EGL_NATIVE_RENDERABLE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_NATIVE_VISUAL_ID, value_a);
			Log.d("AGS", "EGL_NATIVE_VISUAL_ID:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_NATIVE_VISUAL_TYPE, value_a);
			Log.d("AGS", "EGL_NATIVE_VISUAL_TYPE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_RED_SIZE, value_a);
			Log.d("AGS", "EGL_RED_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_RENDERABLE_TYPE, value_a);
			Log.d("AGS", "EGL_RENDERABLE_TYPE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_SAMPLE_BUFFERS, value_a);
			Log.d("AGS", "EGL_SAMPLE_BUFFERS:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_SAMPLES, value_a);
			Log.d("AGS", "EGL_SAMPLES:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_STENCIL_SIZE, value_a);
			Log.d("AGS", "EGL_STENCIL_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_SURFACE_TYPE, value_a);
			Log.d("AGS", "EGL_SURFACE_TYPE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_TRANSPARENT_BLUE_VALUE, value_a);
			Log.d("AGS", "EGL_TRANSPARENT_BLUE_VALUE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_TRANSPARENT_GREEN_VALUE, value_a);
			Log.d("AGS", "EGL_TRANSPARENT_GREEN_VALUE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_TRANSPARENT_RED_VALUE, value_a);
			Log.d("AGS", "EGL_TRANSPARENT_RED_VALUE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL_TRANSPARENT_TYPE, value_a);
			Log.d("AGS", "EGL_TRANSPARENT_TYPE:"+value_a[0]);
		}

		EGLConfig[] configs = new EGLConfig[1];
		int[] num_config = new int[1];
		egl.eglChooseConfig(display, configSpec, configs, 1, num_config);
		config = configs[0];


		int[] attrib_list = {
				EGL_CONTEXT_CLIENT_VERSION, 2,
				EGL_NONE
		};

		eglContext = egl.eglCreateContext(display, config, EGL10.EGL_NO_CONTEXT, attrib_list);

		hasSurface = false;
		surface = null;
		createSurface();
	}

	private void createSurface()
	{
		hasSurface = false;
		
		if (surface != null)
		{
			egl.eglMakeCurrent(display, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_CONTEXT);
			egl.eglDestroySurface(display, surface);
		}

		surface = egl.eglCreateWindowSurface(display, config, surfaceHolder, null);
		egl.eglMakeCurrent(display, surface, surface, eglContext);

		gl = eglContext.getGL();

		hasSurface = true;
	}

	public void swapBuffers()
	{
		egl.eglSwapBuffers(display, surface);
		
		// Must be called from the rendering thread
		if (!hasSurface)
			createSurface();
	}
}
