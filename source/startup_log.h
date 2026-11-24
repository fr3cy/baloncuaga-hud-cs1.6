#pragma once
#include <windows.h>
// Privacy build: no file logger, debug output, identifiers or runtime paths.
namespace StartupLog {
static void configure(HMODULE){}
static void write(const char *){}
}
