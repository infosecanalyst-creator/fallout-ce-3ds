#include "platform_compat.h"

#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <timeapi.h>
#else
#include <chrono>
#endif

#include <SDL.h>

namespace fallout {

#ifdef __3DS__
// ---------------------------------------------------------------------------
// Loose-file index (3DS). Every game file is looked up in the loose "data"
// folder before MASTER.DAT/CRITTER.DAT. On the real SD card each failed lookup
// costs ~70-100 ms (plus a directory scan for the case-insensitive retry), and
// a map change opens hundreds of files, so area loads took many seconds. The
// folder is scanned once into memory; lookups of files that aren't there are
// answered from memory, and writes/removes/renames keep the index current.
// FAT ignores case, so the case-resolving directory scan is never needed.
// ---------------------------------------------------------------------------
} // namespace fallout
#include <string>
#include <unordered_set>
#include <vector>
namespace fallout {

static std::vector<std::string> gLooseRoots;
unsigned int gCtrSdOpens = 0;     // fopen/gzopen calls that reached the SD card
unsigned int gCtrSdSkipped = 0;   // lookups answered from the index

static std::unordered_set<std::string> gLooseFiles;

static void looseKey(const char* nativePath, std::string& out)
{
    out.clear();
    const char* p = nativePath;
    while (p[0] == '.' && p[1] == '/') p += 2;
    for (; *p; ++p) {
        char c = *p;
        if (c == '\\') c = '/';
        if (c == '/' && !out.empty() && out.back() == '/') continue;
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        out.push_back(c);
    }
}

static bool looseCovered(const std::string& key)
{
    for (const std::string& root : gLooseRoots) {
        if (key.size() > root.size() && key.compare(0, root.size(), root) == 0 && key[root.size()] == '/') {
            return true;
        }
    }
    return false;
}

static void looseScan(const std::string& dirPath, const std::string& keyPrefix, int depth)
{
    if (depth > 8) return;
    DIR* dir = opendir(dirPath.c_str());
    if (dir == nullptr) return;
    std::vector<std::pair<std::string, std::string>> subdirs;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.' && (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) continue;
        std::string key;
        std::string full = keyPrefix + "/" + entry->d_name;
        looseKey(full.c_str(), key);
        bool isDir;
        if (entry->d_type == DT_DIR) {
            isDir = true;
        } else if (entry->d_type == DT_UNKNOWN) {
            struct stat st;
            isDir = stat((dirPath + "/" + entry->d_name).c_str(), &st) == 0 && S_ISDIR(st.st_mode);
        } else {
            isDir = false;
        }
        if (isDir) {
            subdirs.emplace_back(dirPath + "/" + entry->d_name, full);
        } else {
            gLooseFiles.insert(key);
        }
    }
    closedir(dir);
    for (auto& sd : subdirs) looseScan(sd.first, sd.second, depth + 1);
}

void compat_index_loose_dir(const char* path)
{
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
    std::string root;
    looseKey(nativePath, root);
    while (!root.empty() && root.back() == '/') root.pop_back();
    if (root.empty()) return;
    for (const std::string& r : gLooseRoots) if (r == root) return;
    looseScan(nativePath, root, 0);
    gLooseRoots.push_back(root);
}

// true: the path is inside an indexed folder and no such file exists.
static bool looseKnownMissing(const char* nativePath)
{
    if (gLooseRoots.empty()) return false;
    std::string key;
    looseKey(nativePath, key);
    if (looseCovered(key) && gLooseFiles.find(key) == gLooseFiles.end()) {
        gCtrSdSkipped++;
        return true;
    }
    return false;
}

static void looseNoteCreated(const char* nativePath)
{
    if (gLooseRoots.empty()) return;
    std::string key;
    looseKey(nativePath, key);
    if (looseCovered(key)) gLooseFiles.insert(key);
}

static void looseNoteRemoved(const char* nativePath)
{
    if (gLooseRoots.empty()) return;
    std::string key;
    looseKey(nativePath, key);
    gLooseFiles.erase(key);
}
#endif

int compat_stricmp(const char* string1, const char* string2)
{
    return SDL_strcasecmp(string1, string2);
}

int compat_strnicmp(const char* string1, const char* string2, size_t size)
{
    return SDL_strncasecmp(string1, string2, size);
}

char* compat_strupr(char* string)
{
    return SDL_strupr(string);
}

char* compat_strlwr(char* string)
{
    return SDL_strlwr(string);
}

char* compat_itoa(int value, char* buffer, int radix)
{
    return SDL_itoa(value, buffer, radix);
}

