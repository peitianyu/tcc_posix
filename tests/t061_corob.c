/* t061_corob: verify __bound_add_region registers a coroutine (ucontext)
 * stack as a checked region, so an EXPLICIT out-of-bounds access on it is
 * reported by -b (bcheck), even though the buffer's owning frame is not the
 * top of the current stack during context switches.
 *
 * Stack is a static global here (also covered by static-bounds), but the path
 * exercised is the same __bound_add_region call makecontext performs for any
 * user buffer, incl. borrowed/heap sub-buffers.
 *
 * Expected: WITHOUT -b  -> exit 0 (plain run, silent stack smash)
 *           WITH    -b  -> bcheck aborts with an out-of-region error
 */
#include <ucontext.h>
#include <string.h>

static ucontext_t main_ctx, co;
static char st[4096] __attribute__((aligned(16)));
static volatile int fail = 0;

static void co_fn(void)
{
    /* explicit overflow beyond the coroutine stack top */
    char *p = st + sizeof st;
    p[0] = 1;   /* -b: 0x.. is outside of the region -> abort */
    fail = 1;
    swapcontext(&co, &main_ctx);
}

int main(void)
{
    co.uc_stack.ss_sp   = st;
    co.uc_stack.ss_size = sizeof st;
    makecontext(&co, co_fn, 0);

    swapcontext(&main_ctx, &co);
    (void)fail;
    return 0;
}