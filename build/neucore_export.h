
#ifndef NEUCORE_API_H
#define NEUCORE_API_H

#ifdef NEUCORE_STATIC_DEFINE
#  define NEUCORE_API
#  define NEUCORE_NO_EXPORT
#else
#  ifndef NEUCORE_API
#    ifdef NeuCoreLib_EXPORTS
        /* We are building this library */
#      define NEUCORE_API 
#    else
        /* We are using this library */
#      define NEUCORE_API 
#    endif
#  endif

#  ifndef NEUCORE_NO_EXPORT
#    define NEUCORE_NO_EXPORT 
#  endif
#endif

#ifndef NEUCORE_DEPRECATED
#  define NEUCORE_DEPRECATED __declspec(deprecated)
#endif

#ifndef NEUCORE_DEPRECATED_EXPORT
#  define NEUCORE_DEPRECATED_EXPORT NEUCORE_API NEUCORE_DEPRECATED
#endif

#ifndef NEUCORE_DEPRECATED_NO_EXPORT
#  define NEUCORE_DEPRECATED_NO_EXPORT NEUCORE_NO_EXPORT NEUCORE_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef NEUCORE_NO_DEPRECATED
#    define NEUCORE_NO_DEPRECATED
#  endif
#endif

#endif /* NEUCORE_API_H */
