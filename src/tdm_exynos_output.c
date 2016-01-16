#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <drm_fourcc.h>
#include <tdm_helper.h>
#include "tdm_exynos.h"

#define MIN_WIDTH   32

static tdm_error
check_hw_restriction(unsigned int crtc_w, unsigned int buf_w,
                     unsigned int src_x, unsigned int src_w, unsigned int dst_x, unsigned int dst_w,
                     unsigned int *new_src_x, unsigned int *new_src_w,
                     unsigned int *new_dst_x, unsigned int *new_dst_w)
{
    int start, end, diff;
    int virtual_screen;

    *new_src_x = src_x;
    *new_src_w = src_w;
    *new_dst_x = dst_x;
    *new_dst_w = dst_w;

    if (buf_w < MIN_WIDTH || buf_w % 2)
    {
        TDM_ERR("buf_w(%d) not 2's multiple or less than %d", buf_w, MIN_WIDTH);
        return TDM_ERROR_BAD_REQUEST;
    }

    if (src_x > dst_x || ((dst_x - src_x) + buf_w) > crtc_w)
        virtual_screen = 1;
    else
        virtual_screen = 0;

    start = (dst_x < 0) ? 0 : dst_x;
    end = ((dst_x + dst_w) > crtc_w) ? crtc_w : (dst_x + dst_w);

    /* check window minimun width */
    if ((end - start) < MIN_WIDTH)
    {
        TDM_ERR("visible_w(%d) less than %d", end-start, MIN_WIDTH);
        return TDM_ERROR_BAD_REQUEST;
    }

    if (!virtual_screen)
    {
        /* Pagewidth of window (= 8 byte align / bytes-per-pixel ) */
        if ((end - start) % 2)
            end--;
    }
    else
    {
        /* You should align the sum of PAGEWIDTH_F and OFFSIZE_F double-word (8 byte) boundary. */
        if (end % 2)
            end--;
    }

    *new_dst_x = start;
    *new_dst_w = end - start;
    *new_src_w = *new_dst_w;
    diff = start - dst_x;
    *new_src_x += diff;

    RETURN_VAL_IF_FAIL(*new_src_w > 0, TDM_ERROR_BAD_REQUEST);
    RETURN_VAL_IF_FAIL(*new_dst_w > 0, TDM_ERROR_BAD_REQUEST);

    if (src_x != *new_src_x || src_w != *new_src_w || dst_x != *new_dst_x || dst_w != *new_dst_w)
        TDM_DBG("=> buf_w(%d) src(%d,%d) dst(%d,%d), virt(%d) start(%d) end(%d)",
                buf_w, *new_src_x, *new_src_w, *new_dst_x, *new_dst_w, virtual_screen, start, end);

    return TDM_ERROR_NONE;
}

static drmModeModeInfoPtr
_tdm_exynos_output_get_mode(tdm_exynos_output_data *output_data)
{
    int i;

    if (!output_data->current_mode)
    {
        TDM_ERR("no output_data->current_mode");
        return NULL;
    }

    for (i = 0; i < output_data->count_modes; i++)
    {
        drmModeModeInfoPtr drm_mode = &output_data->drm_modes[i];
        if ((drm_mode->hdisplay == output_data->current_mode->width) &&
            (drm_mode->vdisplay == output_data->current_mode->height) &&
            (drm_mode->vrefresh == output_data->current_mode->refresh) &&
            (drm_mode->flags == output_data->current_mode->flags) &&
            (drm_mode->type == output_data->current_mode->type) &&
            !(strncmp(drm_mode->name, output_data->current_mode->name, TDM_NAME_LEN)))
            return drm_mode;
    }

    return NULL;
}

static tdm_error
_tdm_exynos_output_get_cur_msc (int fd, int pipe, uint *msc)
{
    drmVBlank vbl;

    vbl.request.type = DRM_VBLANK_RELATIVE;
    if (pipe > 0)
        vbl.request.type |= DRM_VBLANK_SECONDARY;

    vbl.request.sequence = 0;
    if (drmWaitVBlank(fd, &vbl))
    {
        TDM_ERR("get vblank counter failed: %m");
        *msc = 0;
        return TDM_ERROR_OPERATION_FAILED;
    }

    *msc = vbl.reply.sequence;

    return TDM_ERROR_NONE;
}

