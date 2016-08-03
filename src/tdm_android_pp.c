#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tdm_android.h"

void
tdm_android_pp_cb(int fd, unsigned int prop_id, unsigned int *buf_idx,
                 unsigned int tv_sec, unsigned int tv_usec,
                 void *user_data)
{
	tdm_android_pp_handler(prop_id, buf_idx, tv_sec, tv_usec, user_data);
}

void
tdm_android_pp_handler(unsigned int prop_id, unsigned int *buf_idx,
                      unsigned int tv_sec, unsigned int tv_usec, void *data)
{
}

tdm_error
tdm_android_pp_get_capability(tdm_android_data *android_data, tdm_caps_pp *caps)
{
	return TDM_ERROR_NONE;
}

tdm_pp *
tdm_android_pp_create(tdm_android_data *android_data, tdm_error *error)
{
	return NULL;
}

void
android_pp_destroy(tdm_pp *pp)
{
}

tdm_error
android_pp_set_info(tdm_pp *pp, tdm_info_pp *info)
{
	return TDM_ERROR_NONE;
}

tdm_error
android_pp_attach(tdm_pp *pp, tbm_surface_h src, tbm_surface_h dst)
{
	return TDM_ERROR_NONE;
}

tdm_error
android_pp_commit(tdm_pp *pp)
{
	return TDM_ERROR_NONE;
}

tdm_error
android_pp_set_done_handler(tdm_pp *pp, tdm_pp_done_handler func,
                           void *user_data)
{
	return TDM_ERROR_NONE;
}
