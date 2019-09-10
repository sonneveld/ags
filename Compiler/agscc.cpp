#include "script/cs_compiler.h"

int main() 
{
    // not designed to be run, just to ensure linkage.
    
    char headerscript[] = "script";
    char headername[] = "name";
    ccAddDefaultHeader(headerscript, headername);

    ccRemoveDefaultHeaders();

    ccSetSoftwareVersion("version");

    ccCompileText("script", "scriptname");
}