static tdm_error
_tdm_exynos_output_wait_vblank(int fd, int pipe, uint *target_msc, void *data)
{
    drmVBlank vbl;

    vbl.request.type =  DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
    if (pipe > 0)
        vbl.request.type |= DRM_VBLANK_SECONDARY;

    vbl.request.sequence = *target_msc;
    vbl.request.signal = (unsigned long)(uintptr_t)data;

    if (drmWaitVBlank(fd, &vbl))
    {
        *target_msc = 0;
        TDM_ERR("wait vblank failed: %m");
        return TDM_ERROR_OPERATION_FAILED;
    }

    *target_msc = vbl.reply.sequence;

    return TDM_ERROR_NONE;
}

static tdm_error
_tdm_exynos_output_commit_primary_layer(tdm_exynos_layer_data *layer_data,
                tdm_exynos_output_data *output, void *user_data, int *do_waitvblank)
{
    tdm_exynos_data *exynos_data = layer_data->exynos_data;
    tdm_exynos_output_data *output_data = layer_data->output_data;
    *do_waitvblank = 0;

    if (output_data->mode_changed && layer_data->display_buffer_changed)
    {
        drmModeModeInfoPtr mode;

        if (!layer_data->display_buffer)
        {
            TDM_ERR("primary layer should have a buffer for modestting");
            return TDM_ERROR_BAD_REQUEST;
        }

        output_data->mode_changed = 0;
        layer_data->display_buffer_changed = 0;
        layer_data->info_changed = 0;

        mode = _tdm_exynos_output_get_mode(output_data);
        if (!mode)
        {
            TDM_ERR("couldn't find proper mode");
            return TDM_ERROR_BAD_REQUEST;
        }

        if (drmModeSetCrtc(exynos_data->drm_fd, output_data->crtc_id,
                           layer_data->display_buffer->fb_id, 0, 0,
                           &output_data->connector_id, 1, mode))
        {
            TDM_ERR("set crtc failed: %m");
            return TDM_ERROR_OPERATION_FAILED;
        }
        *do_waitvblank = 1;
        return TDM_ERROR_NONE;
    }
    else if (layer_data->display_buffer_changed)
    {
        layer_data->display_buffer_changed = 0;

        if (!layer_data->display_buffer)
        {
            if (drmModeSetCrtc(exynos_data->drm_fd, output_data->crtc_id,
                               0, 0, 0, NULL, 0, NULL))
            {
                TDM_ERR("unset crtc failed: %m");
                return TDM_ERROR_OPERATION_FAILED;
            }
            *do_waitvblank = 1;
        }
        else
        {
            tdm_exynos_vblank_data *vblank_data = calloc(1, sizeof(tdm_exynos_vblank_data));
            if (!vblank_data)
            {
                TDM_ERR("alloc failed");
                return TDM_ERROR_OUT_OF_MEMORY;
            }
            vblank_data->type = VBLANK_TYPE_PAGEFLIP;
            vblank_data->output_data = output;
            vblank_data->user_data = user_data;
            if (drmModePageFlip(exynos_data->drm_fd, output_data->crtc_id,
                    layer_data->display_buffer->fb_id, DRM_MODE_PAGE_FLIP_EVENT, vblank_data))
            {
                TDM_ERR("pageflip failed: %m");
                return TDM_ERROR_OPERATION_FAILED;
            }
        }
    }

    return TDM_ERROR_NONE;
}

