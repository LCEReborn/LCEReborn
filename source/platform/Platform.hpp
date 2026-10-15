#ifndef PLATFORM_PLATFORM_HPP
#define PLATFORM_PLATFORM_HPP

#if !defined(LCE_PLATFORM_WIIU) && !defined(LCE_PLATFORM_PC)
#if defined(__WIIU__)
#define LCE_PLATFORM_WIIU 1
#elif defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
#define LCE_PLATFORM_PC 1
#else
#error "Unknown target platform"
#endif
#endif

#if defined(LCE_PLATFORM_WIIU) && defined(LCE_PLATFORM_PC)
#error "More than one target platform"
#endif

#endif
