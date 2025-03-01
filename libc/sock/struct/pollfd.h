#ifndef COSMOPOLITAN_LIBC_SOCK_STRUCT_POLLFD_H_
#define COSMOPOLITAN_LIBC_SOCK_STRUCT_POLLFD_H_
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

struct pollfd {
  int32_t fd;
  int16_t events;
  int16_t revents;
};

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_SOCK_STRUCT_POLLFD_H_ */
