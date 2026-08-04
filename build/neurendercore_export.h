
#ifndef NEURENDERCORE_API_H
#define NEURENDERCORE_API_H

#ifdef NEURENDERCORE_STATIC_DEFINE
#  define NEURENDERCORE_API
#  define NEURENDERCORE_NO_EXPORT
#else
#  ifndef NEURENDERCORE_API
#    ifdef NeuRenderCoreLib_EXPORTS
        /* We are building this library */
#      define NEURENDERCORE_API 
#    else
        /* We are using this library */
#      define NEURENDERCORE_API 
#    endif
#  endif

#  ifndef NEURENDERCORE_NO_EXPORT
#    define NEURENDERCORE_NO_EXPORT 
#  endif
#endif

#ifndef NEURENDERCORE_DEPRECATED
#  define NEURENDERCORE_DEPRECATED __declspec(deprecated)
#endif

#ifndef NEURENDERCORE_DEPRECATED_EXPORT
#  define NEURENDERCORE_DEPRECATED_EXPORT NEURENDERCORE_API NEURENDERCORE_DEPRECATED
#endif

#ifndef NEURENDERCORE_DEPRECATED_NO_EXPORT
#  define NEURENDERCORE_DEPRECATED_NO_EXPORT NEURENDERCORE_NO_EXPORT NEURENDERCORE_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef NEURENDERCORE_NO_DEPRECATED
#    define NEURENDERCORE_NO_DEPRECATED
#  endif
#endif

#endif /* NEURENDERCORE_API_H */
