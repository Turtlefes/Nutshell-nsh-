#include "platform.h"

#ifdef _WIN32
#include <string.h>
#include <stdlib.h>

int unsetenv(const char *name)
{
    if (name == NULL || *name == '\0' || strchr(name, '=') != NULL)
    {
        errno = EINVAL;
        return -1;
    }
    return _putenv_s(name, "");
}
#endif