void compat_splitpath(const char* path, char* drive, char* dir, char* fname, char* ext)
{
#ifdef _WIN32
    _splitpath(path, drive, dir, fname, ext);
#else
    const char* driveStart = path;
    if (path[0] == '/' && path[1] == '/') {
        path += 2;
        while (*path != '\0' && *path != '/' && *path != '.') {
            path++;
        }
    }

    if (drive != nullptr) {
        size_t driveSize = path - driveStart;
        if (driveSize > COMPAT_MAX_DRIVE - 1) {
            driveSize = COMPAT_MAX_DRIVE - 1;
        }
        strncpy(drive, path, driveSize);
        drive[driveSize] = '\0';
    }

    const char* dirStart = path;
    const char* fnameStart = path;
    const char* extStart = nullptr;

    const char* end = path;
    while (*end != '\0') {
        if (*end == '/') {
            fnameStart = end + 1;
        } else if (*end == '.') {
            extStart = end;
        }
        end++;
    }

    if (extStart == nullptr) {
        extStart = end;
    }

    if (dir != nullptr) {
        size_t dirSize = fnameStart - dirStart;
        if (dirSize > COMPAT_MAX_DIR - 1) {
            dirSize = COMPAT_MAX_DIR - 1;
        }
        strncpy(dir, path, dirSize);
        dir[dirSize] = '\0';
    }

    if (fname != nullptr) {
        size_t fileNameSize = extStart - fnameStart;
        if (fileNameSize > COMPAT_MAX_FNAME - 1) {
            fileNameSize = COMPAT_MAX_FNAME - 1;
        }
        strncpy(fname, fnameStart, fileNameSize);
        fname[fileNameSize] = '\0';
    }

    if (ext != nullptr) {
        size_t extSize = end - extStart;
        if (extSize > COMPAT_MAX_EXT - 1) {
            extSize = COMPAT_MAX_EXT - 1;
        }
        strncpy(ext, extStart, extSize);
        ext[extSize] = '\0';
    }
#endif
}

void compat_makepath(char* path, const char* drive, const char* dir, const char* fname, const char* ext)
{
#ifdef _WIN32
    _makepath(path, drive, dir, fname, ext);
#else
    path[0] = '\0';

    if (drive != nullptr) {
        if (*drive != '\0') {
            strcpy(path, drive);
            path = strchr(path, '\0');

            if (path[-1] == '/') {
                path--;
            } else {
                *path = '/';
            }
        }
    }

    if (dir != nullptr) {
        if (*dir != '\0') {
            if (*dir != '/' && *path == '/') {
                path++;
            }

            strcpy(path, dir);
            path = strchr(path, '\0');

            if (path[-1] == '/') {
                path--;
            } else {
                *path = '/';
            }
        }
    }

    if (fname != nullptr && *fname != '\0') {
        if (*fname != '/' && *path == '/') {
            path++;
        }

        strcpy(path, fname);
        path = strchr(path, '\0');
    } else {
        if (*path == '/') {
            path++;
        }
    }

    if (ext != nullptr) {
        if (*ext != '\0') {
            if (*ext != '.') {
                *path++ = '.';
            }

            strcpy(path, ext);
            path = strchr(path, '\0');
        }
    }

    *path = '\0';
#endif
}

long compat_tell(int fd)
{
    return lseek(fd, 0, SEEK_CUR);
}

long compat_filelength(int fd)
{
    long originalOffset = lseek(fd, 0, SEEK_CUR);
    lseek(fd, 0, SEEK_SET);
    long filesize = lseek(fd, 0, SEEK_END);
    lseek(fd, originalOffset, SEEK_SET);
    return filesize;
}

int compat_mkdir(const char* path)
{
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
    compat_resolve_path(nativePath);

#ifdef _WIN32
    return mkdir(nativePath);
#else
    return mkdir(nativePath, 0755);
#endif
}

unsigned int compat_timeGetTime()
{
#ifdef _WIN32
    return timeGetTime();
#else
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return static_cast<unsigned int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
#endif
}

FILE* compat_fopen(const char* path, const char* mode)
{
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
#ifdef __3DS__
    // The SD card's FAT ignores case, so no case-resolving scan; files known
    // not to be in an indexed loose folder are refused without touching the SD.
    if (mode[0] == 'r' && strchr(mode, '+') == nullptr && looseKnownMissing(nativePath)) {
        return nullptr;
    }
    gCtrSdOpens++;
    FILE* stream = fopen(nativePath, mode);
    if (stream != nullptr && (mode[0] == 'w' || mode[0] == 'a')) {
        looseNoteCreated(nativePath);
    }
    return stream;
#else
    compat_resolve_path(nativePath);
    return fopen(nativePath, mode);
#endif
}

