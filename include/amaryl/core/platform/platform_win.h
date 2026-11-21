#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDICAPMASKS
// #define NOVIRTUALKEYCODES
#define NOMENUS
// #define NOICONS
// #define NOKEYSTATES
// #define NOSYSCOMMANDS
// #define NOGDI
#define NOMEMMGR
#define NOKANJI
#define NOHELP
#define NOMCX
#define NOSERVICE

// @note: for OCR_* definitions in `RGFW.h`
#define OEMRESOURCE
#define _CRT_SECURE_NO_WARNINGS  // NOLINT(*-reserved-identifier)

// clang-format off

#include <Windows.h>

#ifdef USE_DEBUG
	#include <debugapi.h>
  #include <DbgHelp.h>
#endif

// Enable check memory leak by crt.

/** @note: comment the macro definition, conflicted with the basic stdlib functions. */
// #define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#if defined(TEXT)
  #undef TEXT
#endif

#if !defined(TEXT)
#if UNICODE
  #define TEXT_PASTE(x) L## x
  #define TEXT(x) TEXT_PASTE(x)
#else
  #define TEXT(x) x
#endif
#endif

#ifdef _WIN64
  typedef unsigned __int64 SIZE_T;
  typedef          __int64 SSIZE_T;
#else
  typedef unsigned long    SIZE_T;
  typedef          long    SSIZE_T;
#endif

#ifndef COUNTOF
  #define COUNTOF(x) _countof(x)
#endif

// clang-format on
