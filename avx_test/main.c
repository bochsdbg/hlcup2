#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <immintrin.h>
#include <xmmintrin.h>

#define DEBUG(x) write(1, x, strlen(x))

int main()
{
    DEBUG("testing _mm256_zeroall(): ");
    _mm256_zeroall();
    DEBUG("ok\n");
    DEBUG("testing  _mm256_zeroupper(): ");
     _mm256_zeroupper();
     DEBUG("ok\n");
    return 0;
}
