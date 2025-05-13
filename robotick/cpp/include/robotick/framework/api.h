#pragma once

#if defined(_WIN32)
  #if defined(ROBOTICK_EXPORTS)
    #define ROBOTICK_API __declspec(dllexport)
  #else
    #define ROBOTICK_API __declspec(dllimport)
  #endif
#else
  #define ROBOTICK_API
#endif

#ifdef _MSC_VER
    #define ROBOTICK_DECLARE_PIMPL() \
        struct Impl; \
        __pragma(warning(push)) \
        __pragma(warning(disable: 4251)) \
        std::unique_ptr<Impl> m_impl; \
        __pragma(warning(pop))
#else
    #define ROBOTICK_DECLARE_PIMPL() \
        struct Impl; \
        std::unique_ptr<Impl> m_impl
#endif

