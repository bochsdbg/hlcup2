#pragma once

static __inline__ unsigned long long rdtsc() {
    unsigned hi, lo;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<unsigned long long>(lo)) | (static_cast<unsigned long long>(hi) << 32);
}

static __inline__ unsigned long long rdtscp() {
    unsigned hi, lo;
    __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi) : : "%ecx");
    return (static_cast<unsigned long long>(lo)) | (static_cast<unsigned long long>(hi) << 32);
}
