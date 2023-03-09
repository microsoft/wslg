// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <iostream>
#include <cassert>
#include <string>
#include <windows.h>
#include <wrl.h>
#include <objidl.h>   /* For IPersistFile */
#include <shlobj.h>   /* For IShellLink */
#include <shlwapi.h>  /* For PathIsDirectoryEmpty */
#include <shellapi.h> /* For SHGetPropertyStoreForWindow */
#include <propvarutil.h> /* For InitPropVariantFromString */
#include <propkey.h> /* For PKEY_* */
#include <inttypes.h> /* For PRIx64 */
