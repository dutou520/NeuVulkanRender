
#ifndef NEULOG_API_H
#define NEULOG_API_H

#ifdef NEULOG_STATIC_DEFINE
#  define NEULOG_API
#  define NEULOG_NO_EXPORT
#else
#  ifndef NEULOG_API
#    ifdef NeuLogLib_EXPORTS
        /* We are building this library */
#      define NEULOG_API 
#    else
        /* We are using this library */
#      define NEULOG_API 
#    endif
#  endif

#  ifndef NEULOG_NO_EXPORT
#    define NEULOG_NO_EXPORT 
#  endif
#endif

#ifndef NEULOG_DEPRECATED
#  define NEULOG_DEPRECATED __declspec(deprecated)
#endif

#ifndef NEULOG_DEPRECATED_EXPORT
#  define NEULOG_DEPRECATED_EXPORT NEULOG_API NEULOG_DEPRECATED
#endif

#ifndef NEULOG_DEPRECATED_NO_EXPORT
#  define NEULOG_DEPRECATED_NO_EXPORT NEULOG_NO_EXPORT NEULOG_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef NEULOG_NO_DEPRECATED
#    define NEULOG_NO_DEPRECATED
#  endif
#endif

#endif /* NEULOG_API_H */
