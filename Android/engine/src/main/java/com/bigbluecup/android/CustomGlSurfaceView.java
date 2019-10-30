package com.bigbluecup.android;

import android.content.Context;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import javax.microedition.khronos.opengles.GL;
import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGL11;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;

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

	private void DumpEglConfigs()
	{
		EGLConfig[] allconfigs = new EGLConfig[256];
		int[] numconfigs_a = {0};
		egl.eglGetConfigs(display, allconfigs, 256, numconfigs_a);
		int numconfigs = numconfigs_a[0];

		for (int i = 0; i < numconfigs; i++) {
			int[] value_a = new int[1];
			Log.d("AGS", "-");
			Log.d("AGS", "-");

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_CONFIG_ID, value_a);
			Log.d("AGS", "EGL_CONFIG_ID:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_RENDERABLE_TYPE, value_a);
			Log.d("AGS", "EGL_RENDERABLE_TYPE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_CONFORMANT, value_a);
			Log.d("AGS", "EGL_CONFORMANT:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_CONFIG_CAVEAT, value_a);
			Log.d("AGS", "EGL_CONFIG_CAVEAT:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_SURFACE_TYPE, value_a);
			Log.d("AGS", "EGL_SURFACE_TYPE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_COLOR_BUFFER_TYPE, value_a);
			Log.d("AGS", "EGL_COLOR_BUFFER_TYPE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_RED_SIZE, value_a);
			Log.d("AGS", "EGL_RED_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_GREEN_SIZE, value_a);
			Log.d("AGS", "EGL_GREEN_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_BLUE_SIZE, value_a);
			Log.d("AGS", "EGL_BLUE_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_ALPHA_SIZE, value_a);
			Log.d("AGS", "EGL_ALPHA_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_BUFFER_SIZE, value_a);
			Log.d("AGS", "EGL_BUFFER_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_DEPTH_SIZE, value_a);
			Log.d("AGS", "EGL_DEPTH_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_STENCIL_SIZE, value_a);
			Log.d("AGS", "EGL_STENCIL_SIZE:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_MAX_PBUFFER_WIDTH, value_a);
			Log.d("AGS", "EGL_MAX_PBUFFER_WIDTH:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_MAX_PBUFFER_HEIGHT, value_a);
			Log.d("AGS", "EGL_MAX_PBUFFER_HEIGHT:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_MAX_PBUFFER_PIXELS, value_a);
			Log.d("AGS", "EGL_MAX_PBUFFER_PIXELS:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_BIND_TO_TEXTURE_RGB, value_a);
			Log.d("AGS", "EGL_BIND_TO_TEXTURE_RGB:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_BIND_TO_TEXTURE_RGBA, value_a);
			Log.d("AGS", "EGL_BIND_TO_TEXTURE_RGBA:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_MIN_SWAP_INTERVAL, value_a);
			Log.d("AGS", "EGL_MIN_SWAP_INTERVAL:"+value_a[0]);

			egl.eglGetConfigAttrib(display, allconfigs[i], EGL14.EGL_MAX_SWAP_INTERVAL, value_a);
			Log.d("AGS", "EGL_MAX_SWAP_INTERVAL:"+value_a[0]);
		}
	}

	public void initialize(Renderer rendererInterface)
	{
		renderer = rendererInterface;
		
		egl = (EGL10)EGLContext.getEGL();

		display = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
		if (display == EGL10.EGL_NO_DISPLAY) {
			throw new RuntimeException("Could not get EGL display connection. err:0x" + Integer.toHexString(egl.eglGetError()));
		}

		boolean initSuccess = egl.eglInitialize(display, null);
		if (!initSuccess) {
			throw new RuntimeException("Could not initialise an EGL display connection. err:0x" + Integer.toHexString(egl.eglGetError()));
		}

		DumpEglConfigs();

		int[] configSpec =
				{
						EGL10.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
						EGL14.EGL_CONFORMANT, EGL14.EGL_OPENGL_ES2_BIT,
						EGL10.EGL_SURFACE_TYPE, EGL10.EGL_WINDOW_BIT|EGL10.EGL_PBUFFER_BIT,
						EGL10.EGL_COLOR_BUFFER_TYPE, EGL10.EGL_RGB_BUFFER,
						EGL10.EGL_RED_SIZE, 8,
						EGL10.EGL_GREEN_SIZE, 8,
						EGL10.EGL_BLUE_SIZE, 8,
						EGL10.EGL_ALPHA_SIZE, 0,  // We don't want alpha for framebuffer or it will blend with surface
						EGL10.EGL_NONE
				};

		EGLConfig[] configs = new EGLConfig[1];
		int[] num_config = new int[1];
		boolean chooseconfigSuccess = egl.eglChooseConfig(display, configSpec, configs, 1, num_config);
		if (!chooseconfigSuccess) {
			throw new RuntimeException("Could not match EGL frame buffer config. err:0x" + Integer.toHexString(egl.eglGetError()));
		}
		if (num_config[0] <= 0) {
			throw new RuntimeException("No available EGL context configs. err:0x" + Integer.toHexString(egl.eglGetError()));
		}
		config = configs[0];

		int[] value_a = new int[1];
		egl.eglGetConfigAttrib(display, config, EGL14.EGL_CONFIG_ID, value_a);
		Log.d("AGS", "Selected EGL config with ID "+value_a[0]);


		int[] attrib_list = {
				EGL14.EGL_CONTEXT_CLIENT_VERSION, 2,
				EGL10.EGL_NONE
		};

		eglContext = egl.eglCreateContext(display, config, EGL10.EGL_NO_CONTEXT, attrib_list);
		if (eglContext == EGL10.EGL_NO_CONTEXT) {
			throw new RuntimeException("Could not create EGL context. err:0x" + Integer.toHexString(egl.eglGetError()));
		}

		gl = eglContext.getGL();
		if (gl == null) {
			throw new RuntimeException("Could not create GL context. err:0x" + Integer.toHexString(egl.eglGetError()));
		}

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
			surface = null;
		}

		surface = egl.eglCreateWindowSurface(display, config, surfaceHolder, null);
		if (surface == EGL10.EGL_NO_SURFACE) {
			throw new RuntimeException("Could not create EGL surface. err:0x" + Integer.toHexString(egl.eglGetError()));
		}
		egl.eglMakeCurrent(display, surface, surface, eglContext);

		hasSurface = true;
	}

	public void swapBuffers()
	{
		if (hasSurface) {
			boolean swapSuccess = egl.eglSwapBuffers(display, surface);
			if (!swapSuccess) {
				hasSurface = false;
			}
		}

		// Must be called from the rendering thread
		if (!hasSurface) {
			createSurface();
		}
	}
}
