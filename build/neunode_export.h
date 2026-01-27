
#ifndef NEUNODE_API_H
#define NEUNODE_API_H

#ifdef NEUNODE_STATIC_DEFINE
#  define NEUNODE_API
#  define NEUNODE_NO_EXPORT
#else
#  ifndef NEUNODE_API
#    ifdef NeuNodeLib_EXPORTS
        /* We are building this library */
#      define NEUNODE_API 
#    else
        /* We are using this library */
#      define NEUNODE_API 
#    endif
#  endif

#  ifndef NEUNODE_NO_EXPORT
#    define NEUNODE_NO_EXPORT 
#  endif
#endif

#ifndef NEUNODE_DEPRECATED
#  define NEUNODE_DEPRECATED __declspec(deprecated)
#endif

#ifndef NEUNODE_DEPRECATED_EXPORT
#  define NEUNODE_DEPRECATED_EXPORT NEUNODE_API NEUNODE_DEPRECATED
#endif

#ifndef NEUNODE_DEPRECATED_NO_EXPORT
#  define NEUNODE_DEPRECATED_NO_EXPORT NEUNODE_NO_EXPORT NEUNODE_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef NEUNODE_NO_DEPRECATED
#    define NEUNODE_NO_DEPRECATED
#  endif
#endif

#endif /* NEUNODE_API_H */
