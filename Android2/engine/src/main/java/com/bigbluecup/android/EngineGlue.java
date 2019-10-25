package com.bigbluecup.android;

import javax.microedition.khronos.egl.EGL10;
import com.bigbluecup.android.AgsEngine;

import android.content.pm.ActivityInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.os.Message;
import android.view.Display;

public class EngineGlue extends Thread implements CustomGlSurfaceView.Renderer
{
	public static final int MSG_SWITCH_TO_INGAME = 1;
	public static final int MSG_SHOW_MESSAGE = 2;
	public static final int MSG_SHOW_TOAST = 3;
	public static final int MSG_SET_ORIENTATION = 4;
	public static final int MSG_ENABLE_LONGCLICK = 5;
	
	public static final int MOUSE_CLICK_LEFT = 1;
	public static final int MOUSE_CLICK_RIGHT = 2;
	public static final int MOUSE_HOLD_LEFT = 10;
	
	public int keyboardKeycode = 0;
	public short mouseMoveX = 0;
	public short mouseMoveY = 0;
	public short mouseRelativeMoveX = 0;
	public short mouseRelativeMoveY = 0;
	public int mouseClick = 0;
	
	private int screenPhysicalWidth = 480;
	private int screenPhysicalHeight = 320;
	private int screenVirtualWidth = 320;
	private int screenVirtualHeight = 200;
	private int screenOffsetX = 0;
	private int screenOffsetY = 0;
	
	private boolean paused = false;

	private AudioTrack audioTrack;
	private byte[] audioBuffer;	
	private int bufferSize = 0;
	
	private AgsEngine activity;

	private String gameFilename = "";
	private String baseDirectory = "";
	private String appDirectory = "";
	private boolean loadLastSave = false;
	
	public native void nativeInitializeRenderer(int width, int height);
	public native void shutdownEngine();
	private native boolean startEngine(Object object, String filename, String directory, String appDirectory, boolean loadLastSave);
	private native void pauseEngine();
	private native void resumeEngine();
	public native static String FindGameDataInDirectory(String path);

	static {
		NativeLibraryLoader.LoadLibraries();
    }

	public EngineGlue(AgsEngine activity, String filename, String directory, String appDirectory, boolean loadLastSave)
	{
		this.activity = activity;
		gameFilename = filename;
		baseDirectory = directory;
		this.appDirectory = appDirectory;
		this.loadLastSave = loadLastSave;
	}	
	
	public void run()
	{
		startEngine(this, gameFilename, baseDirectory, appDirectory, loadLastSave);
	}	

	public void pauseGame()
	{
		paused = true;
		if (audioTrack != null)
			audioTrack.pause();
		pauseEngine();
	}

	public void resumeGame()
	{
		if (audioTrack != null)
			audioTrack.play();
		resumeEngine();
		paused = false;
	}
	
	public void moveMouse(float relativeX, float relativeY, float x, float y)
	{
		mouseMoveX = (short)(x - screenOffsetX);
		mouseMoveY = (short)(y - screenOffsetY);

		if (mouseMoveX < 0)
			mouseMoveX = 0;
		if (mouseMoveY < 0)
			mouseMoveY = 0;

		mouseRelativeMoveX = (short)relativeX;
		mouseRelativeMoveY = (short)relativeY;
	}
	
	public void clickMouse(int button)
	{
		mouseClick = button;
	}
	
	public void keyboardEvent(int keycode, int character, boolean shiftPressed)
	{
		keyboardKeycode = keycode | (character << 16) | ((shiftPressed ? 1 : 0) << 30);
	}
	
	private void showMessage(String message)
	{
		Bundle data = new Bundle();
		data.putString("message", message);
		sendMessageToActivity(MSG_SHOW_MESSAGE, data);
	}
	
	private void showToast(String message)
	{
		Bundle data = new Bundle();
		data.putString("message", message);
		sendMessageToActivity(MSG_SHOW_TOAST, data);
	}
	
	private void sendMessageToActivity(int messageId, Bundle data)
	{
		Message m = activity.handler.obtainMessage();
		m.what = messageId;
		if (data != null)
			m.setData(data);
		activity.handler.sendMessage(m);
	}
	
