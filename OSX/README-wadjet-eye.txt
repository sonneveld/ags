# Wadjet Eye for OSX

G'day! This document should contain everything you need to know about building Wadjet Eye games in xcode.

Steps:

1. Download the Steamworks SDK.  Has been tested with v1.37.  Unzip and install it somewhere in your home directory. (location doesn't matter as you will soon see).  You should be able to download from https://partner.steamgames.com/home after signing in.

2. Install Xcode. I've been developing in 7.2.1 but newer versions should work just as well.

3. Build libraries required.  The instructions are in OSX/buildlibs/README.txt.  The gist is that you install command line tools via homebrew and then use the makefile to build libraries.

4. Open Xcode. 

5. Configure the Steamworks SDK location. In Preferences > Locations > Source Trees, Click the "+" and add an entry with the following details:

	* Name: steamworks_sdk
	* Display Name: Steamworks SDK
	* Path: path to where you installed the sdk!

	More details here: https://developer.apple.com/library/ios/recipes/xcode_help-locations_preferences/About/About.html

6. Copy the relevant game files into the project directory.  For example, for Shivah, you would ensure you had these files copied:
	* OSX/xcode/shivah/game_files/ac2game.dat
	* OSX/xcode/shivah/game_files/audio.vox
	* OSX/xcode/shivah/game_files/speech.vox
	* OSX/xcode/shivah/game_files/steam/ac2game.dat

7. Open the Wadjet Eye workspace file in xcode.  It is located in OSX/xcode/wadjeteye.xcworkspace

8. Build the non-Steam version. Select the "Shivah" target in the upper left. (next to the stop/start buttons). From the Product menu, select Archive.

9. Build the Steam version. Select the "Shivah Steam" target in the upper left. (next to the stop/start buttons). From the Product menu, select Archive.

10. Visit the Organizer by going to Window > Organiser.

11. You should see your products available here as archives.  You want to extract the Applications from here.

12. Right click on the item (be careful you don't go into edit name mode) and select "Show in Finder"

13. On the archive, right click again and select "Show Package Contents"

14. In Products / Applications, you should find your application. Copy it out.

15. Do the same for the other product you built! 

