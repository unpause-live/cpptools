/* Copyright (c) 2020 Unpause, SAS.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3.0 of the License, or (at your option) any later version.
 *  See the file LICENSE included with this distribution for more
 *  information.
 */

#ifndef __unpause_tools_log_h
#define __unpause_tools_log_h

#include <ctime>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <chrono>

static inline const std::string currentDateTime()
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
	tstruct = *localtime(&now);

    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);

    return buf;
}

#ifndef COMMERCIAL
#define LOG_STR(fmt, ...) "[%s] [%s:%4d] " fmt "\n", currentDateTime().c_str(), __BASE_FILE__, __LINE__, ##__VA_ARGS__
#else
#define LOG_STR(fmt, ...) "[%s] " fmt "\n", currentDateTime().c_str(), ##__VA_ARGS__
#endif

#if LOG_LEVEL >= 0
#define DFatal(fmt, ...) fprintf(stderr, "[F]" LOG_STR(fmt, ##__VA_ARGS__)); fflush(stderr);
#else
#define DFatal(fmt, ...) {}
#endif

#if LOG_LEVEL >= 1
#define DErr(fmt, ...) fprintf(stderr,   "[E]" LOG_STR(fmt, ##__VA_ARGS__)); fflush(stderr);
#else
#define DErr(fmt, ...) {}
#endif

#if LOG_LEVEL >= 2
#define DInfo(fmt, ...) fprintf(stderr,  "[I]" LOG_STR(fmt, ##__VA_ARGS__)); fflush(stderr);
#else
#define DInfo(fmt, ...) {}
#endif

#if LOG_LEVEL >= 3
#define DDbg(fmt, ...) fprintf(stderr,   "[D]" LOG_STR(fmt, ##__VA_ARGS__)); fflush(stderr);
#else
#define DDbg(fmt, ...) {}
#endif

#endif