	private void createScreen(int width, int height, int color_depth)
	{
		screenVirtualWidth = width;
		screenVirtualHeight = height;
		sendMessageToActivity(MSG_SWITCH_TO_INGAME, null);
		
		int[] configSpec = 
		{
			EGL10.EGL_RED_SIZE, 8,
			EGL10.EGL_GREEN_SIZE, 8,
			EGL10.EGL_BLUE_SIZE, 8,
			EGL10.EGL_ALPHA_SIZE, 0,  // We don't want alpha for framebuffer or it will blend with surface
			EGL10.EGL_NONE
		};
		
		while (!activity.isInGame || (activity.surfaceView == null) || !activity.surfaceView.created)
		{
			try
			{
				Thread.sleep(100, 0);
			}
			catch (InterruptedException e) {}
		}
		
		activity.surfaceView.initialize(configSpec, this);

		// Make sure the mouse starts in the center of the screen
		mouseMoveX = (short)(activity.surfaceView.getWidth() / 2);
		mouseMoveY = (short)(activity.surfaceView.getHeight() / 2);
	}
	
	private void swapBuffers()
	{
		activity.surfaceView.swapBuffers();
	}
	
	public void onSurfaceChanged(int width, int height)
	{
		Display display = activity.getWindowManager().getDefaultDisplay(); 
		screenOffsetX = display.getWidth() - width;
		screenOffsetY = display.getHeight() - height;
		
		setPhysicalScreenResolution(width, height);
		nativeInitializeRenderer(width, height);
	}
		
	// Called from Allegro
	private int pollKeyboard()
	{
		int result = keyboardKeycode;
		keyboardKeycode = 0;
		return result;
	}

	private int pollMouseAbsolute()
	{
		return mouseMoveX | ((int)(mouseMoveY & 0xFFFF) << 16);
	}

	private int pollMouseRelative()
	{
		int result = (mouseRelativeMoveX & 0xFFFF) | ((int)(mouseRelativeMoveY & 0xFFFF) << 16);
		mouseRelativeMoveX = 0;
		mouseRelativeMoveY = 0;
		return result;
	}
	
	private int pollMouseButtons()
	{
		int result = mouseClick;
		mouseClick = 0;
		return result;
	}
	
	private void blockExecution()
	{
		while (paused)
		{
			try
			{
				Thread.sleep(100, 0);
			}
			catch (InterruptedException e) {}
		}
	}
	
	private void setRotation(int orientation)
	{
		Bundle data = new Bundle();
		
		if (orientation == 1)
			data.putInt("orientation", ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
		else if (orientation == 2)
			data.putInt("orientation", ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
		
		sendMessageToActivity(MSG_SET_ORIENTATION, data);
	}
	
	private void enableLongclick()
	{
		sendMessageToActivity(MSG_ENABLE_LONGCLICK, null);
	}

	// Called from Allegro, the buffer is allocated in native code
	public void initializeSound(byte[] buffer, int bufferSize)
	{
		audioBuffer = buffer;
		this.bufferSize = bufferSize;
		
		int sampleRate = 44100;
		int minBufferSize = AudioTrack.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_CONFIGURATION_STEREO, AudioFormat.ENCODING_PCM_16BIT);
		
		if (minBufferSize < bufferSize * 4)
			minBufferSize = bufferSize * 4;
		
		audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, sampleRate, AudioFormat.CHANNEL_CONFIGURATION_STEREO, AudioFormat.ENCODING_PCM_16BIT, minBufferSize, AudioTrack.MODE_STREAM);
		
		float audioVolume = AudioTrack.getMaxVolume();
		audioTrack.setStereoVolume(audioVolume, audioVolume);
		
		audioTrack.play();
	}
	
	public void updateSound()
	{
		audioTrack.write(audioBuffer, 0, bufferSize);
	}

	public void setPhysicalScreenResolution(int width, int height)
	{
		screenPhysicalWidth = width;
		screenPhysicalHeight = height;
	}
}
