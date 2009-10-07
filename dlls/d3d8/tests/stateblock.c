/*
 * Copyright 2006 Ivan Gyurdiev
 * Copyright 2005, 2008 Henri Verbeet
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS
#include <d3d8.h>
#include "wine/test.h"

static DWORD texture_stages;

static HWND create_window(void)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = DefWindowProc;
    wc.lpszClassName = "d3d8_test_wc";
    RegisterClass(&wc);

    return CreateWindow("d3d8_test_wc", "d3d8_test", 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

static HRESULT init_d3d8(HMODULE d3d8_module, IDirect3DDevice8 **device, D3DPRESENT_PARAMETERS *device_pparams)
{
    IDirect3D8 * (WINAPI *d3d8_create)(UINT SDKVersion) = 0;
    D3DDISPLAYMODE d3ddm;
    IDirect3D8 *d3d8 = 0;
    HWND window;
    HRESULT hr;

    d3d8_create = (void *)GetProcAddress(d3d8_module, "Direct3DCreate8");
    if (!d3d8_create) return E_FAIL;

    d3d8 = d3d8_create(D3D_SDK_VERSION);
    if (!d3d8)
    {
        skip("Failed to create D3D8 object.\n");
        return E_FAIL;
    }

    window = create_window();

    IDirect3D8_GetAdapterDisplayMode(d3d8, D3DADAPTER_DEFAULT, &d3ddm);
    memset(device_pparams, 0, sizeof(*device_pparams));
    device_pparams->Windowed = TRUE;
    device_pparams->SwapEffect = D3DSWAPEFFECT_DISCARD;
    device_pparams->BackBufferFormat = d3ddm.Format;

    hr = IDirect3D8_CreateDevice(d3d8, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, device_pparams, device);
    ok(SUCCEEDED(hr) || hr == D3DERR_NOTAVAILABLE || broken(hr == D3DERR_INVALIDCALL),
            "IDirect3D8_CreateDevice failed, hr %#x.\n", hr);

    return hr;
}

static void test_begin_end_state_block(IDirect3DDevice8 *device)
{
    DWORD state_block;
    HRESULT hr;

    /* Should succeed */
    hr = IDirect3DDevice8_BeginStateBlock(device);
    ok(SUCCEEDED(hr), "BeginStateBlock failed, hr %#x.\n", hr);
    if (FAILED(hr)) return;

    /* Calling BeginStateBlock while recording should return D3DERR_INVALIDCALL */
    hr = IDirect3DDevice8_BeginStateBlock(device);
    ok(hr == D3DERR_INVALIDCALL, "BeginStateBlock returned %#x, expected %#x.\n", hr, D3DERR_INVALIDCALL);
    if (hr != D3DERR_INVALIDCALL) return;

    /* Should succeed */
    state_block = 0xdeadbeef;
    hr = IDirect3DDevice8_EndStateBlock(device, &state_block);
    ok(SUCCEEDED(hr) && state_block && state_block != 0xdeadbeef,
            "EndStateBlock returned: hr %#x, state_block %#x. "
            "Expected hr %#x, state_block != 0, state_block != 0xdeadbeef.\n", hr, state_block, D3D_OK);
    IDirect3DDevice8_DeleteStateBlock(device, state_block);

    /* Calling EndStateBlock while not recording should return D3DERR_INVALIDCALL.
     * state_block should not be touched. */
    state_block = 0xdeadbeef;
    hr = IDirect3DDevice8_EndStateBlock(device, &state_block);
    ok(hr == D3DERR_INVALIDCALL && state_block == 0xdeadbeef,
            "EndStateBlock returned: hr %#x, state_block %#x. "
            "Expected hr %#x, state_block 0xdeadbeef.\n", hr, state_block, D3DERR_INVALIDCALL);
}

/* ============================ State Testing Framework ========================== */

struct state_test
{
    const char *test_name;

    /* The initial data is usually the same
     * as the default data, but a write can have side effects.
     * The initial data is tested first, before any writes take place
     * The default data can be tested after a write */
    const void *initial_data;

    /* The default data is the standard state to compare
     * against, and restore to */
    const void *default_data;

    /* The test data is the experiment data to try
     * in - what we want to write
     * out - what windows will actually write (not necessarily the same)  */
    const void *test_data_in;
    const void *test_data_out;

    /* Test resource management handlers */
    HRESULT (*setup_handler)(struct state_test *test);
    void (*teardown_handler)(struct state_test *test);

    /* Test data handlers */
    void (*set_handler)(IDirect3DDevice8 *device, const struct state_test *test, const void *data_in);
    void (*check_data)(IDirect3DDevice8 *device, unsigned int chain_stage,
            const struct state_test *test, const void *data_expected);

    /* Test arguments */
    const void *test_arg;

    /* Test-specific context data */
    void *test_context;
};

/* See below for explanation of the flags */
#define EVENT_OK    0
#define EVENT_ERROR -1

struct event_data
{
    DWORD stateblock;
    IDirect3DSurface8 *original_render_target;
    IDirect3DSwapChain8 *new_swap_chain;
};

enum stateblock_data
{
    SB_DATA_NONE = 0,
    SB_DATA_DEFAULT,
    SB_DATA_INITIAL,
    SB_DATA_TEST_IN,
    SB_DATA_TEST,
};

struct event
{
    int (*event_fn)(IDirect3DDevice8 *device, struct event_data *event_data);
    enum stateblock_data check;
    enum stateblock_data apply;
};

static const void *get_event_data(const struct state_test *test, enum stateblock_data data)
{
    switch (data)
    {
        case SB_DATA_DEFAULT:
            return test->default_data;

        case SB_DATA_INITIAL:
            return test->initial_data;

        case SB_DATA_TEST_IN:
            return test->test_data_in;

        case SB_DATA_TEST:
            return test->test_data_out;

        default:
            return NULL;
    }
}

/* This is an event-machine, which tests things.
 * It tests get and set operations for a batch of states, based on
 * results from the event function, which directs what's to be done */
static void execute_test_chain(IDirect3DDevice8 *device, struct state_test *test,
        unsigned int ntests, struct event *event, unsigned int nevents, struct event_data *event_data)
{
    unsigned int i, j;

    /* For each queued event */
    for (j = 0; j < nevents; ++j)
    {
        const void *data;

        /* Execute the next event handler (if available). */
        if (event[j].event_fn)
        {
            if (event[j].event_fn(device, event_data) == EVENT_ERROR)
            {
                trace("Stage %u in error state, aborting.\n", j);
                break;
            }
        }

        if (event[j].check != SB_DATA_NONE)
        {
            for (i = 0; i < ntests; ++i)
            {
                data = get_event_data(&test[i], event[j].check);
                test[i].check_data(device, j, &test[i], data);
            }
        }

        if (event[j].apply != SB_DATA_NONE)
        {
            for (i = 0; i < ntests; ++i)
            {
                data = get_event_data(&test[i], event[j].apply);
                test[i].set_handler(device, &test[i], data);
            }
        }
    }

     /* Attempt to reset any changes made */
    for (i=0; i < ntests; i++)
    {
        test[i].set_handler(device, &test[i], test[i].default_data);
    }
}

