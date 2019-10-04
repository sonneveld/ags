package com.bigbluecup.android;

public class PEHelper
{
	static {
		System.loadLibrary("pe");
	}

	public native boolean isAgsDatafile(Object object, String filename);
	
	public PEHelper()
	{
	}
}
