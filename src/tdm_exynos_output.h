#ifndef _TDM_EXYNOS_OUTPUT_H_
#define _TDM_EXYNOS_OUTPUT_H_

#include "tdm_exynos.h"

void tdm_exynos_output_cb_vblank(int fd, unsigned int sequence,
                                 unsigned int tv_sec, unsigned int tv_usec,
                                 void *user_data);

#endif /* _TDM_EXYNOS_OUTPUT_H_ */
