
#include "core/platform.h"
#if AGS_PLATFORM_OS_WINDOWS
#include <windows.h>
#endif
#include "allegro/file.h"
#include "util/path.h"
#include "util/stdio_compat.h"

// TODO: implement proper portable path length
#ifndef MAX_PATH
#define MAX_PATH 512
#endif

namespace AGS
{
namespace Common
{

namespace Path
{

bool IsDirectory(const String &filename)
{
    // stat() does not like trailing slashes, remove them
    String fixed_path = MakePathNoSlash(filename);
    return ags_directory_exists(fixed_path.GetCStr()) != 0;
}

bool IsFile(const String &filename)
{
    return ags_file_exists(filename.GetCStr()) != 0;
}

bool IsFileOrDir(const String &filename)
{
    // stat() does not like trailing slashes, remove them
    String fixed_path = MakePathNoSlash(filename);
    return ags_path_exists(fixed_path.GetCStr()) != 0;
}

int ComparePaths(const String &path1, const String &path2)
{
    // Make minimal absolute paths
    String fixed_path1 = MakeAbsolutePath(path1);
    String fixed_path2 = MakeAbsolutePath(path2);

    fixed_path1.TrimRight('/');
    fixed_path2.TrimRight('/');

    int cmp_result =
#if defined AGS_CASE_SENSITIVE_FILESYSTEM
        fixed_path1.Compare(fixed_path2);
#else
        fixed_path1.CompareNoCase(fixed_path2);
#endif // AGS_CASE_SENSITIVE_FILESYSTEM
    return cmp_result;
}

String GetDirectoryPath(const String &path)
{
    if (IsDirectory(path))
        return path;

    String dir = path;
    FixupPath(dir);
    size_t slash_at = dir.FindCharReverse('/');
    if (slash_at != -1)
    {
        dir.ClipMid(slash_at);
        return dir;
    }
    return ".";
}

bool IsSameOrSubDir(const String &parent, const String &path)
{
    char can_parent[MAX_PATH];
    char can_path[MAX_PATH];
    char relative[MAX_PATH];
    // canonicalize_filename treats "." as "./." (file in working dir)
    const char *use_parent = parent == "." ? "./" : parent.GetCStr();
    const char *use_path   = path   == "." ? "./" : path.GetCStr();
    canonicalize_filename(can_parent, use_parent, MAX_PATH);
    canonicalize_filename(can_path, use_path, MAX_PATH);
    const char *pstr = make_relative_filename(relative, can_parent, can_path, MAX_PATH);
    if (!pstr)
        return false;
    for (pstr = strstr(pstr, ".."); pstr && *pstr; pstr = strstr(pstr, ".."))
    {
        pstr += 2;
        if (*pstr == '/' || *pstr == '\\' || *pstr == 0)
            return false;
    }
    return true;
}

void FixupPath(String &path)
{
    if (path.IsEmpty())
    {
        return;
    }
    path.Replace('\\', '/');
}

String MakePathNoSlash(const String &path)
{
    String dir_path = path;
    FixupPath(dir_path);
#if AGS_PLATFORM_OS_WINDOWS
    // if the path is 'x:/' don't strip the slash
    if (path.GetLength() == 3 && path[1u] == ':')
        ;
    else
#endif
    // if the path is '/' don't strip the slash
    if (dir_path.GetLength() > 1)
        dir_path.TrimRight('/');
    return dir_path;
}

String MakeAbsolutePath(const String &path)
{
    if (path.IsEmpty())
    {
        return "";
    }
    // canonicalize_filename treats "." as "./." (file in working dir)
    String abs_path = path == "." ? "./" : path;
#if AGS_PLATFORM_OS_WINDOWS
    // NOTE: cannot use long path names in the engine, because it does not have unicode strings support
    //
    //char long_path_buffer[MAX_PATH];
    //if (GetLongPathNameA(path, long_path_buffer, MAX_PATH) > 0)
    //{
    //    abs_path = long_path_buffer;
    //}
#endif
    char buf[512];
    canonicalize_filename(buf, abs_path.GetCStr(), 512);
    abs_path = buf;
    FixupPath(abs_path);
    return abs_path;
}

String ConcatPaths(const String &parent, const String &child)
{
    String path = parent;
    FixupPath(path);
    if (path.GetLast() != '/')
        path.AppendChar('/');
    path.Append(child);
    return path;
}

String FixupSharedFilename(const String &filename)
{
    const char *illegal_chars = "\\/:?\"<>|*";
    String fixed_name = filename;
    for (size_t i = 0; i < filename.GetLength(); ++i)
    {
        if (filename[i] < ' ')
        {
            fixed_name.SetAt(i, '_');
        }
        else
        {
            for (const char *ch_ptr = illegal_chars; *ch_ptr; ++ch_ptr)
                if (filename[i] == *ch_ptr)
                    fixed_name.SetAt(i, '_');
        }
    }
    return fixed_name;
}

} // namespace Path

} // namespace Common
} // namespace AGS
