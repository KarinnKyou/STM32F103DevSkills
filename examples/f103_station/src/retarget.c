/* GCC replacement for the Wildfire examples' Keil-only printf retarget.
 *
 * The vendor examples redirect printf with `int fputc(int, FILE *)`, which is
 * an ARM Compiler / Keil mechanism. Under newlib (arm-none-eabi-gcc) printf
 * ends up in `_write`, which by default is the "not implemented" stub, so the
 * program links fine and then prints nothing at all. Define `_write` here to
 * push bytes out of USART1 (PA9) with plain polling.
 *
 * Requires USART1 to be initialised first (bsp_debug_usart does that at
 * 115200 8N1).
 */
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>

#include "stm32f1xx.h"

int _write(int fd, char *ptr, int len)
{
    (void)fd;
    for (int i = 0; i < len; i++) {
        while ((USART1->SR & USART_SR_TXE) == 0U) {
        }
        USART1->DR = (uint8_t)ptr[i];
    }
    return len;
}

int _read(int fd, char *ptr, int len)
{
    (void)fd;
    (void)ptr;
    (void)len;
    return 0;
}

int _close(int fd) { (void)fd; return -1; }
int _fstat(int fd, struct stat *st)
{
    (void)fd;
    st->st_mode = S_IFCHR;
    return 0;
}
int _isatty(int fd) { (void)fd; return 1; }
int _lseek(int fd, int ptr, int dir) { (void)fd; (void)ptr; (void)dir; return 0; }
int _getpid(void) { return 1; }
int _kill(int pid, int sig) { (void)pid; (void)sig; errno = EINVAL; return -1; }
