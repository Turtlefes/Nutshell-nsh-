#ifndef PLATFORM_H
#define PLATFORM_H

#include <filesystem>
#include <cerrno>

#ifdef __ANDROID__
namespace fs = std::__fs::filesystem;
#else
namespace fs = std::filesystem;
#endif

#ifndef _WIN32
#include <strings.h>
#else
#define strcasecmp _stricmp
// Implementasi unsetenv untuk Windows
int unsetenv(const char *name);
#endif

#endif // PLATFORM_H
