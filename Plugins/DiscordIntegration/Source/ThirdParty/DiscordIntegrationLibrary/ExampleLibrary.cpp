#if defined _WIN32 || defined _WIN64
    #define WIN32_LEAN_AND_MEAN
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"

#include "Windows.h"

#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"

    #define EXAMPLELIBRARY_EXPORT __declspec(dllexport)
#else
    #include <stdio.h>
#endif

#ifndef EXAMPLELIBRARY_EXPORT
    #define EXAMPLELIBRARY_EXPORT
#endif

EXAMPLELIBRARY_EXPORT void ExampleLibraryFunction()
{
#if defined _WIN32 || defined _WIN64
	MessageBox(NULL, TEXT("Loaded ExampleLibrary.dll from Third Party Plugin sample."), TEXT("Third Party Plugin"), MB_OK);
#else
    printf("Loaded ExampleLibrary from Third Party Plugin sample");
#endif
}