
#ifndef NEUASSET_API_H
#define NEUASSET_API_H

#ifdef NEUASSET_STATIC_DEFINE
#  define NEUASSET_API
#  define NEUASSET_NO_EXPORT
#else
#  ifndef NEUASSET_API
#    ifdef NeuAssetLib_EXPORTS
        /* We are building this library */
#      define NEUASSET_API 
#    else
        /* We are using this library */
#      define NEUASSET_API 
#    endif
#  endif

#  ifndef NEUASSET_NO_EXPORT
#    define NEUASSET_NO_EXPORT 
#  endif
#endif

#ifndef NEUASSET_DEPRECATED
#  define NEUASSET_DEPRECATED __declspec(deprecated)
#endif

#ifndef NEUASSET_DEPRECATED_EXPORT
#  define NEUASSET_DEPRECATED_EXPORT NEUASSET_API NEUASSET_DEPRECATED
#endif

#ifndef NEUASSET_DEPRECATED_NO_EXPORT
#  define NEUASSET_DEPRECATED_NO_EXPORT NEUASSET_NO_EXPORT NEUASSET_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef NEUASSET_NO_DEPRECATED
#    define NEUASSET_NO_DEPRECATED
#  endif
#endif

#endif /* NEUASSET_API_H */