static int switch_render_target(IDirect3DDevice8* device, struct event_data *event_data)
{
    D3DPRESENT_PARAMETERS present_parameters;
    IDirect3DSwapChain8 *swapchain = NULL;
    IDirect3DSurface8 *backbuffer = NULL;
    D3DDISPLAYMODE d3ddm;
    HRESULT hr;

    /* Parameters for new swapchain */
    IDirect3DDevice8_GetDisplayMode(device, &d3ddm);
    memset(&present_parameters, 0, sizeof(present_parameters));
    present_parameters.Windowed = TRUE;
    present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present_parameters.BackBufferFormat = d3ddm.Format;

    /* Create new swapchain */
    hr = IDirect3DDevice8_CreateAdditionalSwapChain(device, &present_parameters, &swapchain);
    ok(SUCCEEDED(hr), "CreateAdditionalSwapChain returned %#x.\n", hr);
    if (FAILED(hr)) goto error;

    /* Get its backbuffer */
    hr = IDirect3DSwapChain8_GetBackBuffer(swapchain, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
    ok(SUCCEEDED(hr), "GetBackBuffer returned %#x.\n", hr);
    if (FAILED(hr)) goto error;

    /* Save the current render target */
    hr = IDirect3DDevice8_GetRenderTarget(device, &event_data->original_render_target);
    ok(SUCCEEDED(hr), "GetRenderTarget returned %#x.\n", hr);
    if (FAILED(hr)) goto error;

    /* Set the new swapchain's backbuffer as a render target */
    hr = IDirect3DDevice8_SetRenderTarget(device, backbuffer, NULL);
    ok(SUCCEEDED(hr), "SetRenderTarget returned %#x.\n", hr);
    if (FAILED(hr)) goto error;

    IDirect3DSurface8_Release(backbuffer);
    event_data->new_swap_chain = swapchain;
    return EVENT_OK;

error:
    if (backbuffer) IDirect3DSurface8_Release(backbuffer);
    if (swapchain) IDirect3DSwapChain8_Release(swapchain);
    return EVENT_ERROR;
}

static int revert_render_target(IDirect3DDevice8 *device, struct event_data *event_data)
{
    HRESULT hr;

    /* Reset the old render target */
    hr = IDirect3DDevice8_SetRenderTarget(device, event_data->original_render_target, NULL);
    ok(SUCCEEDED(hr), "SetRenderTarget returned %#x.\n", hr);
    if (FAILED(hr))
    {
        IDirect3DSurface8_Release(event_data->original_render_target);
        return EVENT_ERROR;
    }

    IDirect3DSurface8_Release(event_data->original_render_target);
    IDirect3DSwapChain8_Release(event_data->new_swap_chain);
    return EVENT_OK;
}

static int begin_stateblock(IDirect3DDevice8 *device, struct event_data *event_data)
{
    HRESULT hr;

    hr = IDirect3DDevice8_BeginStateBlock(device);
    ok(SUCCEEDED(hr), "BeginStateBlock returned %#x.\n", hr);
    if (FAILED(hr)) return EVENT_ERROR;
    return EVENT_OK;
}

static int end_stateblock(IDirect3DDevice8 *device, struct event_data *event_data)
{
    HRESULT hr;

    hr = IDirect3DDevice8_EndStateBlock(device, &event_data->stateblock);
    ok(SUCCEEDED(hr), "EndStateBlock returned %#x.\n", hr);
    if (FAILED(hr)) return EVENT_ERROR;
    return EVENT_OK;
}

static int abort_stateblock(IDirect3DDevice8 *device, struct event_data *event_data)
{
    IDirect3DDevice8_DeleteStateBlock(device, event_data->stateblock);
    return EVENT_OK;
}

static int apply_stateblock(IDirect3DDevice8 *device, struct event_data *event_data)
{
    HRESULT hr;

    hr = IDirect3DDevice8_ApplyStateBlock(device, event_data->stateblock);
    ok(SUCCEEDED(hr), "Apply returned %#x.\n", hr);

    IDirect3DDevice8_DeleteStateBlock(device, event_data->stateblock);
    if (FAILED(hr)) return EVENT_ERROR;
    return EVENT_OK;
}

static int capture_stateblock(IDirect3DDevice8 *device, struct event_data *event_data)
{
    HRESULT hr;

    hr = IDirect3DDevice8_CaptureStateBlock(device, event_data->stateblock);
    ok(SUCCEEDED(hr), "Capture returned %#x.\n", hr);
    if (FAILED(hr)) return EVENT_ERROR;

    return EVENT_OK;
}

static void execute_test_chain_all(IDirect3DDevice8 *device, struct state_test *test, unsigned int ntests)
{
    struct event_data arg;
    unsigned int i;
    HRESULT hr;

    struct event read_events[] =
    {
        {NULL,                      SB_DATA_INITIAL,        SB_DATA_NONE},
    };

    struct event write_read_events[] =
    {
        {NULL,                      SB_DATA_NONE,           SB_DATA_TEST_IN},
        {NULL,                      SB_DATA_TEST,           SB_DATA_NONE},
    };

    struct event abort_stateblock_events[] =
    {
        {begin_stateblock,          SB_DATA_NONE,           SB_DATA_TEST_IN},
        {end_stateblock,            SB_DATA_NONE,           SB_DATA_NONE},
        {abort_stateblock,          SB_DATA_DEFAULT,        SB_DATA_NONE},
    };

    struct event apply_stateblock_events[] =
    {
        {begin_stateblock,          SB_DATA_NONE,           SB_DATA_TEST_IN},
        {end_stateblock,            SB_DATA_NONE,           SB_DATA_NONE},
        {apply_stateblock,          SB_DATA_TEST,           SB_DATA_NONE},
    };

    struct event capture_reapply_stateblock_events[] =
    {
        {begin_stateblock,          SB_DATA_NONE,           SB_DATA_TEST_IN},
        {end_stateblock,            SB_DATA_NONE,           SB_DATA_NONE},
        {capture_stateblock,        SB_DATA_DEFAULT,        SB_DATA_TEST_IN},
        {apply_stateblock,          SB_DATA_DEFAULT,        SB_DATA_NONE},
    };

    struct event rendertarget_switch_events[] =
    {
        {NULL,                      SB_DATA_NONE,           SB_DATA_TEST_IN},
        {switch_render_target,      SB_DATA_TEST,           SB_DATA_NONE},
        {revert_render_target,      SB_DATA_NONE,           SB_DATA_NONE},
    };

    struct event rendertarget_stateblock_events[] =
    {
        {begin_stateblock,          SB_DATA_NONE,           SB_DATA_TEST_IN},
        {switch_render_target,      SB_DATA_DEFAULT,        SB_DATA_NONE},
        {end_stateblock,            SB_DATA_NONE,           SB_DATA_NONE},
        {revert_render_target,      SB_DATA_NONE,           SB_DATA_NONE},
        {apply_stateblock,          SB_DATA_TEST,           SB_DATA_NONE},
    };

    /* Setup each test for execution */
    for (i = 0; i < ntests; ++i)
    {
        hr = test[i].setup_handler(&test[i]);
        ok(SUCCEEDED(hr), "Test \"%s\" failed setup, aborting\n", test[i].test_name);
        if (FAILED(hr)) return;
    }

    trace("Running initial read state tests\n");
    execute_test_chain(device, test, ntests, read_events, 1, NULL);

    trace("Running write-read state tests\n");
    execute_test_chain(device, test, ntests, write_read_events, 2, NULL);

    trace("Running stateblock abort state tests\n");
    execute_test_chain(device, test, ntests, abort_stateblock_events, 3, &arg);

    trace("Running stateblock apply state tests\n");
    execute_test_chain(device, test, ntests, apply_stateblock_events, 3, &arg);

    trace("Running stateblock capture/reapply state tests\n");
    execute_test_chain(device, test, ntests, capture_reapply_stateblock_events, 4, &arg);

    trace("Running rendertarget switch state tests\n");
    execute_test_chain(device, test, ntests, rendertarget_switch_events, 3, &arg);

    trace("Running stateblock apply over rendertarget switch interrupt tests\n");
    execute_test_chain(device, test, ntests, rendertarget_stateblock_events, 5, &arg);

    /* Cleanup resources */
    for (i = 0; i < ntests; ++i)
    {
        test[i].teardown_handler(&test[i]);
    }
}

/* =================== State test: Pixel and Vertex Shader constants ============ */

struct shader_constant_data
{
    float float_constant[4]; /* 1x4 float constant */
};

struct shader_constant_arg
{
    unsigned int idx;
    BOOL pshader;
};

static const struct shader_constant_data shader_constant_poison_data =
{
    {1.0f, 2.0f, 3.0f, 4.0f},
};

static const struct shader_constant_data shader_constant_default_data =
{
    {0.0f, 0.0f, 0.0f, 0.0f},
};

static const struct shader_constant_data shader_constant_test_data =
{
    {5.0f, 6.0f, 7.0f, 8.0f},
};

static void shader_constant_set_handler(IDirect3DDevice8* device, const struct state_test *test, const void *data)
{
    const struct shader_constant_data *scdata = data;
    const struct shader_constant_arg *scarg = test->test_arg;
    unsigned int index = scarg->idx;
    HRESULT hr;

    if (!scarg->pshader)
    {
        hr = IDirect3DDevice8_SetVertexShaderConstant(device, index, scdata->float_constant, 1);
        ok(SUCCEEDED(hr), "SetVertexShaderConstant returned %#x.\n", hr);
    }
    else
    {
        hr = IDirect3DDevice8_SetPixelShaderConstant(device, index, scdata->float_constant, 1);
        ok(SUCCEEDED(hr), "SetPixelShaderConstant returned %#x.\n", hr);
    }
}

static void shader_constant_check_data(IDirect3DDevice8 *device, unsigned int chain_stage,
        const struct state_test *test, const void *expected_data)
{
    const struct shader_constant_data *scdata = expected_data;
    const struct shader_constant_arg *scarg = test->test_arg;
    struct shader_constant_data value;
    HRESULT hr;

    value = shader_constant_poison_data;

    if (!scarg->pshader)
    {
        hr = IDirect3DDevice8_GetVertexShaderConstant(device, scarg->idx, value.float_constant, 1);
        ok(SUCCEEDED(hr), "GetVertexShaderConstant returned %#x.\n", hr);
    }
    else
    {
        hr = IDirect3DDevice8_GetPixelShaderConstant(device, scarg->idx, value.float_constant, 1);
        ok(SUCCEEDED(hr), "GetPixelShaderConstant returned %#x.\n", hr);
    }

    ok(!memcmp(value.float_constant, scdata->float_constant, sizeof(scdata->float_constant)),
            "Chain stage %u, %s constant:\n\t{%.8e, %.8e, %.8e, %.8e} expected\n\t{%.8e, %.8e, %.8e, %.8e} received\n",
            chain_stage, scarg->pshader ? "pixel shader" : "vertex shader",
            scdata->float_constant[0], scdata->float_constant[1],
            scdata->float_constant[2], scdata->float_constant[3],
            value.float_constant[0], value.float_constant[1],
            value.float_constant[2], value.float_constant[3]);
}

static HRESULT shader_constant_setup_handler(struct state_test *test)
{
    test->test_context = NULL;
    test->test_data_in = &shader_constant_test_data;
    test->test_data_out = &shader_constant_test_data;
    test->default_data = &shader_constant_default_data;
    test->initial_data = &shader_constant_default_data;

    return D3D_OK;
}

static void shader_constant_teardown_handler(struct state_test *test)
{
}

static void shader_constants_queue_test(struct state_test *test, const struct shader_constant_arg *test_arg)
{
    test->setup_handler = shader_constant_setup_handler;
    test->teardown_handler = shader_constant_teardown_handler;
    test->set_handler = shader_constant_set_handler;
    test->check_data = shader_constant_check_data;
    test->test_name = test_arg->pshader ? "set_get_pshader_constants" : "set_get_vshader_constants";
    test->test_arg = test_arg;
}

/* =================== State test: Lights ===================================== */

struct light_data
{
    D3DLIGHT8 light;
    BOOL enabled;
    HRESULT get_light_result;
    HRESULT get_enabled_result;
};

struct light_arg
{
    unsigned int idx;
};

static const struct light_data light_poison_data =
{
    {
        0x1337c0de,
        {7.0, 4.0, 2.0, 1.0},
        {7.0, 4.0, 2.0, 1.0},
        {7.0, 4.0, 2.0, 1.0},
        {3.3f, 4.4f, 5.5f},
        {6.6f, 7.7f, 8.8f},
        12.12f, 13.13f, 14.14f, 15.15f, 16.16f, 17.17f, 18.18f,
    },
    TRUE,
    0x1337c0de,
    0x1337c0de,
};

static const struct light_data light_default_data =
{
    {
        D3DLIGHT_DIRECTIONAL,
        {1.0, 1.0, 1.0, 0.0},
        {0.0, 0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
        {0.0, 0.0, 1.0},
        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
    },
    FALSE,
    D3D_OK,
    D3D_OK,
};

/* This is used for the initial read state (before a write causes side effects)
 * The proper return status is D3DERR_INVALIDCALL */
static const struct light_data light_initial_data =
{
    {
        0x1337c0de,
        {7.0, 4.0, 2.0, 1.0},
        {7.0, 4.0, 2.0, 1.0},
        {7.0, 4.0, 2.0, 1.0},
        {3.3f, 4.4f, 5.5f},
        {6.6f, 7.7f, 8.8f},
        12.12f, 13.13f, 14.14f, 15.15f, 16.16f, 17.17f, 18.18f,
    },
    TRUE,
    D3DERR_INVALIDCALL,
    D3DERR_INVALIDCALL,
};

static const struct light_data light_test_data_in =
{
    {
        1,
        {2.0, 2.0, 2.0, 2.0},
        {3.0, 3.0, 3.0, 3.0},
        {4.0, 4.0, 4.0, 4.0},
        {5.0, 5.0, 5.0},
        {6.0, 6.0, 6.0},
        7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0,
    },
    TRUE,
    D3D_OK,
    D3D_OK,
};

/* SetLight will use 128 as the "enabled" value */
static const struct light_data light_test_data_out =
{
    {
        1,
        {2.0, 2.0, 2.0, 2.0},
        {3.0, 3.0, 3.0, 3.0},
        {4.0, 4.0, 4.0, 4.0},
        {5.0, 5.0, 5.0},
        {6.0, 6.0, 6.0},
        7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0,
    },
    128,
    D3D_OK,
    D3D_OK,
};

static void light_set_handler(IDirect3DDevice8 *device, const struct state_test *test, const void *data)
{
    const struct light_data *ldata = data;
    const struct light_arg *larg = test->test_arg;
    unsigned int index = larg->idx;
    HRESULT hr;

    hr = IDirect3DDevice8_SetLight(device, index, &ldata->light);
    ok(SUCCEEDED(hr), "SetLight returned %#x.\n", hr);

    hr = IDirect3DDevice8_LightEnable(device, index, ldata->enabled);
    ok(SUCCEEDED(hr), "SetLightEnable returned %#x.\n", hr);
}

static void light_check_data(IDirect3DDevice8 *device, unsigned int chain_stage,
        const struct state_test *test, const void *expected_data)
{
    const struct light_arg *larg = test->test_arg;
    const struct light_data *ldata = expected_data;
    struct light_data value;

    value = light_poison_data;

    value.get_enabled_result = IDirect3DDevice8_GetLightEnable(device, larg->idx, &value.enabled);
    value.get_light_result = IDirect3DDevice8_GetLight(device, larg->idx, &value.light);

    ok(value.get_enabled_result == ldata->get_enabled_result,
            "Chain stage %u: expected get_enabled_result %#x, got %#x.\n",
            chain_stage, ldata->get_enabled_result, value.get_enabled_result);
    ok(value.get_light_result == ldata->get_light_result,
            "Chain stage %u: expected get_light_result %#x, got %#x.\n",
            chain_stage, ldata->get_light_result, value.get_light_result);

    ok(value.enabled == ldata->enabled,
            "Chain stage %u: expected enabled %#x, got %#x.\n",
            chain_stage, ldata->enabled, value.enabled);
    ok(value.light.Type == ldata->light.Type,
            "Chain stage %u: expected light.Type %#x, got %#x.\n",
            chain_stage, ldata->light.Type, value.light.Type);
    ok(!memcmp(&value.light.Diffuse, &ldata->light.Diffuse, sizeof(value.light.Diffuse)),
            "Chain stage %u, light.Diffuse:\n\t{%.8e, %.8e, %.8e, %.8e} expected\n"
            "\t{%.8e, %.8e, %.8e, %.8e} received.\n", chain_stage,
            ldata->light.Diffuse.r, ldata->light.Diffuse.g,
            ldata->light.Diffuse.b, ldata->light.Diffuse.a,
            value.light.Diffuse.r, value.light.Diffuse.g,
            value.light.Diffuse.b, value.light.Diffuse.a);
    ok(!memcmp(&value.light.Specular, &ldata->light.Specular, sizeof(value.light.Specular)),
            "Chain stage %u, light.Specular:\n\t{%.8e, %.8e, %.8e, %.8e} expected\n"
            "\t{%.8e, %.8e, %.8e, %.8e} received.\n", chain_stage,
            ldata->light.Specular.r, ldata->light.Specular.g,
            ldata->light.Specular.b, ldata->light.Specular.a,
            value.light.Specular.r, value.light.Specular.g,
            value.light.Specular.b, value.light.Specular.a);
    ok(!memcmp(&value.light.Ambient, &ldata->light.Ambient, sizeof(value.light.Ambient)),
            "Chain stage %u, light.Ambient:\n\t{%.8e, %.8e, %.8e, %.8e} expected\n"
            "\t{%.8e, %.8e, %.8e, %.8e} received.\n", chain_stage,
            ldata->light.Ambient.r, ldata->light.Ambient.g,
            ldata->light.Ambient.b, ldata->light.Ambient.a,
            value.light.Ambient.r, value.light.Ambient.g,
            value.light.Ambient.b, value.light.Ambient.a);
    ok(!memcmp(&value.light.Position, &ldata->light.Position, sizeof(value.light.Position)),
            "Chain stage %u, light.Position:\n\t{%.8e, %.8e, %.8e} expected\n\t{%.8e, %.8e, %.8e} received.\n",
            chain_stage, ldata->light.Position.x, ldata->light.Position.y, ldata->light.Position.z,
            value.light.Position.x, value.light.Position.y, value.light.Position.z);
    ok(!memcmp(&value.light.Direction, &ldata->light.Direction, sizeof(value.light.Direction)),
            "Chain stage %u, light.Direction:\n\t{%.8e, %.8e, %.8e} expected\n\t{%.8e, %.8e, %.8e} received.\n",
            chain_stage, ldata->light.Direction.x, ldata->light.Direction.y, ldata->light.Direction.z,
            value.light.Direction.x, value.light.Direction.y, value.light.Direction.z);
    ok(value.light.Range == ldata->light.Range,
            "Chain stage %u: expected light.Range %.8e, got %.8e.\n",
            chain_stage, ldata->light.Range, value.light.Range);
    ok(value.light.Falloff == ldata->light.Falloff,
            "Chain stage %u: expected light.Falloff %.8e, got %.8e.\n",
            chain_stage, ldata->light.Falloff, value.light.Falloff);
    ok(value.light.Attenuation0 == ldata->light.Attenuation0,
            "Chain stage %u: expected light.Attenuation0 %.8e, got %.8e.\n",
            chain_stage, ldata->light.Attenuation0, value.light.Attenuation0);
    ok(value.light.Attenuation1 == ldata->light.Attenuation1,
            "Chain stage %u: expected light.Attenuation1 %.8e, got %.8e.\n",
            chain_stage, ldata->light.Attenuation1, value.light.Attenuation1);
    ok(value.light.Attenuation2 == ldata->light.Attenuation2,
            "Chain stage %u: expected light.Attenuation2 %.8e, got %.8e.\n",
            chain_stage, ldata->light.Attenuation2, value.light.Attenuation2);
    ok(value.light.Theta == ldata->light.Theta,
            "Chain stage %u: expected light.Theta %.8e, got %.8e.\n",
            chain_stage, ldata->light.Theta, value.light.Theta);
    ok(value.light.Phi == ldata->light.Phi,
            "Chain stage %u: expected light.Phi %.8e, got %.8e.\n",
            chain_stage, ldata->light.Phi, value.light.Phi);
}

static HRESULT light_setup_handler(struct state_test *test)
{
    test->test_context = NULL;
    test->test_data_in = &light_test_data_in;
    test->test_data_out = &light_test_data_out;
    test->default_data = &light_default_data;
    test->initial_data = &light_initial_data;

    return D3D_OK;
}

static void light_teardown_handler(struct state_test *test)
{
}

static void lights_queue_test(struct state_test *test, const struct light_arg *test_arg)
{
    test->setup_handler = light_setup_handler;
    test->teardown_handler = light_teardown_handler;
    test->set_handler = light_set_handler;
    test->check_data = light_check_data;
    test->test_name = "set_get_light";
    test->test_arg = test_arg;
}

/* =================== State test: Transforms ===================================== */

struct transform_data
{
    D3DMATRIX view;
    D3DMATRIX projection;
    D3DMATRIX texture0;
    D3DMATRIX texture7;
    D3DMATRIX world0;
    D3DMATRIX world255;
};

static const struct transform_data transform_default_data =
{
    {{{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    }}},
    {{{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    }}},
    {{{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    }}},
    {{{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    }}},
    {{{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    }}},
    {{{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    }}},
};

static const struct transform_data transform_poison_data =
{
    {{{
         1.0f,  2.0f,  3.0f,  4.0f,
         5.0f,  6.0f,  7.0f,  8.0f,
         9.0f, 10.0f, 11.0f, 12.0f,
        13.0f, 14.0f, 15.0f, 16.0f,
    }}},
    {{{
        17.0f, 18.0f, 19.0f, 20.0f,
        21.0f, 22.0f, 23.0f, 24.0f,
        25.0f, 26.0f, 27.0f, 28.0f,
        29.0f, 30.0f, 31.0f, 32.0f,
    }}},
    {{{
        33.0f, 34.0f, 35.0f, 36.0f,
        37.0f, 38.0f, 39.0f, 40.0f,
        41.0f, 42.0f, 43.0f, 44.0f,
        45.0f, 46.0f, 47.0f, 48.0f,
    }}},
    {{{
        49.0f, 50.0f, 51.0f, 52.0f,
        53.0f, 54.0f, 55.0f, 56.0f,
        57.0f, 58.0f, 59.0f, 60.0f,
        61.0f, 62.0f, 63.0f, 64.0f,
    }}},
    {{{
        64.0f, 66.0f, 67.0f, 68.0f,
        69.0f, 70.0f, 71.0f, 72.0f,
        73.0f, 74.0f, 75.0f, 76.0f,
        77.0f, 78.0f, 79.0f, 80.0f,
    }}},
    {{{
        81.0f, 82.0f, 83.0f, 84.0f,
        85.0f, 86.0f, 87.0f, 88.0f,
        89.0f, 90.0f, 91.0f, 92.0f,
        93.0f, 94.0f, 95.0f, 96.0f,
    }}},
};

static const struct transform_data transform_test_data =
{
    {{{
          1.2f,     3.4f,  -5.6f,  7.2f,
        10.11f,  -12.13f, 14.15f, -1.5f,
        23.56f,   12.89f, 44.56f, -1.0f,
          2.3f,     0.0f,   4.4f,  5.5f,
    }}},
    {{{
          9.2f,    38.7f,  -6.6f,  7.2f,
        10.11f,  -12.13f, 77.15f, -1.5f,
        23.56f,   12.89f, 14.56f, -1.0f,
         12.3f,     0.0f,   4.4f,  5.5f,
    }}},
    {{{
         10.2f,     3.4f,   0.6f,  7.2f,
        10.11f,  -12.13f, 14.15f, -1.5f,
        23.54f,    12.9f, 44.56f, -1.0f,
          2.3f,     0.0f,   4.4f,  5.5f,
    }}},
    {{{
          1.2f,     3.4f,  -5.6f,  7.2f,
        10.11f,  -12.13f, -14.5f, -1.5f,
         2.56f,   12.89f, 23.56f, -1.0f,
        112.3f,     0.0f,   4.4f,  2.5f,
    }}},
    {{{
          1.2f,   31.41f,  58.6f,  7.2f,
        10.11f,  -12.13f, -14.5f, -1.5f,
         2.56f,   12.89f, 11.56f, -1.0f,
        112.3f,     0.0f,  44.4f,  2.5f,
    }}},

    {{{
         1.20f,     3.4f,  -5.6f,  7.0f,
        10.11f, -12.156f, -14.5f, -1.5f,
         2.56f,   1.829f,  23.6f, -1.0f,
        112.3f,     0.0f,  41.4f,  2.5f,
    }}},
};


static void transform_set_handler(IDirect3DDevice8 *device, const struct state_test *test, const void *data)
{
    const struct transform_data *tdata = data;
    HRESULT hr;

    hr = IDirect3DDevice8_SetTransform(device, D3DTS_VIEW, &tdata->view);
    ok(SUCCEEDED(hr), "SetTransform returned %#x.\n", hr);

    hr = IDirect3DDevice8_SetTransform(device, D3DTS_PROJECTION, &tdata->projection);
    ok(SUCCEEDED(hr), "SetTransform returned %#x.\n", hr);

    hr = IDirect3DDevice8_SetTransform(device, D3DTS_TEXTURE0, &tdata->texture0);
    ok(SUCCEEDED(hr), "SetTransform returned %#x.\n", hr);

    hr = IDirect3DDevice8_SetTransform(device, D3DTS_TEXTURE0 + texture_stages - 1, &tdata->texture7);
    ok(SUCCEEDED(hr), "SetTransform returned %#x.\n", hr);

    hr = IDirect3DDevice8_SetTransform(device, D3DTS_WORLD, &tdata->world0);
    ok(SUCCEEDED(hr), "SetTransform returned %#x.\n", hr);

    hr = IDirect3DDevice8_SetTransform(device, D3DTS_WORLDMATRIX(255), &tdata->world255);
    ok(SUCCEEDED(hr), "SetTransform returned %#x.\n", hr);
}

static void compare_matrix(const char *name, unsigned int chain_stage,
        const D3DMATRIX *received, const D3DMATRIX *expected)
{
    ok(!memcmp(expected, received, sizeof(*expected)),
            "Chain stage %u, matrix %s:\n"
            "\t{\n"
            "\t\t%.8e, %.8e, %.8e, %.8e,\n"
            "\t\t%.8e, %.8e, %.8e, %.8e,\n"
            "\t\t%.8e, %.8e, %.8e, %.8e,\n"
            "\t\t%.8e, %.8e, %.8e, %.8e,\n"
            "\t} expected\n"
            "\t{\n"
            "\t\t%.8e, %.8e, %.8e, %.8e,\n"
            "\t\t%.8e, %.8e, %.8e, %.8e,\n"
            "\t\t%.8e, %.8e, %.8e, %.8e,\n"
            "\t\t%.8e, %.8e, %.8e, %.8e,\n"
            "\t} received\n",
            chain_stage, name,
            U(*expected).m[0][0], U(*expected).m[1][0], U(*expected).m[2][0], U(*expected).m[3][0],
            U(*expected).m[0][1], U(*expected).m[1][1], U(*expected).m[2][1], U(*expected).m[3][1],
            U(*expected).m[0][2], U(*expected).m[1][2], U(*expected).m[2][2], U(*expected).m[3][2],
            U(*expected).m[0][3], U(*expected).m[1][3], U(*expected).m[2][3], U(*expected).m[3][3],
            U(*received).m[0][0], U(*received).m[1][0], U(*received).m[2][0], U(*received).m[3][0],
            U(*received).m[0][1], U(*received).m[1][1], U(*received).m[2][1], U(*received).m[3][1],
            U(*received).m[0][2], U(*received).m[1][2], U(*received).m[2][2], U(*received).m[3][2],
            U(*received).m[0][3], U(*received).m[1][3], U(*received).m[2][3], U(*received).m[3][3]);
}

static void transform_check_data(IDirect3DDevice8 *device, unsigned int chain_stage,
        const struct state_test *test, const void *expected_data)
{
    const struct transform_data *tdata = expected_data;
    D3DMATRIX value;
    HRESULT hr;

    value = transform_poison_data.view;
    hr = IDirect3DDevice8_GetTransform(device, D3DTS_VIEW, &value);
    ok(SUCCEEDED(hr), "GetTransform returned %#x.\n", hr);
    compare_matrix("View", chain_stage, &value, &tdata->view);

    value = transform_poison_data.projection;
    hr = IDirect3DDevice8_GetTransform(device, D3DTS_PROJECTION, &value);
    ok(SUCCEEDED(hr), "GetTransform returned %#x.\n", hr);
    compare_matrix("Projection", chain_stage, &value, &tdata->projection);

    value = transform_poison_data.texture0;
    hr = IDirect3DDevice8_GetTransform(device, D3DTS_TEXTURE0, &value);
    ok(SUCCEEDED(hr), "GetTransform returned %#x.\n", hr);
    compare_matrix("Texture0", chain_stage, &value, &tdata->texture0);

    value = transform_poison_data.texture7;
    hr = IDirect3DDevice8_GetTransform(device, D3DTS_TEXTURE0 + texture_stages - 1, &value);
    ok(SUCCEEDED(hr), "GetTransform returned %#x.\n", hr);
    compare_matrix("Texture7", chain_stage, &value, &tdata->texture7);

    value = transform_poison_data.world0;
    hr = IDirect3DDevice8_GetTransform(device, D3DTS_WORLD, &value);
    ok(SUCCEEDED(hr), "GetTransform returned %#x.\n", hr);
    compare_matrix("World0", chain_stage, &value, &tdata->world0);

    value = transform_poison_data.world255;
    hr = IDirect3DDevice8_GetTransform(device, D3DTS_WORLDMATRIX(255), &value);
    ok(SUCCEEDED(hr), "GetTransform returned %#x.\n", hr);
    compare_matrix("World255", chain_stage, &value, &tdata->world255);
}

static HRESULT transform_setup_handler(struct state_test *test)
{
    test->test_context = NULL;
    test->test_data_in = &transform_test_data;
    test->test_data_out = &transform_test_data;
    test->default_data = &transform_default_data;
    test->initial_data = &transform_default_data;

    return D3D_OK;
}

static void transform_teardown_handler(struct state_test *test)
{
}

static void transform_queue_test(struct state_test *test)
{
    test->setup_handler = transform_setup_handler;
    test->teardown_handler = transform_teardown_handler;
    test->set_handler = transform_set_handler;
    test->check_data = transform_check_data;
    test->test_name = "set_get_transforms";
    test->test_arg = NULL;
}

/* =================== State test: Render States ===================================== */

const D3DRENDERSTATETYPE render_state_indices[] =
{
    D3DRS_ZENABLE,
    D3DRS_FILLMODE,
    D3DRS_SHADEMODE,
    D3DRS_ZWRITEENABLE,
    D3DRS_ALPHATESTENABLE,
    D3DRS_LASTPIXEL,
    D3DRS_SRCBLEND,
    D3DRS_DESTBLEND,
    D3DRS_CULLMODE,
    D3DRS_ZFUNC,
    D3DRS_ALPHAREF,
    D3DRS_ALPHAFUNC,
    D3DRS_DITHERENABLE,
    D3DRS_ALPHABLENDENABLE,
    D3DRS_FOGENABLE,
    D3DRS_SPECULARENABLE,
    D3DRS_FOGCOLOR,
    D3DRS_FOGTABLEMODE,
    D3DRS_FOGSTART,
    D3DRS_FOGEND,
    D3DRS_FOGDENSITY,
    D3DRS_RANGEFOGENABLE,
    D3DRS_STENCILENABLE,
    D3DRS_STENCILFAIL,
    D3DRS_STENCILZFAIL,
    D3DRS_STENCILPASS,
    D3DRS_STENCILFUNC,
    D3DRS_STENCILREF,
    D3DRS_STENCILMASK,
    D3DRS_STENCILWRITEMASK,
    D3DRS_TEXTUREFACTOR,
    D3DRS_WRAP0,
    D3DRS_WRAP1,
    D3DRS_WRAP2,
    D3DRS_WRAP3,
    D3DRS_WRAP4,
    D3DRS_WRAP5,
    D3DRS_WRAP6,
    D3DRS_WRAP7,
    D3DRS_CLIPPING,
    D3DRS_LIGHTING,
    D3DRS_AMBIENT,
    D3DRS_FOGVERTEXMODE,
    D3DRS_COLORVERTEX,
    D3DRS_LOCALVIEWER,
    D3DRS_NORMALIZENORMALS,
    D3DRS_DIFFUSEMATERIALSOURCE,
    D3DRS_SPECULARMATERIALSOURCE,
    D3DRS_AMBIENTMATERIALSOURCE,
    D3DRS_EMISSIVEMATERIALSOURCE,
    D3DRS_VERTEXBLEND,
    D3DRS_CLIPPLANEENABLE,
#if 0 /* Driver dependent, increase array size to enable */
    D3DRS_POINTSIZE,
#endif
    D3DRS_POINTSIZE_MIN,
    D3DRS_POINTSPRITEENABLE,
    D3DRS_POINTSCALEENABLE,
    D3DRS_POINTSCALE_A,
    D3DRS_POINTSCALE_B,
    D3DRS_POINTSCALE_C,
    D3DRS_MULTISAMPLEANTIALIAS,
    D3DRS_MULTISAMPLEMASK,
    D3DRS_PATCHEDGESTYLE,
    D3DRS_DEBUGMONITORTOKEN,
    D3DRS_POINTSIZE_MAX,
    D3DRS_INDEXEDVERTEXBLENDENABLE,
    D3DRS_COLORWRITEENABLE,
    D3DRS_TWEENFACTOR,
    D3DRS_BLENDOP,
};

struct render_state_data
{
    DWORD states[sizeof(render_state_indices) / sizeof(*render_state_indices)];
};

struct render_state_arg
{
    D3DPRESENT_PARAMETERS *device_pparams;
    float pointsize_max;
};

struct render_state_context
{
    struct render_state_data default_data_buffer;
    struct render_state_data test_data_buffer;
    struct render_state_data poison_data_buffer;
};

static void render_state_set_handler(IDirect3DDevice8 *device, const struct state_test *test, const void *data)
{
    const struct render_state_data *rsdata = data;
    unsigned int i;
    HRESULT hr;

    for (i = 0; i < sizeof(render_state_indices) / sizeof(*render_state_indices); ++i)
    {
        hr = IDirect3DDevice8_SetRenderState(device, render_state_indices[i], rsdata->states[i]);
        ok(SUCCEEDED(hr), "SetRenderState returned %#x.\n", hr);
    }
}

static void render_state_check_data(IDirect3DDevice8 *device, unsigned int chain_stage,
        const struct state_test *test, const void *expected_data)
{
    const struct render_state_context *ctx = test->test_context;
    const struct render_state_data *rsdata = expected_data;
    unsigned int i;
    HRESULT hr;

    for (i = 0; i < sizeof(render_state_indices) / sizeof(*render_state_indices); ++i)
    {
        DWORD value = ctx->poison_data_buffer.states[i];
        hr = IDirect3DDevice8_GetRenderState(device, render_state_indices[i], &value);
        ok(SUCCEEDED(hr), "GetRenderState returned %#x.\n", hr);
        ok(value == rsdata->states[i], "Chain stage %u, render state %#x: expected %#x, got %#x.\n",
                chain_stage, render_state_indices[i], rsdata->states[i], value);
    }
}

static inline DWORD to_dword(float fl)
{
    union {float f; DWORD d;} ret;

    ret.f = fl;
    return ret.d;
}

static void render_state_default_data_init(const struct render_state_arg *rsarg, struct render_state_data *data)
{
    DWORD zenable = rsarg->device_pparams->EnableAutoDepthStencil ? D3DZB_TRUE : D3DZB_FALSE;
    unsigned int idx = 0;

    data->states[idx++] = zenable;               /* ZENABLE */
    data->states[idx++] = D3DFILL_SOLID;         /* FILLMODE */
    data->states[idx++] = D3DSHADE_GOURAUD;      /* SHADEMODE */
    data->states[idx++] = TRUE;                  /* ZWRITEENABLE */
    data->states[idx++] = FALSE;                 /* ALPHATESTENABLE */
    data->states[idx++] = TRUE;                  /* LASTPIXEL */
    data->states[idx++] = D3DBLEND_ONE;          /* SRCBLEND */
    data->states[idx++] = D3DBLEND_ZERO;         /* DESTBLEND */
    data->states[idx++] = D3DCULL_CCW;           /* CULLMODE */
    data->states[idx++] = D3DCMP_LESSEQUAL;      /* ZFUNC */
    data->states[idx++] = 0;                     /* ALPHAREF */
    data->states[idx++] = D3DCMP_ALWAYS;         /* ALPHAFUNC */
    data->states[idx++] = FALSE;                 /* DITHERENABLE */
    data->states[idx++] = FALSE;                 /* ALPHABLENDENABLE */
    data->states[idx++] = FALSE;                 /* FOGENABLE */
    data->states[idx++] = FALSE;                 /* SPECULARENABLE */
    data->states[idx++] = 0;                     /* FOGCOLOR */
    data->states[idx++] = D3DFOG_NONE;           /* FOGTABLEMODE */
    data->states[idx++] = to_dword(0.0f);        /* FOGSTART */
    data->states[idx++] = to_dword(1.0f);        /* FOGEND */
    data->states[idx++] = to_dword(1.0f);        /* FOGDENSITY */
    data->states[idx++] = FALSE;                 /* RANGEFOGENABLE */
    data->states[idx++] = FALSE;                 /* STENCILENABLE */
    data->states[idx++] = D3DSTENCILOP_KEEP;     /* STENCILFAIL */
    data->states[idx++] = D3DSTENCILOP_KEEP;     /* STENCILZFAIL */
    data->states[idx++] = D3DSTENCILOP_KEEP;     /* STENCILPASS */
    data->states[idx++] = D3DCMP_ALWAYS;         /* STENCILFUNC */
    data->states[idx++] = 0;                     /* STENCILREF */
    data->states[idx++] = 0xFFFFFFFF;            /* STENCILMASK */
    data->states[idx++] = 0xFFFFFFFF;            /* STENCILWRITEMASK */
    data->states[idx++] = 0xFFFFFFFF;            /* TEXTUREFACTOR */
    data->states[idx++] = 0;                     /* WRAP 0 */
    data->states[idx++] = 0;                     /* WRAP 1 */
    data->states[idx++] = 0;                     /* WRAP 2 */
    data->states[idx++] = 0;                     /* WRAP 3 */
    data->states[idx++] = 0;                     /* WRAP 4 */
    data->states[idx++] = 0;                     /* WRAP 5 */
    data->states[idx++] = 0;                     /* WRAP 6 */
    data->states[idx++] = 0;                     /* WRAP 7 */
    data->states[idx++] = TRUE;                  /* CLIPPING */
    data->states[idx++] = TRUE;                  /* LIGHTING */
    data->states[idx++] = 0;                     /* AMBIENT */
    data->states[idx++] = D3DFOG_NONE;           /* FOGVERTEXMODE */
    data->states[idx++] = TRUE;                  /* COLORVERTEX */
    data->states[idx++] = TRUE;                  /* LOCALVIEWER */
    data->states[idx++] = FALSE;                 /* NORMALIZENORMALS */
    data->states[idx++] = D3DMCS_COLOR1;         /* DIFFUSEMATERIALSOURCE */
    data->states[idx++] = D3DMCS_COLOR2;         /* SPECULARMATERIALSOURCE */
    data->states[idx++] = D3DMCS_MATERIAL;       /* AMBIENTMATERIALSOURCE */
    data->states[idx++] = D3DMCS_MATERIAL;       /* EMISSIVEMATERIALSOURCE */
    data->states[idx++] = D3DVBF_DISABLE;        /* VERTEXBLEND */
    data->states[idx++] = 0;                     /* CLIPPLANEENABLE */
    if (0) data->states[idx++] = to_dword(1.0f); /* POINTSIZE, driver dependent, increase array size to enable */
    data->states[idx++] = to_dword(0.0f);        /* POINTSIZEMIN */
    data->states[idx++] = FALSE;                 /* POINTSPRITEENABLE */
    data->states[idx++] = FALSE;                 /* POINTSCALEENABLE */
    data->states[idx++] = to_dword(1.0f);        /* POINTSCALE_A */
    data->states[idx++] = to_dword(0.0f);        /* POINTSCALE_B */
    data->states[idx++] = to_dword(0.0f);        /* POINTSCALE_C */
    data->states[idx++] = TRUE;                  /* MULTISAMPLEANTIALIAS */
    data->states[idx++] = 0xFFFFFFFF;            /* MULTISAMPLEMASK */
    data->states[idx++] = D3DPATCHEDGE_DISCRETE; /* PATCHEDGESTYLE */
    data->states[idx++] = 0xbaadcafe;            /* DEBUGMONITORTOKEN */
    data->states[idx++] = to_dword(rsarg->pointsize_max); /* POINTSIZE_MAX */
    data->states[idx++] = FALSE;                 /* INDEXEDVERTEXBLENDENABLE */
    data->states[idx++] = 0x0000000F;            /* COLORWRITEENABLE */
    data->states[idx++] = to_dword(0.0f);        /* TWEENFACTOR */
    data->states[idx++] = D3DBLENDOP_ADD;        /* BLENDOP */
}

static void render_state_poison_data_init(struct render_state_data *data)
{
    unsigned int i;

    for (i = 0; i < sizeof(render_state_indices) / sizeof(*render_state_indices); ++i)
    {
        data->states[i] = 0x1337c0de;
    }
}

static void render_state_test_data_init(struct render_state_data *data)
{
    unsigned int idx = 0;

    data->states[idx++] = D3DZB_USEW;            /* ZENABLE */
    data->states[idx++] = D3DFILL_WIREFRAME;     /* FILLMODE */
    data->states[idx++] = D3DSHADE_PHONG;        /* SHADEMODE */
    data->states[idx++] = FALSE;                 /* ZWRITEENABLE */
    data->states[idx++] = TRUE;                  /* ALPHATESTENABLE */
    data->states[idx++] = FALSE;                 /* LASTPIXEL */
    data->states[idx++] = D3DBLEND_SRCALPHASAT;  /* SRCBLEND */
    data->states[idx++] = D3DBLEND_INVDESTALPHA; /* DESTBLEND */
    data->states[idx++] = D3DCULL_CW;            /* CULLMODE */
    data->states[idx++] = D3DCMP_NOTEQUAL;       /* ZFUNC */
    data->states[idx++] = 10;                    /* ALPHAREF */
    data->states[idx++] = D3DCMP_GREATER;        /* ALPHAFUNC */
    data->states[idx++] = TRUE;                  /* DITHERENABLE */
    data->states[idx++] = TRUE;                  /* ALPHABLENDENABLE */
    data->states[idx++] = TRUE;                  /* FOGENABLE */
    data->states[idx++] = TRUE;                  /* SPECULARENABLE */
    data->states[idx++] = 255 << 31;             /* FOGCOLOR */
    data->states[idx++] = D3DFOG_EXP;            /* FOGTABLEMODE */
    data->states[idx++] = to_dword(0.1f);        /* FOGSTART */
    data->states[idx++] = to_dword(0.8f);        /* FOGEND */
    data->states[idx++] = to_dword(0.5f);        /* FOGDENSITY */
    data->states[idx++] = TRUE;                  /* RANGEFOGENABLE */
    data->states[idx++] = TRUE;                  /* STENCILENABLE */
    data->states[idx++] = D3DSTENCILOP_INCRSAT;  /* STENCILFAIL */
    data->states[idx++] = D3DSTENCILOP_REPLACE;  /* STENCILZFAIL */
    data->states[idx++] = D3DSTENCILOP_INVERT;   /* STENCILPASS */
    data->states[idx++] = D3DCMP_LESS;           /* STENCILFUNC */
    data->states[idx++] = 10;                    /* STENCILREF */
    data->states[idx++] = 0xFF00FF00;            /* STENCILMASK */
    data->states[idx++] = 0x00FF00FF;            /* STENCILWRITEMASK */
    data->states[idx++] = 0xF0F0F0F0;            /* TEXTUREFACTOR */
    data->states[idx++] = D3DWRAPCOORD_0 | D3DWRAPCOORD_2;                                   /* WRAP 0 */
    data->states[idx++] = D3DWRAPCOORD_1 | D3DWRAPCOORD_3;                                   /* WRAP 1 */
    data->states[idx++] = D3DWRAPCOORD_2 | D3DWRAPCOORD_3;                                   /* WRAP 2 */
    data->states[idx++] = D3DWRAPCOORD_3 | D3DWRAPCOORD_0;                                   /* WRAP 4 */
    data->states[idx++] = D3DWRAPCOORD_0 | D3DWRAPCOORD_1 | D3DWRAPCOORD_2;                  /* WRAP 5 */
    data->states[idx++] = D3DWRAPCOORD_1 | D3DWRAPCOORD_3 | D3DWRAPCOORD_2;                  /* WRAP 6 */
    data->states[idx++] = D3DWRAPCOORD_2 | D3DWRAPCOORD_1 | D3DWRAPCOORD_0;                  /* WRAP 7 */
    data->states[idx++] = D3DWRAPCOORD_1 | D3DWRAPCOORD_0 | D3DWRAPCOORD_2 | D3DWRAPCOORD_3; /* WRAP 8 */
    data->states[idx++] = FALSE;                 /* CLIPPING */
    data->states[idx++] = FALSE;                 /* LIGHTING */
    data->states[idx++] = 255 << 16;             /* AMBIENT */
    data->states[idx++] = D3DFOG_EXP2;           /* FOGVERTEXMODE */
    data->states[idx++] = FALSE;                 /* COLORVERTEX */
    data->states[idx++] = FALSE;                 /* LOCALVIEWER */
    data->states[idx++] = TRUE;                  /* NORMALIZENORMALS */
    data->states[idx++] = D3DMCS_COLOR2;         /* DIFFUSEMATERIALSOURCE */
    data->states[idx++] = D3DMCS_MATERIAL;       /* SPECULARMATERIALSOURCE */
    data->states[idx++] = D3DMCS_COLOR1;         /* AMBIENTMATERIALSOURCE */
    data->states[idx++] = D3DMCS_COLOR2;         /* EMISSIVEMATERIALSOURCE */
    data->states[idx++] = D3DVBF_3WEIGHTS;       /* VERTEXBLEND */
    data->states[idx++] = 0xf1f1f1f1;            /* CLIPPLANEENABLE */
    if (0) data->states[idx++] = to_dword(32.0f);/* POINTSIZE, driver dependent, increase array size to enable */
    data->states[idx++] = to_dword(0.7f);        /* POINTSIZEMIN */
    data->states[idx++] = TRUE;                  /* POINTSPRITEENABLE */
    data->states[idx++] = TRUE;                  /* POINTSCALEENABLE */
    data->states[idx++] = to_dword(0.7f);        /* POINTSCALE_A */
    data->states[idx++] = to_dword(0.5f);        /* POINTSCALE_B */
    data->states[idx++] = to_dword(0.4f);        /* POINTSCALE_C */
    data->states[idx++] = FALSE;                 /* MULTISAMPLEANTIALIAS */
    data->states[idx++] = 0xABCDDBCA;            /* MULTISAMPLEMASK */
    data->states[idx++] = D3DPATCHEDGE_CONTINUOUS; /* PATCHEDGESTYLE */
    data->states[idx++] = D3DDMT_DISABLE;        /* DEBUGMONITORTOKEN */
    data->states[idx++] = to_dword(77.0f);       /* POINTSIZE_MAX */
    data->states[idx++] = TRUE;                  /* INDEXEDVERTEXBLENDENABLE */
    data->states[idx++] = 0x00000009;            /* COLORWRITEENABLE */
    data->states[idx++] = to_dword(0.2f);        /* TWEENFACTOR */
    data->states[idx++] = D3DBLENDOP_REVSUBTRACT;/* BLENDOP */
}

static HRESULT render_state_setup_handler(struct state_test *test)
{
    const struct render_state_arg *rsarg = test->test_arg;

    struct render_state_context *ctx = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*ctx));
    if (!ctx) return E_FAIL;
    test->test_context = ctx;

    test->default_data = &ctx->default_data_buffer;
    test->initial_data = &ctx->default_data_buffer;
    test->test_data_in = &ctx->test_data_buffer;
    test->test_data_out = &ctx->test_data_buffer;

    render_state_default_data_init(rsarg, &ctx->default_data_buffer);
    render_state_test_data_init(&ctx->test_data_buffer);
    render_state_poison_data_init(&ctx->poison_data_buffer);

    return D3D_OK;
}

