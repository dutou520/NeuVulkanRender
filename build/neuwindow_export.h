
#ifndef NEUWINDOW_API_H
#define NEUWINDOW_API_H

#ifdef NEUWINDOW_STATIC_DEFINE
#  define NEUWINDOW_API
#  define NEUWINDOW_NO_EXPORT
#else
#  ifndef NEUWINDOW_API
#    ifdef NeuWindowLib_EXPORTS
        /* We are building this library */
#      define NEUWINDOW_API 
#    else
        /* We are using this library */
#      define NEUWINDOW_API 
#    endif
#  endif

#  ifndef NEUWINDOW_NO_EXPORT
#    define NEUWINDOW_NO_EXPORT 
#  endif
#endif

#ifndef NEUWINDOW_DEPRECATED
#  define NEUWINDOW_DEPRECATED __declspec(deprecated)
#endif

#ifndef NEUWINDOW_DEPRECATED_EXPORT
#  define NEUWINDOW_DEPRECATED_EXPORT NEUWINDOW_API NEUWINDOW_DEPRECATED
#endif

#ifndef NEUWINDOW_DEPRECATED_NO_EXPORT
#  define NEUWINDOW_DEPRECATED_NO_EXPORT NEUWINDOW_NO_EXPORT NEUWINDOW_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef NEUWINDOW_NO_DEPRECATED
#    define NEUWINDOW_NO_DEPRECATED
#  endif
#endif

#endif /* NEUWINDOW_API_H */
