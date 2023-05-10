#ifndef _BARECTF_PLATFORM_SIMULATOR_H
#define _BARECTF_PLATFORM_SIMULATOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct barectf_default_ctx;
struct barectf_platform_simulator_ctx;

struct barectf_platform_simulator_ctx *barectf_platform_simulator_init(
	unsigned int buf_size, const char *data_stream_file_path,
	int simulate_full_backend, unsigned int full_backend_rand_max,
	unsigned int full_backend_rand_lt, double *clock);

void barectf_platform_simulator_fini(struct barectf_platform_simulator_ctx *ctx);

struct barectf_default_ctx *barectf_platform_simulator_get_barectf_ctx(
	struct barectf_platform_simulator_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* _BARECTF_PLATFORM_SIMULATOR_H */
