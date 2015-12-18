#ifndef _TDM_EXYNOS_FORMAT_H_
#define _TDM_EXYNOS_FORMAT_H_

#include "tdm_exynos.h"

uint32_t     tdm_exynos_format_to_drm_format(tbm_format format);
tbm_format   tdm_exynos_format_to_tbm_format(uint32_t format);

#endif /* _TDM_EXYNOS_FORMAT_H_ */