static tdm_error
_tdm_exynos_output_commit_layer(tdm_exynos_layer_data *layer_data)
{
    tdm_exynos_data *exynos_data = layer_data->exynos_data;
    tdm_exynos_output_data *output_data = layer_data->output_data;
    unsigned int new_src_x, new_src_w;
    unsigned int new_dst_x, new_dst_w;
    uint32_t fx, fy, fw, fh;
    int crtc_w;

    if (!layer_data->display_buffer_changed && !layer_data->info_changed)
        return TDM_ERROR_NONE;

    if (output_data->current_mode)
        crtc_w = output_data->current_mode->width;
    else
    {
        drmModeCrtcPtr crtc = drmModeGetCrtc(exynos_data->drm_fd, output_data->crtc_id);
        if (!crtc)
        {
            TDM_ERR("getting crtc failed");
            return TDM_ERROR_OPERATION_FAILED;
        }
        crtc_w = crtc->width;
        if (crtc_w == 0)
        {
            TDM_ERR("getting crtc width failed");
            return TDM_ERROR_OPERATION_FAILED;
        }
    }

    layer_data->display_buffer_changed = 0;
    layer_data->info_changed = 0;

    if (!layer_data->display_buffer)
    {
        if (drmModeSetPlane(exynos_data->drm_fd, layer_data->plane_id,
                            output_data->crtc_id, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
            TDM_ERR("unset plane(%d) filed: %m", layer_data->plane_id);

        return TDM_ERROR_NONE;
    }

    /* check hw restriction*/
    if (check_hw_restriction(crtc_w, layer_data->display_buffer->width,
                             layer_data->info.src_config.pos.x,
                             layer_data->info.src_config.pos.w,
                             layer_data->info.dst_pos.x,
                             layer_data->info.dst_pos.w,
                             &new_src_x, &new_src_w, &new_dst_x, &new_dst_w) != TDM_ERROR_NONE)
    {
        TDM_WRN("not going to set plane(%d)", layer_data->plane_id);
        return TDM_ERROR_NONE;
    }

    if (layer_data->info.src_config.pos.x != new_src_x)
        TDM_DBG("src_x changed: %d => %d", layer_data->info.src_config.pos.x, new_src_x);
    if (layer_data->info.src_config.pos.w != new_src_w)
        TDM_DBG("src_w changed: %d => %d", layer_data->info.src_config.pos.w, new_src_w);
    if (layer_data->info.dst_pos.x != new_dst_x)
        TDM_DBG("dst_x changed: %d => %d", layer_data->info.dst_pos.x, new_dst_x);
    if (layer_data->info.dst_pos.w != new_dst_w)
        TDM_DBG("dst_w changed: %d => %d", layer_data->info.dst_pos.w, new_dst_w);

    /* Source values are 16.16 fixed point */
    fx = ((unsigned int)new_src_x) << 16;
    fy = ((unsigned int)layer_data->info.src_config.pos.y) << 16;
    fw = ((unsigned int)new_src_w) << 16;
    fh = ((unsigned int)layer_data->info.src_config.pos.h) << 16;

    if (drmModeSetPlane(exynos_data->drm_fd, layer_data->plane_id,
                        output_data->crtc_id, layer_data->display_buffer->fb_id, 0,
                        new_dst_x, layer_data->info.dst_pos.y,
                        new_dst_w, layer_data->info.dst_pos.h,
                        fx, fy, fw, fh) < 0)
    {
        TDM_ERR("set plane(%d) failed: %m", layer_data->plane_id);
        return TDM_ERROR_OPERATION_FAILED;
    }

    return TDM_ERROR_NONE;
}

void
tdm_exynos_output_cb_vblank(int fd, unsigned int sequence,
                            unsigned int tv_sec, unsigned int tv_usec,
                            void *user_data)
{
    tdm_exynos_vblank_data *vblank_data = user_data;
    tdm_exynos_output_data *output_data;

    if (!vblank_data)
    {
        TDM_ERR("no vblank data");
        return;
    }

    output_data = vblank_data->output_data;

    switch(vblank_data->type)
    {
    case VBLANK_TYPE_WAIT:
        if (output_data->vblank_func)
            output_data->vblank_func(output_data, sequence, tv_sec, tv_usec, vblank_data->user_data);
        break;
    case VBLANK_TYPE_COMMIT:
        if (output_data->commit_func)
            output_data->commit_func(output_data, sequence, tv_sec, tv_usec, vblank_data->user_data);
        break;
    default:
        break;
    }

    free(vblank_data);
}

void
tdm_exynos_output_cb_pageflip(int fd, unsigned int sequence,
                            unsigned int tv_sec, unsigned int tv_usec,
                            void *user_data)
{
    tdm_exynos_vblank_data *vblank_data = user_data;
    tdm_exynos_output_data *output_data;

    if (!vblank_data)
    {
        TDM_ERR("no vblank data");
        return;
    }

    output_data = vblank_data->output_data;

    switch(vblank_data->type)
    {
    case VBLANK_TYPE_PAGEFLIP:
        if (output_data->commit_func)
            output_data->commit_func(output_data, sequence, tv_sec, tv_usec, vblank_data->user_data);
        break;
    default:
        break;
    }

    free(vblank_data);
}

tdm_error
exynos_output_get_capability(tdm_output *output, tdm_caps_output *caps)
{
    tdm_exynos_output_data *output_data = output;
    tdm_exynos_data *exynos_data;
    drmModeConnectorPtr connector = NULL;
    drmModeCrtcPtr crtc = NULL;
    drmModeObjectPropertiesPtr props = NULL;
    int i;
    tdm_error ret;

    RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(caps, TDM_ERROR_INVALID_PARAMETER);

    memset(caps, 0, sizeof(tdm_caps_output));

    exynos_data = output_data->exynos_data;

    caps->status = output_data->status;
    caps->type = output_data->connector_type;
    caps->type_id = output_data->connector_type_id;

    connector = drmModeGetConnector(exynos_data->drm_fd, output_data->connector_id);
    RETURN_VAL_IF_FAIL(connector, TDM_ERROR_OPERATION_FAILED);

    caps->mode_count = connector->count_modes;
    caps->modes = calloc(1, sizeof(tdm_output_mode) * caps->mode_count);
    if (!caps->modes)
    {
        ret = TDM_ERROR_OUT_OF_MEMORY;
        TDM_ERR("alloc failed\n");
        goto failed_get;
    }
    for (i = 0; i < caps->mode_count; i++)
        caps->modes[i] = output_data->output_modes[i];

    caps->mmWidth = connector->mmWidth;
    caps->mmHeight = connector->mmHeight;
    caps->subpixel = connector->subpixel;

    caps->min_w = exynos_data->mode_res->min_width;
    caps->min_h = exynos_data->mode_res->min_height;
    caps->max_w = exynos_data->mode_res->max_width;
    caps->max_h = exynos_data->mode_res->max_height;
    caps->preferred_align = -1;

    crtc = drmModeGetCrtc(exynos_data->drm_fd, output_data->crtc_id);
    if (!crtc)
    {
        ret = TDM_ERROR_OPERATION_FAILED;
        TDM_ERR("get crtc failed: %m\n");
        goto failed_get;
    }

    props = drmModeObjectGetProperties(exynos_data->drm_fd, output_data->crtc_id, DRM_MODE_OBJECT_CRTC);
    if (!props)
    {
        ret = TDM_ERROR_OPERATION_FAILED;
        TDM_ERR("get crtc properties failed: %m\n");
        goto failed_get;
    }

    caps->prop_count = props->count_props;
    caps->props = calloc(1, sizeof(tdm_prop) * caps->prop_count);
    if (!caps->props)
    {
        ret = TDM_ERROR_OUT_OF_MEMORY;
        TDM_ERR("alloc failed\n");
        goto failed_get;
    }

    for (i = 0; i < caps->prop_count; i++)
    {
        drmModePropertyPtr prop = drmModeGetProperty(exynos_data->drm_fd, props->props[i]);
        if (!prop)
            continue;
        snprintf(caps->props[i].name, TDM_NAME_LEN, "%s", prop->name);
        caps->props[i].id = props->props[i];
        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);
    drmModeFreeCrtc(crtc);
    drmModeFreeConnector(connector);

    return TDM_ERROR_NONE;
failed_get:
    drmModeFreeCrtc(crtc);
    drmModeFreeObjectProperties(props);
    drmModeFreeConnector(connector);
    free(caps->modes);
    free(caps->props);
    memset(caps, 0, sizeof(tdm_caps_output));
    return ret;
}

tdm_layer**
exynos_output_get_layers(tdm_output *output,  int *count, tdm_error *error)
{
    tdm_exynos_output_data *output_data = output;
    tdm_exynos_layer_data *layer_data = NULL;
    tdm_layer **layers;
    tdm_error ret;
    int i;

    RETURN_VAL_IF_FAIL(output_data, NULL);
    RETURN_VAL_IF_FAIL(count, NULL);

    *count = 0;
    LIST_FOR_EACH_ENTRY(layer_data, &output_data->layer_list, link)
        (*count)++;

    if (*count == 0)
    {
        ret = TDM_ERROR_NONE;
        goto failed_get;
    }

    /* will be freed in frontend */
    layers = calloc(*count, sizeof(tdm_exynos_layer_data*));
    if (!layers)
    {
        TDM_ERR("failed: alloc memory");
        *count = 0;
        ret = TDM_ERROR_OUT_OF_MEMORY;
        goto failed_get;
    }

    i = 0;
    LIST_FOR_EACH_ENTRY(layer_data, &output_data->layer_list, link)
        layers[i++] = layer_data;

    if (error)
        *error = TDM_ERROR_NONE;

    return layers;
failed_get:
    if (error)
        *error = ret;
    return NULL;
}

tdm_error
exynos_output_set_property(tdm_output *output, unsigned int id, tdm_value value)
{
    tdm_exynos_output_data *output_data = output;
    tdm_exynos_data *exynos_data;
    int ret;

    RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(output_data->crtc_id > 0, TDM_ERROR_INVALID_PARAMETER);

    exynos_data = output_data->exynos_data;
    ret = drmModeObjectSetProperty(exynos_data->drm_fd,
                                   output_data->crtc_id, DRM_MODE_OBJECT_CRTC,
                                   id, value.u32);
    if (ret < 0)
    {
        TDM_ERR("set property failed: %m");
        return TDM_ERROR_OPERATION_FAILED;
    }

    return TDM_ERROR_NONE;
}

tdm_error
exynos_output_get_property(tdm_output *output, unsigned int id, tdm_value *value)
{
    tdm_exynos_output_data *output_data = output;
    tdm_exynos_data *exynos_data;
    drmModeObjectPropertiesPtr props;
    int i;

    RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(output_data->crtc_id > 0, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(value, TDM_ERROR_INVALID_PARAMETER);

    exynos_data = output_data->exynos_data;
    props = drmModeObjectGetProperties(exynos_data->drm_fd, output_data->crtc_id, DRM_MODE_OBJECT_CRTC);
    if (props == NULL)
    {
        TDM_ERR("get property failed: %m");
        return TDM_ERROR_OPERATION_FAILED;
    }

    for (i = 0; i < props->count_props; i++)
        if (props->props[i] == id)
        {
            (*value).u32 = (uint)props->prop_values[i];
            break;
        }

    drmModeFreeObjectProperties(props);

    return TDM_ERROR_NONE;
}

tdm_error
exynos_output_wait_vblank(tdm_output *output, int interval, int sync, void *user_data)
{
    tdm_exynos_output_data *output_data = output;
    tdm_exynos_data *exynos_data;
    tdm_exynos_vblank_data *vblank_data;
    uint target_msc;
    tdm_error ret;

    RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);

    vblank_data = calloc(1, sizeof(tdm_exynos_vblank_data));
    if (!vblank_data)
    {
        TDM_ERR("alloc failed");
        return TDM_ERROR_OUT_OF_MEMORY;
    }

    exynos_data = output_data->exynos_data;

    ret = _tdm_exynos_output_get_cur_msc(exynos_data->drm_fd, output_data->pipe, &target_msc);
    if (ret != TDM_ERROR_NONE)
        goto failed_vblank;

    target_msc++;

    vblank_data->type = VBLANK_TYPE_WAIT;
    vblank_data->output_data = output_data;
    vblank_data->user_data = user_data;

    ret = _tdm_exynos_output_wait_vblank(exynos_data->drm_fd, output_data->pipe, &target_msc, vblank_data);
    if (ret != TDM_ERROR_NONE)
        goto failed_vblank;

    return TDM_ERROR_NONE;
failed_vblank:
    free(vblank_data);
    return ret;
}

tdm_error
exynos_output_set_vblank_handler(tdm_output *output, tdm_output_vblank_handler func)
{
    tdm_exynos_output_data *output_data = output;

    RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(func, TDM_ERROR_INVALID_PARAMETER);

    output_data->vblank_func = func;

    return TDM_ERROR_NONE;
}

tdm_error
exynos_output_commit(tdm_output *output, int sync, void *user_data)
{
    tdm_exynos_output_data *output_data = output;
    tdm_exynos_data *exynos_data;
    tdm_exynos_layer_data *layer_data = NULL;
    tdm_error ret;
    int do_waitvblank = 0;

    RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);

    exynos_data = output_data->exynos_data;

    LIST_FOR_EACH_ENTRY(layer_data, &output_data->layer_list, link)
    {
        if (layer_data == output_data->primary_layer)
        {
            ret = _tdm_exynos_output_commit_primary_layer(layer_data, output_data, user_data, &do_waitvblank);
            if (ret != TDM_ERROR_NONE)
                return ret;
        }
        else
        {
            ret = _tdm_exynos_output_commit_layer(layer_data);
            if (ret != TDM_ERROR_NONE)
                return ret;
        }
    }

    /* TODO: tdm_helper_drm_fd is external drm_fd which is opened by ecore_drm.
     * This is very tricky. But we can't remove tdm_helper_drm_fd now because
     * ecore_drm doesn't use tdm yet. When we make ecore_drm use tdm,
     * tdm_helper_drm_fd will be removed.
     */
    if ((tdm_helper_drm_fd == -1) && (do_waitvblank == 1))
    {
        tdm_exynos_vblank_data *vblank_data = calloc(1, sizeof(tdm_exynos_vblank_data));
        uint target_msc;

        if (!vblank_data)
        {
            TDM_ERR("alloc failed");
            return TDM_ERROR_OUT_OF_MEMORY;
        }

        ret = _tdm_exynos_output_get_cur_msc(exynos_data->drm_fd, output_data->pipe, &target_msc);
        if (ret != TDM_ERROR_NONE)
        {
            free(vblank_data);
            return ret;
        }

        target_msc++;

        vblank_data->type = VBLANK_TYPE_COMMIT;
        vblank_data->output_data = output_data;
        vblank_data->user_data = user_data;

        ret = _tdm_exynos_output_wait_vblank(exynos_data->drm_fd, output_data->pipe, &target_msc, vblank_data);
        if (ret != TDM_ERROR_NONE)
        {
            free(vblank_data);
            return ret;
        }
    }

    return TDM_ERROR_NONE;
}

tdm_error
exynos_output_set_commit_handler(tdm_output *output, tdm_output_commit_handler func)
{
    tdm_exynos_output_data *output_data = output;

    RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(func, TDM_ERROR_INVALID_PARAMETER);

    output_data->commit_func = func;

    return TDM_ERROR_NONE;
}

tdm_error
exynos_output_set_dpms(tdm_output *output, tdm_output_dpms dpms_value)
{
    tdm_exynos_output_data *output_data = output;
    tdm_exynos_data *exynos_data;
    int ret;

    RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);

    exynos_data = output_data->exynos_data;
    ret = drmModeObjectSetProperty(exynos_data->drm_fd,
                                   output_data->connector_id, DRM_MODE_OBJECT_CONNECTOR,
                                   output_data->dpms_prop_id, dpms_value);
    if (ret < 0)
    {
        TDM_ERR("set dpms failed: %m");
        return TDM_ERROR_OPERATION_FAILED;
    }

    return TDM_ERROR_NONE;
}

