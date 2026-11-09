#ifndef KERNEL_INFO_H
#define KERNEL_INFO_H

#define KERNEL_NAME "Qunetra"

#define VER_MAJOR      0
#define VER_MINOR      9
#define VER_PATCH      1
#define VER_REVISION   0

#define VER_YEAR       26
#define VER_MONTH      11
#define VER_DAY        9

#define _STRINGIFY2(x) #x
#define _STRINGIFY(x)  _STRINGIFY2(x)

#define KERNEL_VERSION_STR  \
_STRINGIFY(VER_MAJOR)    "." \
_STRINGIFY(VER_MINOR)    "." \
_STRINGIFY(VER_PATCH)    "." \
_STRINGIFY(VER_REVISION) ":" \
_STRINGIFY(VER_YEAR)     "." \
_STRINGIFY(VER_MONTH)    "." \
_STRINGIFY(VER_DAY)

#endif
