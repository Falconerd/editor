#include <intrin.h>

usize ctz(usize mask) {
    unsigned long i;
    _BitScanForward(&i, mask);
    return (usize)i;
}
