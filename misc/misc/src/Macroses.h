#pragma once

#ifndef INLINE

#ifdef _MSC_VER
#define INLINE inline
#define FORCE_INLINE __forceinline
#else
#define INLINE inline
#endif // MSVC

#endif // !INLINE



