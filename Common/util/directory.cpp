
#include "util/directory.h"

#include <errno.h>
#if defined (WINDOWS_VERSION)
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "util/path.h"

namespace AGS
{
namespace Common
{

namespace Directory
{

bool EnsureDirectoryExists(const String &path)
{
    return mkdir(path
#if !defined (WINDOWS_VERSION)
        , 0755
#endif
        ) == 0 || errno == EEXIST;
}

String ChangeWorkingDirectory(const String &path)
{
    chdir(path);
    return GetWorkingDirectory();
}

String GetWorkingDirectory()
{
    char buf[512];
    getcwd(buf, 512);
    String str(buf);
    Path::FixupPath(str);
    return str;
}

} // namespace Directory

} // namespace Common
} // namespace AGS
