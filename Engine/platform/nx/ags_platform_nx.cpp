//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-20xx others
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// http://www.opensource.org/licenses/artistic-license-2.0.php
//
//=============================================================================


#include "platform/base/agsplatformdriver.h"

#include "SDL.h"
#include <nn/settings.h>
#include <nn/err.h>


struct AGSNX : AGSPlatformDriver {
	AGSNX() {}
	virtual void DisplayAlert(const char *text, ...) override {
		char displbuf[2000];
		va_list ap;
		va_start(ap, text);
		vsprintf(displbuf, text, ap);
		va_end(ap);

		nn::err::ApplicationErrorArg appError(
			1000,
			(const char*)"AGS Alert",
			(const char*)displbuf,
			nn::settings::LanguageCode::EnglishUs()
		);
		nn::err::ShowApplicationError(appError);
	}
	virtual unsigned long GetDiskFreeSpaceMB() override { return 100; }
	virtual const char* GetNoMouseErrorString() override { return "No mouse detected."; }
	virtual eScriptSystemOSID GetSystemOSID() override { return eOS_NX; }
	virtual void PostAllegroExit() override {}

	const char *GetUserSavedgamesDirectory() override { return SDL_GetPrefPath("uk.co.adventuregamestudio", "AGS"); }
	const char *GetAllUsersDataDirectory() override { return SDL_GetPrefPath("uk.co.adventuregamestudio", "AGS"); }
	const char *GetUserConfigDirectory() override { return SDL_GetPrefPath("uk.co.adventuregamestudio", "AGS"); }
	const char *GetAppOutputDirectory() override { return SDL_GetPrefPath("uk.co.adventuregamestudio", "AGS"); }

	const char *GetIllegalFileChars() override {
		return "\\/:?\"<>|*"; // keep same as Windows so we can sync.
	}

	int  InitializeCDPlayer() override { return 0; }
	int  CDPlayerCommand(int cmdd, int datt) override { return 0;  }
	void ShutdownCDPlayer() override {}

};

AGSPlatformDriver* AGSPlatformDriver::GetDriver() {
	if (instance == NULL)
		instance = new AGSNX();
	return instance;
}
