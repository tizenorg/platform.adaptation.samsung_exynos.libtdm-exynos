#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tdm_helper.h>
#include "tdm_android.h"


tdm_error
tdm_android_display_create_layer_list(tdm_android_data *android_data)
{
	return TDM_ERROR_NONE;
}

void
tdm_android_display_destroy_output_list(tdm_android_data *android_data)
{
}

void
tdm_android_display_update_output_status(tdm_android_data *android_data)
{
}

tdm_error
tdm_android_display_create_output_list(tdm_android_data *android_data)
{
	return TDM_ERROR_NONE;
}

tdm_error
tdm_android_display_set_property(tdm_android_data *android_data,
                                unsigned int obj_id, unsigned int obj_type,
                                const char *name, unsigned int value)
{
	return TDM_ERROR_NONE;
}

tdm_error
tdm_android_display_get_property(tdm_android_data *android_data,
                                unsigned int obj_id, unsigned int obj_type,
                                const char *name, unsigned int *value, int *is_immutable)
{
	return TDM_ERROR_NONE;
}

tdm_android_display_buffer *
tdm_android_display_find_buffer(tdm_android_data *android_data,
                               tbm_surface_h buffer)
{
	return NULL;
}

tdm_error
android_display_get_capabilitiy(tdm_backend_data *bdata, tdm_caps_display *caps)
{
	return TDM_ERROR_NONE;
}

tdm_error
android_display_get_pp_capability(tdm_backend_data *bdata, tdm_caps_pp *caps)
{
	return tdm_android_pp_get_capability(bdata, caps);
}

tdm_output **
android_display_get_outputs(tdm_backend_data *bdata, int *count,
                           tdm_error *error)
{
	return NULL;
}

tdm_error
android_display_get_fd(tdm_backend_data *bdata, int *fd)
{
	*fd = -1;

	return TDM_ERROR_NONE;
}

tdm_error
android_display_handle_events(tdm_backend_data *bdata)
{
	return TDM_ERROR_NONE;
}

tdm_pp *
android_display_create_pp(tdm_backend_data *bdata, tdm_error *error)
{
	return NULL;
}
