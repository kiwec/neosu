#if defined(__aarch64__) && defined(__GNUC__) && !defined(__clang__) && defined(_FORTIFY_SOURCE)
#include <setjmp.h>
#ifdef __cplusplus
extern "C" {void siglongjmp(jmp_buf env, int val);int sigsetjmp(jmp_buf env, int savemask);}
#endif
void siglongjmp(jmp_buf env, int val){longjmp(env, val);}
int sigsetjmp(jmp_buf env, int /*savemask*/){return setjmp(env);}
#endif
