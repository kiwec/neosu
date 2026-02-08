// include this before including ANY windows API headers
#pragma once
#ifdef NOMINMAX
#undef NOMINMAX
#endif
#ifdef NOWINRES
#undef NOWINRES
#endif
#ifdef NOSERVICE
#undef NOSERVICE
#endif
#ifdef NOMCX
#undef NOMCX
#endif
#ifdef NOIME
#undef NOIME
#endif
#ifdef NOCRYPT
#undef NOCRYPT
#endif
#ifdef NOMETAFILE
#undef NOMETAFILE
#endif
#ifdef MMNOSOUND
#undef MMNOSOUND
#endif
#ifdef NOATOM
#undef NOATOM
#endif
#ifdef NOGDI
#undef NOGDI
#endif
#ifdef NOGDICAPMASKS
#undef NOGDICAPMASKS
#endif
#ifdef NOOPENFILE
#undef NOOPENFILE
#endif
#ifdef NORASTEROPS
#undef NORASTEROPS
#endif
#ifdef NOSCROLL
#undef NOSCROLL
#endif
#ifdef NOSOUND
#undef NOSOUND
#endif
#ifdef NOSYSMETRICS
#undef NOSYSMETRICS
#endif
#ifdef NOTEXTMETRIC
#undef NOTEXTMETRIC
#endif
#ifdef NOWH
#undef NOWH
#endif
#ifdef NOCOMM
#undef NOCOMM
#endif
#ifdef NOKANJI
#undef NOKANJI
#endif

#ifdef VC_EXTRALEAN
#undef VC_EXTRALEAN
#endif
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#define NOMINMAX
#define NOWINRES
#define NOSERVICE
#define NOMCX
#define NOIME
#define NOCRYPT
#define NOMETAFILE
#define MMNOSOUND
#define NOATOM
#define NOGDI
#define NOGDICAPMASKS
#define NOOPENFILE
#define NORASTEROPS
#define NOSCROLL
#define NOSYSMETRICS
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