static void render_state_teardown_handler(struct state_test *test)
{
    HeapFree(GetProcessHeap(), 0, test->test_context);
}

static void render_states_queue_test(struct state_test *test, const struct render_state_arg *test_arg)
{
    test->setup_handler = render_state_setup_handler;
    test->teardown_handler = render_state_teardown_handler;
    test->set_handler = render_state_set_handler;
    test->check_data = render_state_check_data;
    test->test_name = "set_get_render_states";
    test->test_arg = test_arg;
}

/* =================== Main state tests function =============================== */

static void test_state_management(IDirect3DDevice8 *device, D3DPRESENT_PARAMETERS *device_pparams)
{
    D3DCAPS8 caps;
    HRESULT hr;

    /* Test count: 2 for shader constants
     *             1 for lights
     *             1 for transforms
     *             1 for render states
     */
    const int max_tests = 5;
    struct state_test tests[5];
    unsigned int tcount = 0;

    struct shader_constant_arg pshader_constant_arg;
    struct shader_constant_arg vshader_constant_arg;
    struct render_state_arg render_state_arg;
    struct light_arg light_arg;

    hr = IDirect3DDevice8_GetDeviceCaps(device, &caps);
    ok(SUCCEEDED(hr), "GetDeviceCaps returned %#x.\n", hr);
    if (FAILED(hr)) return;

    texture_stages = caps.MaxTextureBlendStages;

    /* Zero test memory */
    memset(tests, 0, max_tests * sizeof(*tests));

    if (caps.VertexShaderVersion & 0xffff)
    {
        vshader_constant_arg.idx = 0;
        vshader_constant_arg.pshader = FALSE;
        shader_constants_queue_test(&tests[tcount++], &vshader_constant_arg);
    }

    if (caps.PixelShaderVersion & 0xffff)
    {
        pshader_constant_arg.idx = 0;
        pshader_constant_arg.pshader = TRUE;
        shader_constants_queue_test(&tests[tcount++], &pshader_constant_arg);
    }

    light_arg.idx = 0;
    lights_queue_test(&tests[tcount++], &light_arg);

    transform_queue_test(&tests[tcount++]);

    render_state_arg.device_pparams = device_pparams;
    render_state_arg.pointsize_max = caps.MaxPointSize;
    render_states_queue_test(&tests[tcount++], &render_state_arg);

    execute_test_chain_all(device, tests, tcount);
}

