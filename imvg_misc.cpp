#include "imvg.h"

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

void* ImVG::Utils::DllLoad(const char* name)
{
#ifdef WIN32
    return (void*)LoadLibraryA(name);
#else
    return nullptr;
#endif
}

void ImVG::Utils::DllUnload(void* dll)
{
#ifdef WIN32
    FreeLibrary((HMODULE)dll);
#endif
}

void* ImVG::Utils::DllFindFunction(void* dll, const char* name)
{
#ifdef WIN32
    return GetProcAddress((HMODULE)dll, name);
#else
    return nullptr;
#endif
}
