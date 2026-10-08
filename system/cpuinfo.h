/* ============================================================
 * cpuinfo.h — CPU info commands
 *
 *   cpuid()  — identification: vendor, brand, family/model/step,
 *              CPUID signature
 *   fcpuid() — full dump: identification + topology + frequency
 *              + caches + all feature flags
 *   hcpuid() — hardware: cores, threads, SMT, topology levels,
 *              caches, cache line size, frequency
 *
 * Требует: video.h (print), x86 / x86_64
 * ============================================================ */

#ifndef CPUINFO_H
#define CPUINFO_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * cpuid — identification command
 * Prints: brand, vendor, family/model/step, raw family/model,
 *         CPUID signature (EAX of leaf 1)
 */
void cpuid(void);

/*
 * fcpuid — full CPU info
 * Prints everything: identification, topology, frequency,
 *                    caches, all feature flags (SSE..AVX512, AES, ...)
 */
void fcpuid(void);

/*
 * hcpuid — hardware / microarchitecture info
 * Prints: logical CPUs, physical cores, threads/core, SMT status,
 *         APIC ID, extended topology (leaf 0x0B), caches,
 *         cache line size, base/turbo/bus frequency
 */
void hcpuid(void);

#ifdef __cplusplus
}
#endif

#endif /* CPUINFO_H */