#pragma once

#include <stdio.h>

// debug utilities

#define DEBUG
//#define VERBOSE

#ifdef DEBUG

#define Log(fmt, ...) fprintf(stdout, fmt, __VA_ARGS__)
#define Err(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)

#ifdef VERBOSE
#define LogV(fmt, ...) fprintf(stdout, fmt, __VA_ARGS__)
#else
#define LogV(fmt, ...)
#endif	// VERBOSE

#else

#define Log(fmt, ...) 
#define Err(fmt, ...) 
#define LogV(fmt, ...)

#endif	// DEBUG