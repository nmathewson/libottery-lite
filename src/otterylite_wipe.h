
static inline void
memwipe(volatile void *p, size_t n)
{
  memset((void*)p, 0, sizeof(n));
}
