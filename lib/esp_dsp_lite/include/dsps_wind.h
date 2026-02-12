// ESP-DSP Lite — unified window function header (ANSI C fallback for native tests)
#ifndef DSPS_WIND_H
#define DSPS_WIND_H

#ifdef __cplusplus
extern "C" {
#endif

void dsps_wind_hann_f32(float *window, int len);
void dsps_wind_blackman_f32(float *window, int len);
void dsps_wind_blackman_harris_f32(float *window, int len);
void dsps_wind_blackman_nuttall_f32(float *window, int len);
void dsps_wind_nuttall_f32(float *window, int len);
void dsps_wind_flat_top_f32(float *window, int len);

#ifdef __cplusplus
}
#endif

#endif // DSPS_WIND_H