static void test_shader_constant_apply(IDirect3DDevice8 *device)
{
    static const float initial[] = {0.0f, 0.0f, 0.0f, 0.0f};
    static const float vs_const[] = {1.0f, 2.0f, 3.0f, 4.0f};
    static const float ps_const[] = {5.0f, 6.0f, 7.0f, 8.0f};
    DWORD stateblock;
    float ret[4];
    HRESULT hr;

    hr = IDirect3DDevice8_SetVertexShaderConstant(device, 0, initial, 1);
    ok(SUCCEEDED(hr), "SetVertexShaderConstant returned %#x\n", hr);
    hr = IDirect3DDevice8_SetVertexShaderConstant(device, 1, initial, 1);
    ok(SUCCEEDED(hr), "SetVertexShaderConstant returned %#x\n", hr);
    hr = IDirect3DDevice8_SetPixelShaderConstant(device, 0, initial, 1);
    ok(SUCCEEDED(hr), "SetPixelShaderConstant returned %#x\n", hr);
    hr = IDirect3DDevice8_SetPixelShaderConstant(device, 1, initial, 1);
    ok(SUCCEEDED(hr), "SetPixelShaderConstant returned %#x\n", hr);

    hr = IDirect3DDevice8_GetVertexShaderConstant(device, 0, ret, 1);
    ok(SUCCEEDED(hr), "GetVertexShaderConstant returned %#x\n", hr);
    ok(!memcmp(ret, initial, sizeof(initial)),
            "GetVertexShaderConstant got {%f, %f, %f, %f}, expected {%f, %f, %f, %f}\n",
            ret[0], ret[1], ret[2], ret[3], initial[0], initial[1], initial[2], initial[3]);
    hr = IDirect3DDevice8_GetVertexShaderConstant(device, 1, ret, 1);
    ok(SUCCEEDED(hr), "GetVertexShaderConstant returned %#x\n", hr);
    ok(!memcmp(ret, initial, sizeof(initial)),
            "GetVertexShaderConstant got {%f, %f, %f, %f}, expected {%f, %f, %f, %f}\n",
            ret[0], ret[1], ret[2], ret[3], initial[0], initial[1], initial[2], initial[3]);
    hr = IDirect3DDevice8_GetPixelShaderConstant(device, 0, ret, 1);
    ok(SUCCEEDED(hr), "GetPixelShaderConstant returned %#x\n", hr);
    ok(!memcmp(ret, initial, sizeof(initial)),
            "GetpixelShaderConstant got {%f, %f, %f, %f}, expected {%f, %f, %f, %f}\n",
            ret[0], ret[1], ret[2], ret[3], initial[0], initial[1], initial[2], initial[3]);
    hr = IDirect3DDevice8_GetPixelShaderConstant(device, 1, ret, 1);
    ok(SUCCEEDED(hr), "GetPixelShaderConstant returned %#x\n", hr);
    ok(!memcmp(ret, initial, sizeof(initial)),
            "GetPixelShaderConstant got {%f, %f, %f, %f}, expected {%f, %f, %f, %f}\n",
            ret[0], ret[1], ret[2], ret[3], initial[0], initial[1], initial[2], initial[3]);

    hr = IDirect3DDevice8_SetVertexShaderConstant(device, 0, vs_const, 1);
    ok(SUCCEEDED(hr), "SetVertexShaderConstant returned %#x\n", hr);
    hr = IDirect3DDevice8_SetPixelShaderConstant(device, 0, ps_const, 1);
    ok(SUCCEEDED(hr), "SetPixelShaderConstant returned %#x\n", hr);

    hr = IDirect3DDevice8_BeginStateBlock(device);
    ok(SUCCEEDED(hr), "BeginStateBlock returned %#x\n", hr);

    hr = IDirect3DDevice8_SetVertexShaderConstant(device, 1, vs_const, 1);
    ok(SUCCEEDED(hr), "SetVertexShaderConstant returned %#x\n", hr);
    hr = IDirect3DDevice8_SetPixelShaderConstant(device, 1, ps_const, 1);
    ok(SUCCEEDED(hr), "SetPixelShaderConstant returned %#x\n", hr);

    hr = IDirect3DDevice8_EndStateBlock(device, &stateblock);
    ok(SUCCEEDED(hr), "EndStateBlock returned %#x\n", hr);

    hr = IDirect3DDevice8_GetVertexShaderConstant(device, 0, ret, 1);
    ok(SUCCEEDED(hr), "GetVertexShaderConstant returned %#x\n", hr);
    ok(!memcmp(ret, vs_const, sizeof(vs_const)),
            "GetVertexShaderConstant got {%f, %f, %f, %f}, expected {%f, %f, %f, %f}\n",
            ret[0], ret[1], ret[2], ret[3], vs_const[0], vs_const[1], vs_const[2], vs_const[3]);
    hr = IDirect3DDevice8_GetVertexShaderConstant(device, 1, ret, 1);
    ok(SUCCEEDED(hr), "GetVertexShaderConstant returned %#x\n", hr);
    ok(!memcmp(ret, initial, sizeof(initial)),
            "GetVertexShaderConstant got {%f, %f, %f, %f}, expected {%f, %f, %f, %f}\n",
            ret[0], ret[1], ret[2], ret[3], initial[0], initial[1], initial[2], initial[3]);
    hr = IDirect3DDevice8_GetPixelShaderConstant(device, 0, ret, 1);
    ok(SUCCEEDED(hr), "GetPixelShaderConstant returned %#x\n", hr);
    ok(!memcmp(ret, ps_const, sizeof(ps_const)),
            "GetPixelShaderConstant got {%f, %f, %f, %f}, expected {%f, %f, %f, %f}\n",
            ret[0], ret[1], ret[2], ret[3], ps_const[0], ps_const[1], ps_const[2], ps_const[3]);
    hr = IDirect3DDevice8_GetPixelShaderConstant(device, 1, ret, 1);
    ok(SUCCEEDED(hr), "GetPixelShaderConstant returned %#x\n", hr);
    ok(!memcmp(ret, initial, sizeof(initial)),
            "GetPixelShaderConstant got {%f, %f, %f, %f}, expected {%f, %f, %f, %f}\n",
            ret[0], ret[1], ret[2], ret[3], initial[0], initial[1], initial[2], initial[3]);

    hr = IDirect3DDevice8_ApplyStateBlock(device, stateblock);
    ok(SUCCEEDED(hr), "Apply returned %#x\n", hr);

    /* Apply doesn't overwrite constants that aren't explicitly set on the source stateblock. */
    hr = IDirect3DDevice8_GetVertexShaderConstant(device, 0, ret, 1);
    ok(SUCCEEDED(hr), "GetVertexShaderConstant returned %#x\n", hr);
    ok(!memcmp(ret, vs_const, sizeof(vs_const)),
            "GetVertexShaderConstant got {%f, %f, %f, %f}, expected {%f, %f, %f, %f}\n",
            ret[0], ret[1], ret[2], ret[3], vs_const[0], vs_const[1], vs_const[2], vs_const[3]);
    hr = IDirect3DDevice8_GetVertexShaderConstant(device, 1, ret, 1);
    ok(SUCCEEDED(hr), "GetVertexShaderConstant returned %#x\n", hr);
    ok(!memcmp(ret, vs_const, sizeof(vs_const)),
            "GetVertexShaderConstant got {%f, %f, %f, %f}, expected {%f, %f, %f, %f}\n",
            ret[0], ret[1], ret[2], ret[3], vs_const[0], vs_const[1], vs_const[2], vs_const[3]);
    hr = IDirect3DDevice8_GetPixelShaderConstant(device, 0, ret, 1);
    ok(SUCCEEDED(hr), "GetPixelShaderConstant returned %#x\n", hr);
    ok(!memcmp(ret, ps_const, sizeof(ps_const)),
            "GetPixelShaderConstant got {%f, %f, %f, %f}, expected {%f, %f, %f, %f}\n",
            ret[0], ret[1], ret[2], ret[3], ps_const[0], ps_const[1], ps_const[2], ps_const[3]);
    hr = IDirect3DDevice8_GetPixelShaderConstant(device, 1, ret, 1);
    ok(SUCCEEDED(hr), "GetPixelShaderConstant returned %#x\n", hr);
    ok(!memcmp(ret, ps_const, sizeof(ps_const)),
            "GetPixelShaderConstant got {%f, %f, %f, %f}, expected {%f, %f, %f, %f}\n",
            ret[0], ret[1], ret[2], ret[3], ps_const[0], ps_const[1], ps_const[2], ps_const[3]);

    IDirect3DDevice8_DeleteStateBlock(device, stateblock);
}

START_TEST(stateblock)
{
    IDirect3DDevice8 *device = NULL;
    D3DPRESENT_PARAMETERS device_pparams;
    HMODULE d3d8_module;
    ULONG refcount;
    HRESULT hr;

    d3d8_module = LoadLibraryA("d3d8.dll");
    if (!d3d8_module)
    {
        skip("Could not load d3d8.dll\n");
        return;
    }

    hr = init_d3d8(d3d8_module, &device, &device_pparams);
    if (FAILED(hr))
    {
        FreeLibrary(d3d8_module);
        return;
    }

    test_begin_end_state_block(device);
    test_state_management(device, &device_pparams);
    test_shader_constant_apply(device);

    refcount = IDirect3DDevice8_Release(device);
    ok(!refcount, "Device has %u references left\n", refcount);

    FreeLibrary(d3d8_module);
}