gzFile compat_gzopen(const char* path, const char* mode)
{
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
#ifdef __3DS__
    if (mode[0] == 'r' && looseKnownMissing(nativePath)) {
        return nullptr;
    }
    gCtrSdOpens++;
    gzFile gz = gzopen(nativePath, mode);
    if (gz != nullptr && (mode[0] == 'w' || mode[0] == 'a')) {
        looseNoteCreated(nativePath);
    }
    return gz;
#else
    compat_resolve_path(nativePath);
    return gzopen(nativePath, mode);
#endif
}

char* compat_fgets(char* buffer, int maxCount, FILE* stream)
{
    buffer = fgets(buffer, maxCount, stream);

    if (buffer != nullptr) {
        size_t len = strlen(buffer);
        if (len >= 2 && buffer[len - 1] == '\n' && buffer[len - 2] == '\r') {
            buffer[len - 2] = '\n';
            buffer[len - 1] = '\0';
        }
    }

    return buffer;
}

char* compat_gzgets(gzFile stream, char* buffer, int maxCount)
{
    buffer = gzgets(stream, buffer, maxCount);

    if (buffer != nullptr) {
        size_t len = strlen(buffer);
        if (len >= 2 && buffer[len - 1] == '\n' && buffer[len - 2] == '\r') {
            buffer[len - 2] = '\n';
            buffer[len - 1] = '\0';
        }
    }

    return buffer;
}

int compat_remove(const char* path)
{
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
#ifdef __3DS__
    if (looseKnownMissing(nativePath)) {
        return -1;
    }
    int rc = remove(nativePath);
    if (rc == 0) {
        looseNoteRemoved(nativePath);
    }
    return rc;
#else
    compat_resolve_path(nativePath);
    return remove(nativePath);
#endif
}

int compat_rename(const char* oldFileName, const char* newFileName)
{
    char nativeOldFileName[COMPAT_MAX_PATH];
    strcpy(nativeOldFileName, oldFileName);
    compat_windows_path_to_native(nativeOldFileName);
    compat_resolve_path(nativeOldFileName);

    char nativeNewFileName[COMPAT_MAX_PATH];
    strcpy(nativeNewFileName, newFileName);
    compat_windows_path_to_native(nativeNewFileName);
    compat_resolve_path(nativeNewFileName);

#ifdef __3DS__
    int rc = rename(nativeOldFileName, nativeNewFileName);
    if (rc == 0) {
        looseNoteRemoved(nativeOldFileName);
        looseNoteCreated(nativeNewFileName);
    }
    return rc;
#else
    return rename(nativeOldFileName, nativeNewFileName);
#endif
}

void compat_windows_path_to_native(char* path)
{
#ifndef _WIN32
    char* pch = path;
    while (*pch != '\0') {
        if (*pch == '\\') {
            *pch = '/';
        }
        pch++;
    }
#endif
}

void compat_resolve_path(char* path)
{
#if !defined(_WIN32) && !defined(__3DS__)
    char* pch = path;

    DIR* dir;
    if (pch[0] == '/') {
        dir = opendir("/");
        pch++;
    } else {
        dir = opendir(".");
    }

    while (dir != nullptr) {
        char* sep = strchr(pch, '/');
        size_t length;
        if (sep != nullptr) {
            length = sep - pch;
        } else {
            length = strlen(pch);
        }

        bool found = false;

        struct dirent* entry = readdir(dir);
        while (entry != nullptr) {
            if (strlen(entry->d_name) == length && compat_strnicmp(pch, entry->d_name, length) == 0) {
                strncpy(pch, entry->d_name, length);
                found = true;
                break;
            }
            entry = readdir(dir);
        }

        closedir(dir);
        dir = nullptr;

        if (!found) {
            break;
        }

        if (sep == nullptr) {
            break;
        }

        *sep = '\0';
        dir = opendir(path);
        *sep = '/';

        pch = sep + 1;
    }
#endif
}

int compat_access(const char* path, int mode)
{
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
#ifdef __3DS__
    if (looseKnownMissing(nativePath)) {
        return -1;
    }
#endif
    compat_resolve_path(nativePath);
    return access(nativePath, mode);
}

char* compat_strdup(const char* string)
{
    return SDL_strdup(string);
}

// It's a replacement for compat_filelength(fileno(stream)) on platforms without
// fileno defined.
long getFileSize(FILE* stream)
{
    long originalOffset = ftell(stream);
    fseek(stream, 0, SEEK_END);
    long filesize = ftell(stream);
    fseek(stream, originalOffset, SEEK_SET);
    return filesize;
}

} // namespace fallout
