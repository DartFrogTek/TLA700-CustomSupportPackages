// compat.h - Compatibility header for Visual C++ 6.0
#pragma once

#ifndef COMPAT_H
#define COMPAT_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef int my_bool;
#define MY_TRUE 1
#define MY_FALSE 0

// C99 types not available in VC6
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned __int64 uint64_t;
typedef __int64 int64_t;

// Define ARRAY_SIZE macro
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Completely disable LogDebug
#define LogDebug

// Function replacements
#define snprintf _snprintf
#define vsnprintf _vsnprintf

#endif // COMPAT_H