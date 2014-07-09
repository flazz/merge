CFLAGS = -O2 -std=c99
#CFLAGS = -g -std=c99
merge: merge.c debug.h bitset.h
	$(CC) $(CFLAGS) $< -o $@