tdm_error
exynos_output_get_dpms(tdm_output *output, tdm_output_dpms *dpms_value)
{
    tdm_exynos_output_data *output_data = output;
    tdm_exynos_data *exynos_data;
    drmModeObjectPropertiesPtr props;
    int i;

    RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(dpms_value, TDM_ERROR_INVALID_PARAMETER);

    exynos_data = output_data->exynos_data;
    props = drmModeObjectGetProperties(exynos_data->drm_fd, output_data->connector_id, DRM_MODE_OBJECT_CONNECTOR);
    if (props == NULL)
    {
        TDM_ERR("get property failed: %m");
        return TDM_ERROR_OPERATION_FAILED;
    }

    for (i = 0; i < props->count_props; i++)
        if (props->props[i] == output_data->dpms_prop_id)
        {
            *dpms_value = (uint)props->prop_values[i];
            break;
        }

    drmModeFreeObjectProperties(props);

    return TDM_ERROR_NONE;
}

tdm_error
exynos_output_set_mode(tdm_output *output, const tdm_output_mode *mode)
{
    tdm_exynos_output_data *output_data = output;

    RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(mode, TDM_ERROR_INVALID_PARAMETER);

    output_data->current_mode = mode;
    output_data->mode_changed = 1;

    return TDM_ERROR_NONE;
}

tdm_error
exynos_output_get_mode(tdm_output *output, const tdm_output_mode **mode)
{
    tdm_exynos_output_data *output_data = output;

    RETURN_VAL_IF_FAIL(output_data, TDM_ERROR_INVALID_PARAMETER);
    RETURN_VAL_IF_FAIL(mode, TDM_ERROR_INVALID_PARAMETER);

    *mode = output_data->current_mode;

    return TDM_ERROR_NONE;
}
