
#ifndef NEUSCENE_API_H
#define NEUSCENE_API_H

#ifdef NEUSCENE_STATIC_DEFINE
#  define NEUSCENE_API
#  define NEUSCENE_NO_EXPORT
#else
#  ifndef NEUSCENE_API
#    ifdef NeuSceneLib_EXPORTS
        /* We are building this library */
#      define NEUSCENE_API 
#    else
        /* We are using this library */
#      define NEUSCENE_API 
#    endif
#  endif

#  ifndef NEUSCENE_NO_EXPORT
#    define NEUSCENE_NO_EXPORT 
#  endif
#endif

#ifndef NEUSCENE_DEPRECATED
#  define NEUSCENE_DEPRECATED __declspec(deprecated)
#endif

#ifndef NEUSCENE_DEPRECATED_EXPORT
#  define NEUSCENE_DEPRECATED_EXPORT NEUSCENE_API NEUSCENE_DEPRECATED
#endif

#ifndef NEUSCENE_DEPRECATED_NO_EXPORT
#  define NEUSCENE_DEPRECATED_NO_EXPORT NEUSCENE_NO_EXPORT NEUSCENE_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef NEUSCENE_NO_DEPRECATED
#    define NEUSCENE_NO_DEPRECATED
#  endif
#endif

#endif /* NEUSCENE_API_H */
