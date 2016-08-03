#ifndef _TDM_ANDROID_PP_H_
#define _TDM_ANDROID_PP_H_

#include "tdm_android.h"

tdm_error    tdm_android_pp_get_capability(tdm_android_data *android_data, tdm_caps_pp *caps);
tdm_pp*      tdm_android_pp_create(tdm_android_data *android_data, tdm_error *error);
void         tdm_android_pp_handler(unsigned int prop_id, unsigned int *buf_idx,
                                   unsigned int tv_sec, unsigned int tv_usec, void *data);
void         tdm_android_pp_cb(int fd, unsigned int prop_id, unsigned int *buf_idx,
                              unsigned int tv_sec, unsigned int tv_usec,
                              void *user_data);
#endif /* _TDM_ANDROID_PP_H_ */
