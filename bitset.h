typedef unsigned long BITSET;
#define BS_BIT(n) (1 << n)
#define BS_CLEAR(set) set = 0;
#define BS_SET(set, n) set |= BS_BIT(n)
#define BS_UNSET(set, n) set &= ~BS_BIT(n)
#define BS_TEST(set, n) set & BS_BIT(n)
