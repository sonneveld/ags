package com.bigbluecup.android;

public class NativeLibraryLoader {

     public static void LoadLibraries() {
        System.loadLibrary("allegro");

        System.loadLibrary("freetype");

        System.loadLibrary("ogg");
        System.loadLibrary("theoradec");

        // We could build against Tremor or Vorbis, so try both.

        try {
            System.loadLibrary("vorbisidec");
        } catch (UnsatisfiedLinkError e) { }

        try {
            System.loadLibrary("vorbis");
            System.loadLibrary("vorbisfile");
        } catch (UnsatisfiedLinkError e) { }

        System.loadLibrary("cda");
        System.loadLibrary("glad-gles2");
        System.loadLibrary("glm_shared");
        System.loadLibrary("hq2x");

        System.loadLibrary("aastr");
        System.loadLibrary("aldumb");
        System.loadLibrary("alfont");
        System.loadLibrary("almp3");
        System.loadLibrary("alogg");
        System.loadLibrary("apeg");

        System.loadLibrary("ags_parallax");
        System.loadLibrary("ags_snowrain");
        System.loadLibrary("agsblend");
        System.loadLibrary("agsflashlight");
        System.loadLibrary("agspalrender");
        System.loadLibrary("agsspritefont");
        System.loadLibrary("agstouch");

        System.loadLibrary("engine");
    }
}
