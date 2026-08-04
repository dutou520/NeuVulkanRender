
#ifndef NEUGUI_API_H
#define NEUGUI_API_H

#ifdef NEUGUI_STATIC_DEFINE
#  define NEUGUI_API
#  define NEUGUI_NO_EXPORT
#else
#  ifndef NEUGUI_API
#    ifdef NeuGUILib_EXPORTS
        /* We are building this library */
#      define NEUGUI_API 
#    else
        /* We are using this library */
#      define NEUGUI_API 
#    endif
#  endif

#  ifndef NEUGUI_NO_EXPORT
#    define NEUGUI_NO_EXPORT 
#  endif
#endif

#ifndef NEUGUI_DEPRECATED
#  define NEUGUI_DEPRECATED __declspec(deprecated)
#endif

#ifndef NEUGUI_DEPRECATED_EXPORT
#  define NEUGUI_DEPRECATED_EXPORT NEUGUI_API NEUGUI_DEPRECATED
#endif

#ifndef NEUGUI_DEPRECATED_NO_EXPORT
#  define NEUGUI_DEPRECATED_NO_EXPORT NEUGUI_NO_EXPORT NEUGUI_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef NEUGUI_NO_DEPRECATED
#    define NEUGUI_NO_DEPRECATED
#  endif
#endif

#endif /* NEUGUI_API_H */
