// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_dsi.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 * Authors:
 *	Donghwa Lee <dh09.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <asm/unaligned.h>

#include <drm/drm_of.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_modes.h>
#include <drm/drm_vblank.h>
#include <exynos_display_common.h>

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/gpio/consumer.h>
#include <linux/irq.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/regulator/consumer.h>
#include <linux/component.h>
#include <linux/iommu.h>
#include <linux/delay.h>
#include <linux/dma-heap.h>

#include <video/mipi_display.h>

#if defined(CONFIG_CPU_IDLE)
#include <soc/samsung/exynos-cpupm.h>
#endif

#include <exynos_drm_crtc.h>
#include <exynos_drm_connector.h>
#include <exynos_drm_dsim.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_migov.h>
#include <exynos_drm_freq_hop.h>
#include <regs-dsim.h>

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
#include "exynos_drm_dsimfc.h"
#endif
#include "../exynos_drm_recovery.h"

struct dsim_device *dsim_drvdata[MAX_DSI_CNT];

static char panel_name[64];
module_param_string(panel_name, panel_name, sizeof(panel_name), 0644);
MODULE_PARM_DESC(panel_name, "preferred panel name");

static int dpu_dsim_log_level = 6;
module_param(dpu_dsim_log_level, int, 0600);
MODULE_PARM_DESC(dpu_dsim_log_level, "log level for dpu dsim [default : 6]");

#define dsim_info(dsim, fmt, ...)	\
dpu_pr_info(drv_name((dsim)), (dsim)->id, dpu_dsim_log_level, fmt, ##__VA_ARGS__)

#define dsim_warn(dsim, fmt, ...)	\
dpu_pr_warn(drv_name((dsim)), (dsim)->id, dpu_dsim_log_level, fmt, ##__VA_ARGS__)

#define dsim_err(dsim, fmt, ...)	\
dpu_pr_err(drv_name((dsim)), (dsim)->id, dpu_dsim_log_level, fmt, ##__VA_ARGS__)

#define dsim_debug(dsim, fmt, ...)	\
dpu_pr_debug(drv_name((dsim)), (dsim)->id, dpu_dsim_log_level, fmt, ##__VA_ARGS__)

#define host_to_dsi(host) container_of(host, struct dsim_device, dsi_host)

#define DSIM_ESCAPE_CLK_20MHZ	20

//#define DSIM_BIST

static inline enum dsim_panel_mode
get_op_mode(const struct exynos_display_mode *m)
{
	return m->mode_flags & MIPI_DSI_MODE_VIDEO ? DSIM_VIDEO_MODE :
				DSIM_COMMAND_MODE;
}

static inline struct dsim_device *encoder_to_dsim(struct drm_encoder *e)
{
	return container_of(e, struct dsim_device, encoder);
}

static inline bool is_dsim_enabled(const struct dsim_device *dsim)
{
	return	dsim->state == DSIM_STATE_HSCLKEN || dsim->state == DSIM_STATE_INIT;
}

static const struct of_device_id dsim_of_match[] = {
	{ .compatible = "samsung,exynos-dsim",
	  .data = NULL },
	{ }
};
MODULE_DEVICE_TABLE(of, dsim_of_match);

void dsim_dump(struct dsim_device *dsim)
{
	struct dsim_regs regs;

	dsim_info(dsim, "=== DSIM SFR DUMP ===\n");

	regs.regs = dsim->res.regs;
	regs.ss_regs = dsim->res.ss_reg_base;
	regs.phy_regs = dsim->res.phy_regs;
	regs.phy_regs_ex = dsim->res.phy_regs_ex;
	__dsim_dump(dsim->id, &regs);
}

static int dsim_phy_power_on(struct dsim_device *dsim)
{
	int ret;

	dsim_debug(dsim, "%s +\n", __func__);

	if (IS_ENABLED(CONFIG_BOARD_EMULATOR))
		return 0;

	ret = phy_power_on(dsim->res.phy);
	if (ret) {
		dsim_err(dsim, "failed to enable dphy(%d)\n", ret);
		return ret;
	}
	if (dsim->res.phy_ex) {
		ret = phy_power_on(dsim->res.phy_ex);
		if (ret) {
			dsim_err(dsim, "failed to enable ext dphy(%d)\n", ret);
			return ret;
		}
	}

	dsim_debug(dsim, "%s -\n", __func__);

	return 0;
}

static int dsim_phy_power_off(struct dsim_device *dsim)
{
	int ret;

	dsim_debug(dsim, "%s +\n", __func__);

	if (IS_ENABLED(CONFIG_BOARD_EMULATOR))
		return 0;

	ret = phy_power_off(dsim->res.phy);
	if (ret) {
		dsim_err(dsim, "failed to disable dphy(%d)\n", ret);
		return ret;
	}
	if (dsim->res.phy_ex) {
		ret = phy_power_off(dsim->res.phy_ex);
		if (ret) {
			dsim_err(dsim, "failed to disable ext dphy(%d)\n", ret);
			return ret;
		}
	}

	dsim_debug(dsim, "%s -\n", __func__);

	return 0;
}

void dsim_exit_ulps(struct dsim_device *dsim)
{
	dsim_debug(dsim, "%s +\n", __func__);

	if (dsim->state != DSIM_STATE_ULPS)
		return;

#if defined(CONFIG_CPU_IDLE)
	exynos_update_ip_idle_status(dsim->idle_ip_index, 0);
#endif

	dsim_phy_power_on(dsim);

	dsim_reg_init(dsim->id, &dsim->config, &dsim->clk_param, false);
	dsim_reg_exit_ulps_and_start(dsim->id, 0, 0x1F);

	dsim->state = DSIM_STATE_HSCLKEN;
	enable_irq(dsim->irq);

	dsim_debug(dsim, "%s -\n", __func__);
}

static void dsim_enable(struct drm_encoder *encoder)
{
	struct dsim_device *dsim = encoder_to_dsim(encoder);
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);

	if (dsim->state == DSIM_STATE_HSCLKEN) {
		dsim_info(dsim, "already enabled(%d)\n", dsim->state);
		return;
	}

	dsim_info(dsim, "%s +\n", __func__);

	pm_stay_awake(dsim->dev);
#if IS_ENABLED(CONFIG_PM_SLEEP)
	dev_warn(dsim->dev, "pm_stay_awake(active : %lu, relax : %lu)\n",
			dsim->dev->power.wakeup->active_count,
			dsim->dev->power.wakeup->relax_count);
#else
	dev_warn(dsim->dev, "pm_stay_awake\n");
#endif

#if defined(CONFIG_CPU_IDLE)
	exynos_update_ip_idle_status(dsim->idle_ip_index, 0);
#endif

	mutex_lock(&dsim->cmd_lock);

	dsim_phy_power_on(dsim);

	dsim_reg_init(dsim->id, &dsim->config, &dsim->clk_param, true);
	dsim_reg_start(dsim->id);

	if (dsim->state != DSIM_STATE_INIT)
		enable_irq(dsim->irq);

	dsim->state = DSIM_STATE_HSCLKEN;

	mutex_unlock(&dsim->cmd_lock);

#if defined(DSIM_BIST)
	dsim_reg_set_bist(dsim->id, true, DSIM_GRAY_GRADATION);
	dsim_dump(dsim);
#endif

	DPU_EVENT_LOG(DPU_EVT_DSIM_ENABLED, exynos_crtc, dsim);

	dsim_info(dsim, "%s -\n", __func__);
}

void dsim_enter_ulps(struct dsim_device *dsim)
{
	if (dsim->state != DSIM_STATE_HSCLKEN)
		return;

	dsim_debug(dsim, "%s +\n", __func__);

	/* Wait for current read & write CMDs. */
	mutex_lock(&dsim->cmd_lock);
	dsim->state = DSIM_STATE_ULPS;
	mutex_unlock(&dsim->cmd_lock);

	disable_irq(dsim->irq);
	dsim_reg_stop_and_enter_ulps(dsim->id, 0, 0x1F);

	dsim_phy_power_off(dsim);

#if defined(CONFIG_CPU_IDLE)
	exynos_update_ip_idle_status(dsim->idle_ip_index, 1);
#endif

	dsim_debug(dsim, "%s -\n", __func__);
}

static void dsim_disable(struct drm_encoder *encoder)
{
	u32 lanes;
	struct dsim_device *dsim = encoder_to_dsim(encoder);
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);

	if (dsim->state == DSIM_STATE_SUSPEND) {
		dsim_info(dsim, "already disabled(%d)\n", dsim->state);
		return;
	}

	dsim_info(dsim, "%s +\n", __func__);

	/* Wait for current read & write CMDs. */
	mutex_lock(&dsim->cmd_lock);
	del_timer(&dsim->cmd_timer);
#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	del_timer(&dsim->fcmd_timer);
#endif

	lanes = DSIM_LANE_CLOCK | GENMASK(dsim->config.data_lane_cnt, 1);
	dsim_reg_stop(dsim->id, lanes);
	disable_irq(dsim->irq);

	dsim->state = DSIM_STATE_SUSPEND;
	mutex_unlock(&dsim->cmd_lock);

	dsim_phy_power_off(dsim);

#if defined(CONFIG_CPU_IDLE)
	exynos_update_ip_idle_status(dsim->idle_ip_index, 1);
#endif

	pm_relax(dsim->dev);
#if IS_ENABLED(CONFIG_PM_SLEEP)
	dev_warn(dsim->dev, "pm_relax(active : %lu, relax : %lu)\n",
			dsim->dev->power.wakeup->active_count,
			dsim->dev->power.wakeup->relax_count);
#else
	dev_warn(dsim->dev, "pm_relax\n");
#endif

	DPU_EVENT_LOG(DPU_EVT_DSIM_DISABLED, exynos_crtc, dsim);

	dsim_info(dsim, "%s -\n", __func__);
}

static void dsim_modes_release(struct dsim_pll_params *pll_params)
{
	if (pll_params->params) {
		unsigned int i;

		for (i = 0; i < pll_params->num_modes; i++)
			kfree(pll_params->params[i]);
		kfree(pll_params->params);
	}
	kfree(pll_params);
}

static const struct dsim_pll_param *
dsim_get_clock_mode(const struct dsim_device *dsim,
		    const struct drm_display_mode *mode)
{
	int i;
	const struct dsim_pll_params *pll_params = dsim->pll_params;
	const size_t mlen = strnlen(mode->name, DRM_DISPLAY_MODE_LEN);
	const struct dsim_pll_param *ret = NULL;
	size_t plen;

	for (i = 0; i < pll_params->num_modes; i++) {
		const struct dsim_pll_param *p = pll_params->params[i];

		plen = strnlen(p->name, DRM_DISPLAY_MODE_LEN);

		if (!strncmp(mode->name, p->name, plen)) {
			ret = p;
			/*
			 * if it's not exact match, continue looking for exact
			 * match, use this as a fallback
			 */
			if (plen == mlen)
				break;
		}
	}

	return ret;
}

static int dsim_set_clock_mode(struct dsim_device *dsim,
			       const struct drm_display_mode *mode)
{
	const struct dsim_pll_param *p = dsim_get_clock_mode(dsim, mode);

	if (!p)
		return -ENOENT;

	dsim->config.dphy_pms.p = p->p;
	dsim->config.dphy_pms.m = p->m;
	dsim->config.dphy_pms.s = p->s;
	dsim->config.dphy_pms.k = p->k;

	dsim->config.dphy_pms.mfr = p->mfr;
	dsim->config.dphy_pms.mrr = p->mrr;
	dsim->config.dphy_pms.sel_pf = p->sel_pf;
	dsim->config.dphy_pms.icp = p->icp;
	dsim->config.dphy_pms.afc_enb = p->afc_enb;
	dsim->config.dphy_pms.extafc = p->extafc;
	dsim->config.dphy_pms.feed_en = p->feed_en;
	dsim->config.dphy_pms.fsel = p->fsel;
	dsim->config.dphy_pms.fout_mask = p->fout_mask;
	dsim->config.dphy_pms.rsel = p->rsel;
	dsim->config.dphy_pms.dither_en = p->dither_en;

	dsim->clk_param.hs_clk = p->pll_freq;
	dsim->clk_param.esc_clk = p->esc_freq;

	dsim_debug(dsim, "found proper pll parameter\n");
	dsim_debug(dsim, "\t%s(p:0x%x,m:0x%x,s:0x%x,k:0x%x)\n", p->name,
		 dsim->config.dphy_pms.p, dsim->config.dphy_pms.m,
		 dsim->config.dphy_pms.s, dsim->config.dphy_pms.k);

	dsim_debug(dsim, "\t%s(hs:%d,esc:%d)\n", p->name, dsim->clk_param.hs_clk,
		 dsim->clk_param.esc_clk);

	dsim->config.cmd_underrun_cnt = p->cmd_underrun_cnt;

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
/* Freq hop save default m,k value */
	dsim->config.dphy_pms.freq_hop_m = p->m;
	dsim->config.dphy_pms.freq_hop_k = p->k;
#endif
	return 0;
}

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
static int dsim_of_parse_modes(struct device_node *dsim_np, struct device_node *entry,
		struct dsim_pll_param *pll_param)
#else
static int dsim_of_parse_modes(struct device_node *entry,
		struct dsim_pll_param *pll_param)
#endif
{
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	struct stdphy_pms pms;
#else
	u32 res[14];
#endif
	int ret, cnt;

	memset(pll_param, 0, sizeof(*pll_param));

	ret = of_property_read_string(entry, "mode-name",
			(const char **)&pll_param->name);
	if (ret)
		return ret;

	cnt = of_property_count_u32_elems(entry, "pmsk");
	if (cnt != 4 && cnt != 14) {
		pr_err("mode %s has wrong pmsk elements number %d\n",
				pll_param->name, cnt);
		return -EINVAL;
	}
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	of_property_read_u32(entry, "hs-clk", &pll_param->pll_freq);
	of_property_read_u32(entry, "esc-clk", &pll_param->esc_freq);
	of_property_read_u32(entry, "cmd_underrun_cnt", &pll_param->cmd_underrun_cnt);

	ret = mcd_dsim_of_get_pll_param(dsim_np, pll_param->pll_freq * 1000, &pms);
	if (ret) {
		pr_err("ERR:%s Can't found pll param, hs_clock: %dkhz\n",
			__func__, pll_param->pll_freq * 1000);
		WARN_ON(1);
	}

	pll_param->p = pms.p;
	pll_param->m = pms.m;
	pll_param->s = pms.s;
	pll_param->k = pms.k;

	pll_param->mfr = pms.mfr;
	pll_param->mrr = pms.mrr;
	pll_param->sel_pf = pms.sel_pf;
	pll_param->icp = pms.icp;
	pll_param->afc_enb = pms.afc_enb;
	pll_param->extafc = pms.extafc;
	pll_param->feed_en = pms.feed_en;
	pll_param->fsel = pms.feed_en;
	pll_param->fout_mask = pms.fout_mask;
	pll_param->rsel = pms.rsel;

	pll_param->dither_en = false;

#else
	/* TODO: how dsi dither handle ? */
	of_property_read_u32_array(entry, "pmsk", res, cnt);
	pll_param->dither_en = false;
	pll_param->p = res[0];
	pll_param->m = res[1];
	pll_param->s = res[2];
	pll_param->k = res[3];
	if (cnt == 14) {
		pll_param->mfr = res[4];
		pll_param->mrr = res[5];
		pll_param->sel_pf = res[6];
		pll_param->icp = res[7];
		pll_param->afc_enb = res[8];
		pll_param->extafc = res[9];
		pll_param->feed_en = res[10];
		pll_param->fsel = res[11];
		pll_param->fout_mask = res[12];
		pll_param->rsel = res[13];
		if (pll_param->mfr || pll_param->mrr)
			pll_param->dither_en = true;
	}

	of_property_read_u32(entry, "hs-clk", &pll_param->pll_freq);
	of_property_read_u32(entry, "esc-clk", &pll_param->esc_freq);
	of_property_read_u32(entry, "cmd_underrun_cnt",
			&pll_param->cmd_underrun_cnt);
#endif
	pr_info("hs: %d, pmsk: %d, %d, %d, %d\n", pll_param->pll_freq,
			pll_param->p, pll_param->m, pll_param->s, pll_param->k);

	return 0;
}

static struct dsim_pll_params *dsim_of_get_clock_mode(struct dsim_device *dsim)
{
	struct device *dev = dsim->dev;
	struct device_node *np, *mode_np, *entry;
	struct dsim_pll_params *pll_params;

	np = of_parse_phandle(dev->of_node, "dsim_mode", 0);
	if (!np) {
		dsim_err(dsim, "could not get dsi modes\n");
		return NULL;
	}

	mode_np = of_get_child_by_name(np, "dsim-modes");
	if (!mode_np) {
		dsim_err(dsim, "%pOF: could not find dsim-modes node\n", np);
		goto getnode_fail;
	}

	pll_params = kzalloc(sizeof(*pll_params), GFP_KERNEL);
	if (!pll_params)
		goto getmode_fail;

	entry = of_get_next_child(mode_np, NULL);
	if (!entry) {
		dsim_err(dsim, "could not find child node of dsim-modes");
		goto getchild_fail;
	}

	pll_params->num_modes = of_get_child_count(mode_np);
	if (pll_params->num_modes == 0) {
		dsim_err(dsim, "%pOF: no modes specified\n", np);
		goto getchild_fail;
	}

	pll_params->params = kzalloc(sizeof(struct dsim_pll_param *) *
				pll_params->num_modes, GFP_KERNEL);
	if (!pll_params->params)
		goto getchild_fail;

	pll_params->num_modes = 0;

	for_each_child_of_node(mode_np, entry) {
		struct dsim_pll_param *pll_param;

		pll_param = kzalloc(sizeof(*pll_param), GFP_KERNEL);
		if (!pll_param)
			goto getpll_fail;
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
		if (dsim_of_parse_modes(dev->of_node, entry, pll_param) < 0) {
			kfree(pll_param);
			continue;
		}

#else
		if (dsim_of_parse_modes(entry, pll_param) < 0) {
			kfree(pll_param);
			continue;
		}
#endif

		pll_params->params[pll_params->num_modes] = pll_param;
		pll_params->num_modes++;
	}

	of_node_put(np);
	of_node_put(mode_np);
	of_node_put(entry);

	return pll_params;

getpll_fail:
	of_node_put(entry);
getchild_fail:
	dsim_modes_release(pll_params);
getmode_fail:
	of_node_put(mode_np);
getnode_fail:
	of_node_put(np);
	return NULL;
}

static void dsim_adjust_video_timing(struct dsim_device *dsim,
				     const struct drm_display_mode *mode)
{
	struct videomode vm;
	struct dpu_panel_timing *p_timing = &dsim->config.p_timing;

	drm_display_mode_to_videomode(mode, &vm);

	p_timing->vactive = vm.vactive;
	p_timing->vfp = vm.vfront_porch;
	p_timing->vbp = vm.vback_porch;
	p_timing->vsa = vm.vsync_len;

	p_timing->hactive = vm.hactive;
	p_timing->hfp = vm.hfront_porch;
	p_timing->hbp = vm.hback_porch;
	p_timing->hsa = vm.hsync_len;
	p_timing->vrefresh = drm_mode_vrefresh(mode);

	dsim_set_clock_mode(dsim, mode);
}

static void dsim_update_config_for_mode(struct dsim_reg_config *config,
				const struct drm_display_mode *mode,
				const struct exynos_display_mode *exynos_mode)
{
	config->mode = get_op_mode(exynos_mode);

	config->dsc.enabled = exynos_mode->dsc.enabled;
	if (config->dsc.enabled) {
		config->dsc.dsc_count = exynos_mode->dsc.dsc_count;
		config->dsc.slice_count = exynos_mode->dsc.slice_count;
		config->dsc.slice_height = exynos_mode->dsc.slice_height;
		config->dsc.slice_width = DIV_ROUND_UP(
				config->p_timing.hactive,
				config->dsc.slice_count);
	}
	config->bpc = exynos_mode->bpc;
}

static void dsim_set_display_mode(struct dsim_device *dsim,
				  const struct drm_display_mode *mode,
				  const struct exynos_display_mode *exynos_mode)
{
	struct dsim_reg_config *config = &dsim->config;

	if (!dsim->dsi_device)
		return;

	config->data_lane_cnt = dsim->dsi_device->lanes;
	dsim_adjust_video_timing(dsim, mode);

	dsim_update_config_for_mode(config, mode, exynos_mode);

	dsim_info(dsim, "dsim mode %s dsc is %s [%d %d %d %d]\n",
			config->mode == DSIM_VIDEO_MODE ? "video" : "cmd",
			config->dsc.enabled ? "enabled" : "disabled",
			config->dsc.dsc_count,
			config->dsc.slice_count,
			config->dsc.slice_width,
			config->dsc.slice_height);
}

static void dsim_atomic_mode_set(struct drm_encoder *encoder,
				 struct drm_crtc_state *crtc_state,
				 struct drm_connector_state *conn_state)
{
	struct dsim_device *dsim = encoder_to_dsim(encoder);
	struct dsim_reg_config *config = &dsim->config;
	struct exynos_drm_connector_state *exynos_conn_state =
					to_exynos_connector_state(conn_state);

	dsim_set_display_mode(dsim, &crtc_state->adjusted_mode,
			&exynos_conn_state->exynos_mode);

	if (exynos_conn_state->seamless_modeset & SEAMLESS_MODESET_MRES)
		dsim_reg_set_mres(dsim->id, config);
	if (exynos_conn_state->seamless_modeset & SEAMLESS_MODESET_VREF) {
		if (get_op_mode(&exynos_conn_state->exynos_mode) == DSIM_COMMAND_MODE) {
			dsim_reg_set_cm_underrun_lp_ref(dsim->id, config->cmd_underrun_cnt);
			dsim_reg_set_cmd_ctrl(dsim->id, config, &dsim->clk_param);
		} else if (get_op_mode(&exynos_conn_state->exynos_mode) == DSIM_VIDEO_MODE) {
			dsim_reg_set_porch(dsim->id, config);
		}
	}
}

static enum drm_mode_status dsim_mode_valid(struct drm_encoder *encoder,
					    const struct drm_display_mode *mode)
{
	struct dsim_device *dsim = encoder_to_dsim(encoder);

	if (!dsim_get_clock_mode(dsim, mode))
		return MODE_NOMODE;

	return MODE_OK;
}

static bool dsim_check_seamless_modeset(const struct dsim_device *dsim,
				const struct drm_display_mode *mode,
				const struct exynos_display_mode *exynos_mode)
{
	if (dsim->config.mode != get_op_mode(exynos_mode)) {
		dsim_debug(dsim, "can't chang op mode w/o seamelss\n");
		return false;
	}
	/*
	 * If it needs to check whether to change the display mode seamlessly
	 * for dsim, please add to here.
	 */

	return true;
}

static int dsim_atomic_check(struct drm_encoder *encoder,
			     struct drm_crtc_state *crtc_state,
			     struct drm_connector_state *conn_state)
{
	const struct drm_display_mode *mode;
	const struct exynos_display_mode *exynos_mode;
	const struct dsim_device *dsim = encoder_to_dsim(encoder);
	struct exynos_drm_connector_state *exynos_conn_state;

	if (!crtc_state->mode_changed)
		return 0;

	if (!is_exynos_drm_connector(conn_state->connector)) {
		dsim_warn(dsim, "%s: mode set is only supported w/exynos connector\n",
				__func__);
		return -EINVAL;
	}

	mode = &crtc_state->adjusted_mode;

	exynos_conn_state = to_exynos_connector_state(conn_state);
	exynos_mode = &exynos_conn_state->exynos_mode;
	if (exynos_conn_state->seamless_modeset &&
			!dsim_check_seamless_modeset(dsim, mode, exynos_mode))
		exynos_conn_state->seamless_modeset = false;

	return 0;
}

static const struct drm_encoder_helper_funcs dsim_encoder_helper_funcs = {
	.mode_valid = dsim_mode_valid,
	.atomic_mode_set = dsim_atomic_mode_set,
	.enable = dsim_enable,
	.disable = dsim_disable,
	.atomic_check = dsim_atomic_check,
};

static const struct drm_encoder_funcs dsim_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static int dsim_add_mipi_dsi_device(struct dsim_device *dsim)
{
	struct mipi_dsi_device_info info = { };
	struct device_node *node;
	const char *name;

	dsim_debug(dsim, "preferred panel is %s\n", panel_name);

	for_each_available_child_of_node(dsim->dsi_host.dev->of_node, node) {
		/* panel w/ reg node will be added in mipi_dsi_host_register */
		if (of_find_property(node, "reg", NULL))
			continue;

		if (of_property_read_u32(node, "channel", &info.channel))
			continue;

		name = of_get_property(node, "label", NULL);
		if (name && !strcmp(name, panel_name)) {
			strlcpy(info.type, name, sizeof(info.type));
			info.node = of_node_get(node);
			mipi_dsi_device_register_full(&dsim->dsi_host, &info);

			return 0;
		}
	}

	return -ENODEV;
}

static int dsim_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_encoder *encoder = dev_get_drvdata(dev);
	struct dsim_device *dsim = encoder_to_dsim(encoder);
	struct drm_device *drm_dev = data;
	int ret = 0;

	dsim_debug(dsim, "%s +\n", __func__);

	drm_encoder_init(drm_dev, encoder, &dsim_encoder_funcs,
			 DRM_MODE_ENCODER_DSI, NULL);
	drm_encoder_helper_add(encoder, &dsim_encoder_helper_funcs);

	encoder->possible_crtcs = exynos_drm_get_possible_crtcs(encoder,
							dsim->output_type);
	if (!encoder->possible_crtcs) {
		dsim_err(dsim, "failed to get possible crtc, ret = %d\n", ret);
		drm_encoder_cleanup(encoder);
		return -ENOTSUPP;
	}

	/* add the dsi device for the detected panel */
	dsim_add_mipi_dsi_device(dsim);

	ret = mipi_dsi_host_register(&dsim->dsi_host);

	dsim_debug(dsim, "%s -\n", __func__);

	return ret;
}

static void dsim_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct drm_encoder *encoder = dev_get_drvdata(dev);
	struct dsim_device *dsim = encoder_to_dsim(encoder);

	dsim_debug(dsim, "%s +\n", __func__);
	if (dsim->pll_params)
		dsim_modes_release(dsim->pll_params);

	mipi_dsi_host_unregister(&dsim->dsi_host);
}

static const struct component_ops dsim_component_ops = {
	.bind	= dsim_bind,
	.unbind	= dsim_unbind,
};

static int dsim_parse_dt(struct dsim_device *dsim)
{
	struct device_node *np = dsim->dev->of_node;

	if (!np) {
		dsim_err(dsim, "no device tree information\n");
		return -ENOTSUPP;
	}

	of_property_read_u32(np, "dsim,id", &dsim->id);
	if (dsim->id < 0 || dsim->id >= MAX_DSI_CNT) {
		dsim_err(dsim, "wrong dsim id(%d)\n", dsim->id);
		return -ENODEV;
	}

	if (of_property_read_u32(np, "dsi-burst-cmd",
				 &dsim->config.burst_cmd_en))
		dsim_err(dsim, "failed to parse dsi burst command\n");

	dsim->pll_params = dsim_of_get_clock_mode(dsim);

	return 0;
}

static int dsim_remap_by_name(struct dsim_device *dsim, struct device_node *np,
		void __iomem **base, const char *reg_name, enum dsim_regs_type type)
{
	int i;

	i = of_property_match_string(np, "reg-names", reg_name);
	*base = of_iomap(np, i);
	if (!(*base)) {
		dsim_err(dsim, "failed to remap %s SFR region\n", reg_name);
		return -EINVAL;
	}
	dsim_regs_desc_init(*base, reg_name, type, dsim->id);

	return 0;

}

static int dsim_remap_regs(struct dsim_device *dsim)
{
	struct device *dev = dsim->dev;
	struct device_node *np = dev->of_node;

	if (dsim_remap_by_name(dsim, np, &dsim->res.regs, "dsi", REGS_DSIM_DSI))
		goto err;

	if (dsim_remap_by_name(dsim, np, &dsim->res.phy_regs, "dphy", REGS_DSIM_PHY))
		goto err_dsi;

	if (dsim_remap_by_name(dsim, np, &dsim->res.phy_regs_ex, "dphy-extra",
			REGS_DSIM_PHY_BIAS))
		goto err_dphy;

	np = of_find_compatible_node(NULL, NULL, "samsung,exynos9-disp_ss");
	if (dsim_remap_by_name(dsim, np, &dsim->res.ss_reg_base, "sys",
			REGS_DSIM_SYS))
		goto err_dphy_ext;

	return 0;

err_dphy_ext:
	iounmap(dsim->res.phy_regs_ex);
err_dphy:
	iounmap(dsim->res.phy_regs);
err_dsi:
	iounmap(dsim->res.regs);
err:
	return -EINVAL;
}

static void dsim_underrun_info(struct dsim_device *dsim)
{
	const struct decon_device *decon = dsim_get_decon(dsim);
	const struct exynos_drm_crtc *exynos_crtc;

	if (!decon || (decon->state != DECON_STATE_ON))
		return;

	exynos_crtc = decon->crtc;
	dsim_info(dsim, "underrun(%d) irq occurs\n", exynos_crtc->d.underrun_cnt);
	if (IS_ENABLED(CONFIG_EXYNOS_BTS))
		decon->bts.ops->print_info(decon->crtc);
}

static irqreturn_t dsim_irq_handler(int irq, void *dev_id)
{
	struct dsim_device *dsim = dev_id;
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	unsigned int int_src;

	spin_lock(&dsim->slock);

	dsim_debug(dsim, "%s +\n", __func__);

	if (!is_dsim_enabled(dsim)) {
		dsim_info(dsim, "dsim power is off state(0x%x)\n", dsim->state);
		spin_unlock(&dsim->slock);
		return IRQ_HANDLED;
	}

	int_src = dsim_reg_get_int_and_clear(dsim->id);
	if (int_src & DSIM_INTSRC_SFR_PH_FIFO_EMPTY) {
		del_timer(&dsim->cmd_timer);
		complete(&dsim->ph_wr_comp);
		dsim_debug(dsim, "PH_FIFO_EMPTY irq occurs\n");
	}
#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	if (int_src & DSIM_INTSRC_SFR_PL_FIFO_EMPTY) {
		del_timer(&dsim->fcmd_timer);
		complete(&dsim->fcmd_wr_comp);
		dsim_debug(dsim, "PL_FIFO_EMPTY irq occurs\n");
	}
#endif
	if (int_src & DSIM_INTSRC_RX_DATA_DONE)
		complete(&dsim->rd_comp);
	if (int_src & DSIM_INTSRC_FRAME_DONE) {
		dsim_debug(dsim, "framedone irq occurs\n");
		DPU_EVENT_LOG(DPU_EVT_DSIM_FRAMEDONE, exynos_crtc, NULL);
	}
	if (int_src & DSIM_INTSRC_ERR_RX_ECC)
		dsim_err(dsim, "RX ECC Multibit error was detected!\n");

	if (int_src & DSIM_INTSRC_UNDER_RUN) {
		DPU_EVENT_LOG(DPU_EVT_DSIM_UNDERRUN, exynos_crtc, NULL);
		dsim_underrun_info(dsim);
	}

	if (int_src & DSIM_INTSRC_VT_STATUS) {
		dsim_debug(dsim, "vt_status irq occurs\n");
		if ((dsim->config.mode == DSIM_VIDEO_MODE) && exynos_crtc) {
			drm_crtc_handle_vblank(&exynos_crtc->base);
			if (exynos_crtc->migov)
				exynos_migov_update_vsync_cnt(exynos_crtc->migov);
		}
	}

	spin_unlock(&dsim->slock);

	return IRQ_HANDLED;
}

static int dsim_register_irq(struct dsim_device *dsim)
{
	struct device *dev = dsim->dev;
	struct device_node *np = dev->of_node;
	struct platform_device *pdev;
	int ret = 0;

	pdev = container_of(dev, struct platform_device, dev);

	dsim->irq = of_irq_get_byname(np, "dsim");
	ret = devm_request_irq(dsim->dev, dsim->irq, dsim_irq_handler, 0,
			pdev->name, dsim);
	if (ret) {
		dsim_err(dsim, "failed to install DSIM irq\n");
		return -EINVAL;
	}
	disable_irq(dsim->irq);

	return 0;
}

static int dsim_get_phys(struct dsim_device *dsim)
{
	dsim->res.phy = devm_phy_get(dsim->dev, "dsim_dphy");
	if (IS_ERR(dsim->res.phy)) {
		dsim_err(dsim, "failed to get dsim phy\n");
		return PTR_ERR(dsim->res.phy);
	}

	dsim->res.phy_ex = devm_phy_get(dsim->dev, "dsim_dphy_extra");
	if (IS_ERR(dsim->res.phy_ex)) {
		dsim_warn(dsim, "failed to get dsim extra phy\n");
		dsim->res.phy_ex = NULL;
	}

	return 0;
}

static int dsim_init_resources(struct dsim_device *dsim)
{
	int ret = 0;

	ret = dsim_remap_regs(dsim);
	if (ret)
		goto err;

	ret = dsim_register_irq(dsim);
	if (ret)
		goto err;

	ret = dsim_get_phys(dsim);
	if (ret)
		goto err;

err:
	return ret;
}

static int dsim_host_attach(struct mipi_dsi_host *host,
				  struct mipi_dsi_device *device)
{
	struct dsim_device *dsim = host_to_dsi(host);
	struct drm_bridge *bridge;
	int ret;

	dsim_debug(dsim, "%s +\n", __func__);

	bridge = of_drm_find_bridge(device->dev.of_node);
	if (!bridge) {
		struct drm_panel *panel;

		panel = of_drm_find_panel(device->dev.of_node);
		if (IS_ERR(panel)) {
			dsim_err(dsim, "failed to find panel\n");
			return PTR_ERR(panel);
		}

		bridge = devm_drm_panel_bridge_add_typed(host->dev, panel,
						   DRM_MODE_CONNECTOR_DSI);
		if (IS_ERR(bridge)) {
			dsim_err(dsim, "failed to create panel bridge\n");
			return PTR_ERR(bridge);
		}
	}

	ret = drm_bridge_attach(&dsim->encoder, bridge, NULL, 0);
	if (ret) {
		dsim_err(dsim, "Unable to attach panel bridge\n");
	} else {
		dsim->panel_bridge = bridge;
		dsim->dsi_device = device;
	}

	dsim_debug(dsim, "%s -\n", __func__);

	return ret;
}

static int dsim_host_detach(struct mipi_dsi_host *host,
				  struct mipi_dsi_device *device)
{
	struct dsim_device *dsim = host_to_dsi(host);

	dsim_info(dsim, "%s +\n", __func__);

	dsim_disable(&dsim->encoder);
	if (dsim->panel_bridge) {
		struct drm_bridge *bridge = dsim->panel_bridge;

		if (bridge->funcs && bridge->funcs->detach)
			bridge->funcs->detach(bridge);
		dsim->panel_bridge = NULL;
	}
	dsim->dsi_device = NULL;

	dsim_info(dsim, "%s -\n", __func__);
	return 0;
}

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
#define FCMD_SEND_START		0x4c
#define FCMD_SEND_CONTINUE	0x5c
#define FCMD_DATA_MAX_SIZE	0x00100000
#define FCMD_DFT_FPS		60
static int dsim_wait_fifo_empty(struct dsim_device *dsim, u32 frames)
{
	const struct decon_device *decon = dsim_get_decon(dsim);
	int cnt, fps;

	if (!decon)
		dsim_info(dsim, "no decon drvdata!!!\n");

	fps = decon ? decon->config.fps : FCMD_DFT_FPS;
	cnt = frames * 1000000 / fps / 10;

	do {
		if (dsim_reg_header_fifo_is_empty(dsim->id) &&
			dsim_reg_payload_fifo_is_empty(dsim->id))
			break;
		usleep_range(10, 11);
	} while (cnt--);

	return cnt;
}

static int dsim_wait_for_fcmd_xfer_done(struct dsim_device *dsim)
{
	const struct decon_device *decon = dsim_get_decon(dsim);
	int ret, fps;
	u32 dsimfc_timeout;

	if (!decon)
		dsim_info(dsim, "no decon drvdata!!!\n");

	fps = decon ? decon->config.fps : FCMD_DFT_FPS;
	dsimfc_timeout = 10 * 1000000 / fps / 1000;

	ret = wait_event_interruptible_timeout(dsim->dsimfc->xferdone_wq,
			dsim->dsimfc->config->done, msecs_to_jiffies(dsimfc_timeout));
	if (ret == 0) {
		dsim_err(dsim, "timeout of dsim%d fcmd xferdone\n", dsim->id);
		dsimfc_dump(dsim->dsimfc);
		return -ETIMEDOUT;
	} else
		dsim_info(dsim, "dsim%d fcmd xferdone\n", dsim->id);

	return 0;
}

static int dsim_fcmd_check(struct dsim_device *dsim, u32 id, u32 unit)
{
	u32 max_unit = DSIM_PL_FIFO_THRESHOLD;

	/* Check command id : FCMD support only below 2 types */
	if ((id != MIPI_DSI_GENERIC_LONG_WRITE) && (id != MIPI_DSI_DCS_LONG_WRITE)) {
		dsim_err(dsim, "data id %x is not supported in dma access mode.\n", id);
		return -EINVAL;
	}

	/* Check unit : FCMD xfer unit should not be over DSIM Paylod Theshold */
	if (unit > max_unit) {
		dsim_err(dsim, "packet unit should not be over %d\n", max_unit);
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)
	/* Check align : Xfer unit should be 8byte-aligned */
	if (unit % DSIM_FCMD_ALIGN_CONSTRAINT) {
		dsim_err(dsim, "packet unit should be 8byte-aligned\n");
		return -EINVAL;
	}
#endif

	return 0;
}

int dsim_write_data_fcmd(struct dsim_device *dsim, u32 id, dma_addr_t d0,
			u32 d1, u32 unit, bool wait_empty)
{
	int ret = 0;
	struct dsimfc_config dsimfc_config;
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	const struct exynos_drm_crtc_ops *ops;

	if (dsim_fcmd_check(dsim, id, unit))
		return -EINVAL;

	if (exynos_crtc)
		hibernation_block_exit(exynos_crtc->hibernation);

	mutex_lock(&dsim->cmd_lock);
	if (!is_dsim_enabled(dsim)) {
		dsim_warn(dsim, "DSIM is not ready. state(%d)\n", dsim->state);
		ret = -EINVAL;
		goto err_exit;
	}

	/* Wait 3 frames for ph & pl fifo to be empty before writing command */
	if (!dsim_wait_fifo_empty(dsim, 3)) {
		ret = -EINVAL;
		dsim_err(dsim, "ID(%d): DSIM cmd wr timeout @ don't available ph or pl 0x%lx\n", id, d0);
		goto err_exit;
	}

	if (dsim->config.burst_cmd_en)
		dsim_reg_enable_packetgo(dsim->id, false);

	dsimfc_config.di = (u8)id;
	dsimfc_config.size = d1;
	dsimfc_config.unit = (unit <= d1)? unit: d1;
	dsimfc_config.buf = d0;
	dsimfc_config.done = 0;

	ret = dsim->dsimfc->ops->atomic_config(dsim->dsimfc, &dsimfc_config);
	if (ret) {
		dsim_err(dsim, "failed to config fcmd\n", __func__, ret);
		goto err_exit;
	}

	if (dsim->config.burst_cmd_en)
		dsim_reg_enable_packetgo(dsim->id, false);

	dsim_reg_set_cmd_access_mode(dsim->id, 1);

	if (exynos_crtc) {
		ops = exynos_crtc->ops;
		/* force to exit pll sleep before starting command transfer */
		if (ops->pll_sleep_mask)
			ops->pll_sleep_mask(exynos_crtc, true);
	}

	ret = dsim->dsimfc->ops->atomic_start(dsim->dsimfc);
	if (ret) {
		dsim_err(dsim, "failed to start fcmd\n", __func__, ret);
		goto err_fcmd_exit;
	}

	ret = dsim_wait_for_fcmd_xfer_done(dsim);
	if (ret)
		goto err_fcmd_exit;

	if (!wait_empty) {
		/* timer is running, but already command is transferred */
		if (dsim_reg_header_fifo_is_empty(dsim->id) &&
			dsim_reg_payload_fifo_is_empty(dsim->id))
			dsim_debug(dsim, "Doesn't need to wait fifo_completion\n");
		else {
			dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PL_FIFO_EMPTY);
			/* Run write-fail dectector */
			reinit_completion(&dsim->fcmd_wr_comp);
			mod_timer(&dsim->fcmd_timer, jiffies + MIPI_WR_TIMEOUT);
		}
	} else {
		/* wating time for empty : 10 frames  */
		if (!dsim_wait_fifo_empty(dsim, 10)) {
			dsim_err(dsim, "ID(%d): DSIM command(%lx) fail\n", id, d0);
			ret = -EINVAL;
		}
	}

err_fcmd_exit:
	dsim_reg_set_cmd_access_mode(dsim->id, 0);
	ret |= dsim->dsimfc->ops->atomic_stop(dsim->dsimfc);
	if (dsim->config.burst_cmd_en)
		dsim_reg_enable_packetgo(dsim->id, true);

err_exit:
	if (dsim->config.burst_cmd_en)
		dsim_reg_enable_packetgo(dsim->id, true);
	mutex_unlock(&dsim->cmd_lock);
	if (exynos_crtc) {
		DPU_EVENT_LOG_CMD(exynos_crtc, id, (unsigned long)dsim->fcmd_buf_vaddr);
		hibernation_unblock(exynos_crtc->hibernation);
	}

	return ret;
}

static void dsim_fcmd_fail_detector(struct timer_list *arg)
{
	struct dsim_device *dsim = from_timer(dsim, arg, fcmd_timer);

	dsim_debug(dsim, "%s +\n", __func__);
	if (!is_dsim_enabled(dsim)) {
		dsim_err(dsim, "%s: DSIM is not ready. state(%d)\n", __func__,
				dsim->state);
		goto exit;
	}

	/* If already FIFO empty even though the timer is no pending */
	if (!timer_pending(&dsim->fcmd_timer)
			&& dsim_reg_payload_fifo_is_empty(dsim->id)) {
		reinit_completion(&dsim->fcmd_wr_comp);
		dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PL_FIFO_EMPTY);
		goto exit;
	}

exit:
	dsim_debug(dsim, "%s -\n", __func__);
	return;
}

static unsigned int dsim_fcmd_map_handle(struct device *dev,
		struct dsim_dma_buf_data *dma,
		struct dma_buf *buf)
{
	struct dsim_device *dsim = dev_get_drvdata(dev);

	dma->dma_buf = buf;

	if (IS_ERR_OR_NULL(dev)) {
		dsim_err(dsim, "%s: dev ptr is invalid\n", __func__);
		goto err_buf_map_attach;
	}

	dma->attachment = dma_buf_attach(dma->dma_buf, dev);
	if (IS_ERR_OR_NULL(dma->attachment)) {
		dsim_err(dsim, "dma_buf_attach() failed: %ld\n",
				PTR_ERR(dma->attachment));
		goto err_buf_map_attach;
	}

	dma->sg_table = dma_buf_map_attachment(dma->attachment,
			DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(dma->sg_table)) {
		dsim_err(dsim, "dma_buf_map_attachment() failed: %ld\n",
				PTR_ERR(dma->sg_table));
		goto err_buf_map_attachment;
	}

	/* This is DVA(Device Virtual Address) for setting base address SFR */
	dma->dma_addr = sg_dma_address(dma->sg_table->sgl);
	if (IS_ERR_VALUE(dma->dma_addr)) {
		dsim_err(dsim, "sg_dma_address() failed: %pa\n", &dma->dma_addr);
		goto err_iovmm_map;
	}

	return dma->dma_buf->size;

err_iovmm_map:
err_buf_map_attachment:
err_buf_map_attach:
	return 0;
}

static int dsim_alloc_fcmd_memory(u32 id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	dma_addr_t map_dma;
	struct dma_heap *dma_heap;
	unsigned int ret;
	u32 size = FCMD_DATA_MAX_SIZE;

	dsim_debug(dsim, "%s +\n", __func__);

	dev_info(dsim->dev, "allocating memory for fcmd%d\n", id);

	size = PAGE_ALIGN(size);

	dev_info(dsim->dev, "want %u bytes\n", size);

	dma_heap = dma_heap_find("system-uncached");
	if (dma_heap) {
		dsim->fcmd_buf = dma_heap_buffer_alloc(dma_heap, (size_t)size, 0, 0);
		dma_heap_put(dma_heap);
	} else {
		pr_err("dma_heap_find() failed\n");
		goto err_share_dma_buf;
	}
	if (IS_ERR(dsim->fcmd_buf)) {
		dev_err(dsim->dev, "ion_alloc() failed\n");
		goto err_share_dma_buf;
	}

	dsim->fcmd_buf_vaddr = dma_buf_vmap(dsim->fcmd_buf);

	ret = dsim_fcmd_map_handle(dsim->dev, &dsim->fcmd_buf_data, dsim->fcmd_buf);
	if (!ret)
		goto err_map;

	map_dma = dsim->fcmd_buf_data.dma_addr;

	dev_info(dsim->dev, "alloated memory for fcmd%d\n", id);
	dev_info(dsim->dev, "fcmd start addr = 0x%x\n", (u32)map_dma);

	dsim->fcmd_buf_allocated = true;

	dsim_debug(dsim, "%s -\n", __func__);

	return 0;

err_map:
	dma_buf_put(dsim->fcmd_buf);
err_share_dma_buf:
	return -ENOMEM;
}

int dsim_wr_data_fcmd(struct dsim_device *dsim,
			    const struct mipi_dsi_msg *msg)
{
	struct dsim_fcmd *fcmd = to_dsim_fcmd(msg);
	int ret;

	if (msg->tx_len > FCMD_DATA_MAX_SIZE) {
		dsim_err(dsim, "fcmd transfer size error(%d, %d)\n", msg->tx_len, FCMD_DATA_MAX_SIZE);
		return -EINVAL;
	}

	if (fcmd->xfer_unit > DSIM_PL_FIFO_THRESHOLD)
		dsim_err(dsim, "fcmd transfer align error(%d, %d)\n", fcmd->xfer_unit, DSIM_PL_FIFO_THRESHOLD);

	if (!dsim->fcmd_buf_allocated) {
		ret = dsim_alloc_fcmd_memory(dsim->id);
		if (ret) {
			dsim_err(dsim, "dsim%d : dsim_alloc_fcmd_memory fail\n", dsim->id);
			return ret;
		}
	}

	if (msg->tx_buf != dsim->fcmd_buf_vaddr)
		memcpy((u8 *)dsim->fcmd_buf_vaddr, msg->tx_buf, msg->tx_len);

	return dsim_write_data_fcmd(dsim, msg->type, dsim->fcmd_buf_data.dma_addr,
					msg->tx_len, fcmd->xfer_unit, true);
}

ssize_t dsim_host_fcmd_transfer(struct mipi_dsi_host *host,
			    const struct mipi_dsi_msg *msg)
{
	struct dsim_device *dsim = host_to_dsi(host);
	const struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	int ret;

	if (exynos_crtc)
		hibernation_block_exit(exynos_crtc->hibernation);

	ret = dsim_wr_data_fcmd(dsim, msg);
	if (ret < 0)
		dsim_err(dsim, "dsim%d : failed to transfer fast-command\n", dsim->id);

	if (exynos_crtc)
		hibernation_unblock(exynos_crtc->hibernation);

	return ret;
}
EXPORT_SYMBOL(dsim_host_fcmd_transfer);

static ssize_t dsim_fcmd_write_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dsim_device *dsim = dev_get_drvdata(dev);
	int ret = 0;
	unsigned long size;
	u32 id = MIPI_DSI_DCS_LONG_WRITE;
	int xfer_cnt, xfer_sz, xfer_unit;
	u32 padd_u, padd_c, padd_t;
	u8 *fcmd_data;
	int i;

	dsim_info(dsim, "%s +\n", __func__);

	if (count <= 0) {
		dsim_err(dsim, "fcmd count input error\n");
		return ret;
	}

	ret = kstrtoul(buf, 0, &size);
	if (ret) {
		dsim_err(dsim, "kstrtoul error\n");
		return ret;
	}

	/* xfer_unit does not have to be aligned with 8 from EVT1. */
	xfer_unit = DSIM_PL_FIFO_THRESHOLD;
	if (size <= (xfer_unit - 1))
		xfer_cnt = 1;
	else {
		xfer_cnt = size / (xfer_unit - 1);
		if (size % (xfer_unit - 1))
			xfer_cnt++;
	}
	xfer_sz = size + xfer_cnt;

	/*
	 * when xfer unit is not aligned with 8,,
	 *  padding should be considered.
	*/
	if (xfer_unit % DSIM_FCMD_ALIGN_CONSTRAINT) {
		padd_u = DSIM_FCMD_ALIGN_CONSTRAINT -(xfer_unit % DSIM_FCMD_ALIGN_CONSTRAINT);
		padd_c = xfer_unit - 1;
		padd_t = padd_u * padd_c;
		xfer_sz += padd_t;
	}

	if (xfer_sz > FCMD_DATA_MAX_SIZE) {
		dsim_err(dsim, "fcmd transfer size error(%d, %d)\n", size, xfer_sz);
		return -1;
	}

	if (dsim->fcmd_buf_allocated)
		dev_info(dsim->dev, "memory for fcmd%d has been already allocated\n", dsim->id);
	else {
		ret = dsim_alloc_fcmd_memory(dsim->id);
		if (ret) {
			dsim_err(dsim, "dsim%d : dsim_alloc_fcmd_memory fail\n", dsim->id);
			return -1;
		}
	}

	fcmd_data = (u8 *)dsim->fcmd_buf_vaddr;
	fcmd_data[0] = FCMD_SEND_START;
	for (i = 1; i < xfer_sz; i++) {
		if ((i % xfer_unit) == 0)
			fcmd_data[i] = FCMD_SEND_CONTINUE;
		else
			fcmd_data[i] = i % 256;
	}

	dsim_info(dsim, "%s : input size=%d, xfer size=%d, xfer unit=%d, fcmd_buf_addr=0x%lx\n",
			__func__, size, xfer_sz, xfer_unit, dsim->fcmd_buf_data.dma_addr);

	ret = dsim_write_data_fcmd(dsim, id, dsim->fcmd_buf_data.dma_addr,
					xfer_sz, xfer_unit, true);
	if (ret)
		dsim_err(dsim, "fcmd write error(%d)\n", ret);

	dsim_info(dsim, "%s -\n", __func__);

	return count;
}

static DEVICE_ATTR(fcmd_wr, 0200,
	NULL,
	dsim_fcmd_write_sysfs_store);
#endif

static void dsim_cmd_fail_detector(struct timer_list *arg)
{
	struct dsim_device *dsim = from_timer(dsim, arg, cmd_timer);

	dsim_debug(dsim, "%s +\n", __func__);

	if (!is_dsim_enabled(dsim)) {
		dsim_err(dsim, "%s: DSIM is not ready. state(%d)\n", __func__,
				dsim->state);
		goto exit;
	}

	/* If already FIFO empty even though the timer is no pending */
	if (!timer_pending(&dsim->cmd_timer)
			&& dsim_reg_header_fifo_is_empty(dsim->id)) {
		reinit_completion(&dsim->ph_wr_comp);
		dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);
		goto exit;
	}

exit:
	dsim_debug(dsim, "%s -\n", __func__);
}

static int dsim_wait_for_cmd_fifo_empty(struct dsim_device *dsim,
		bool must_wait)
{
	int ret = 0;

	if (!must_wait) {
		/* timer is running, but already command is transferred */
		if (dsim_reg_header_fifo_is_empty(dsim->id))
			del_timer(&dsim->cmd_timer);

		dsim_debug(dsim, "Doesn't need to wait fifo_completion\n");
		return ret;
	}

	del_timer(&dsim->cmd_timer);
	dsim_debug(dsim, "Waiting for fifo_completion...\n");

	if (!wait_for_completion_timeout(&dsim->ph_wr_comp, MIPI_WR_TIMEOUT)) {
		if (dsim_reg_header_fifo_is_empty(dsim->id)) {
			reinit_completion(&dsim->ph_wr_comp);
			dsim_reg_clear_int(dsim->id,
					DSIM_INTSRC_SFR_PH_FIFO_EMPTY);
			return 0;
		}
		ret = -ETIMEDOUT;
	}

	if ((is_dsim_enabled(dsim)) && (ret == -ETIMEDOUT))
		dsim_err(dsim, "have timed out\n");

	return ret;
}

static void dsim_long_data_wr(struct dsim_device *dsim, unsigned long d0,
		u32 d1)
{
	unsigned int data_cnt = 0, payload = 0;

	/* in case that data count is more then 4 */
	for (data_cnt = 0; data_cnt < d1; data_cnt += 4) {
		/*
		 * after sending 4bytes per one time,
		 * send remainder data less then 4.
		 */
		if ((d1 - data_cnt) < 4) {
			if ((d1 - data_cnt) == 3) {
				payload = *(u8 *)(d0 + data_cnt) |
				    (*(u8 *)(d0 + (data_cnt + 1))) << 8 |
					(*(u8 *)(d0 + (data_cnt + 2))) << 16;
			dsim_debug(dsim, "count = 3 payload = %x, %x %x %x\n",
				payload, *(u8 *)(d0 + data_cnt),
				*(u8 *)(d0 + (data_cnt + 1)),
				*(u8 *)(d0 + (data_cnt + 2)));
			} else if ((d1 - data_cnt) == 2) {
				payload = *(u8 *)(d0 + data_cnt) |
					(*(u8 *)(d0 + (data_cnt + 1))) << 8;
			dsim_debug(dsim, "count = 2 payload = %x, %x %x\n",
				payload,
				*(u8 *)(d0 + data_cnt),
				*(u8 *)(d0 + (data_cnt + 1)));
			} else if ((d1 - data_cnt) == 1) {
				payload = *(u8 *)(d0 + data_cnt);
			}

			dsim_reg_wr_tx_payload(dsim->id, payload);
		/* send 4bytes per one time. */
		} else {
			payload = *(u8 *)(d0 + data_cnt) |
				(*(u8 *)(d0 + (data_cnt + 1))) << 8 |
				(*(u8 *)(d0 + (data_cnt + 2))) << 16 |
				(*(u8 *)(d0 + (data_cnt + 3))) << 24;

			dsim_debug(dsim, "count = 4 payload = %x, %x %x %x %x\n",
				payload, *(u8 *)(d0 + data_cnt),
				*(u8 *)(d0 + (data_cnt + 1)),
				*(u8 *)(d0 + (data_cnt + 2)),
				*(u8 *)(d0 + (data_cnt + 3)));

			dsim_reg_wr_tx_payload(dsim->id, payload);
		}
	}
	dsim->burst_cmd.pl_count += ALIGN(d1, 4);
}

static bool dsim_fifo_empty_needed(struct dsim_device *dsim,
		unsigned int data_id, unsigned long data0)
{
	/* read case or partial update command */
	if (data_id == MIPI_DSI_DCS_READ
			|| data0 == MIPI_DCS_SET_COLUMN_ADDRESS
			|| data0 == MIPI_DCS_SET_PAGE_ADDRESS) {
		dsim_debug(dsim, "id:%d, data=%ld\n", data_id, data0);
		return true;
	}

	/* Check a FIFO level whether writable or not */
	if (!dsim_reg_is_writable_fifo_state(dsim->id))
		return true;

	return false;
}

static bool dsim_is_writable_pl_fifo_status(struct dsim_device *dsim, u32 word_cnt)
{
	if ((DSIM_PL_FIFO_THRESHOLD - dsim->burst_cmd.pl_count) > word_cnt)
		return true;
	else
		return false;
}

int dsim_check_ph_threshold(struct dsim_device *dsim, u32 cmd_cnt)
{
	int cnt = 5000;
	u32 available = 0;

	available = dsim_reg_is_writable_ph_fifo_state(dsim->id, cmd_cnt);

	/* Wait FIFO empty status during 50ms */
	if (!available) {
		dsim_debug(dsim, "%s, waiting for PH FIFO empty\n", __func__);
		do {
			if (dsim_reg_header_fifo_is_empty(dsim->id))
				break;
			usleep_range(10, 11);
			cnt--;
		} while (cnt);
	}
	return cnt;
}

int dsim_check_pl_threshold(struct dsim_device *dsim, u32 d1)
{
	int cnt = 5000;

	if (!dsim_is_writable_pl_fifo_status(dsim, d1)) {
		do {
			if (dsim_reg_payload_fifo_is_empty(dsim->id)) {
				dsim->burst_cmd.pl_count = 0;
				break;
			}
			usleep_range(10, 11);
			cnt--;
		} while (cnt);
	}

	return cnt;
}

int dsim_cal_pl_sum(struct mipi_dsi_msg msg[], int cmd_cnt, struct exynos_dsim_cmd_set *set)
{
	int i;
	int pl_size;
	int pl_sum_total = 0;
	int pl_sum = 0;
	struct mipi_dsi_msg *cmd;

	set->cnt = 1;
	set->index[0] = cmd_cnt - 1;

	for (i = 0; i < cmd_cnt; i++) {
		cmd = &msg[i];

		switch (msg->type) {
		/* long packet types of packet types for command. */
		case MIPI_DSI_GENERIC_LONG_WRITE:
		case MIPI_DSI_DCS_LONG_WRITE:
		case MIPI_DSI_PICTURE_PARAMETER_SET:
			if (cmd->tx_len > DSIM_PL_FIFO_THRESHOLD) {
				pr_err("Don't support for pl size exceeding %d\n",
				       DSIM_PL_FIFO_THRESHOLD);
				return -EINVAL;
			}
			pl_size = ALIGN(cmd->tx_len, 4);
			pl_sum += pl_size;
			pl_sum_total += pl_size;
			if (pl_sum > DSIM_PL_FIFO_THRESHOLD) {
				set->index[set->cnt - 1] = i - 1;
				set->cnt++;
				pl_sum = ALIGN(cmd->tx_len, 4);
			}
			break;
		}
		pr_debug("pl_sum_total : %d\n", pl_sum_total);
	}

	return pl_sum_total;
}
int dsim_write_cmd_set(struct dsim_device *dsim,
		       struct mipi_dsi_msg *msg, int cmd_cnt,
		       bool wait_vsync, bool wait_fifo)
{
	int i, j = 0;
	int ret = 0;
	int pl_sum;
	struct mipi_dsi_msg *cmd;
	struct exynos_dsim_cmd_set set;
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	struct drm_crtc *crtc = &exynos_crtc->base;

	dsim_debug(dsim, "%s+\n", __func__);

	mutex_lock(&dsim->cmd_lock);
	if (!is_dsim_enabled(dsim)) {
		dsim_err(dsim, "Not ready(%d)\n", dsim->state);
		ret = -EINVAL;
		goto err_exit;
	}

	if (dsim->config.burst_cmd_en == 0) {
		dsim_err(dsim, "MIPI burst command is not supported\n");
		ret = -EINVAL;
		goto err_exit;
	}

	if (!dsim->burst_cmd.init_done) {
		dsim_reg_set_burst_cmd(dsim->id, dsim->config.mode);
		dsim->burst_cmd.init_done = 1;
	}

	/* check PH available */
	if (!dsim_check_ph_threshold(dsim, cmd_cnt)) {
		ret = -EINVAL;
		dsim_err(dsim, "DSIM_%d cmd wr timeout @ don't available ph\n", dsim->id);
		goto err_exit;
	}

	/* check PL available */
	pl_sum = dsim_cal_pl_sum(msg, cmd_cnt, &set);
	if (!dsim_check_pl_threshold(dsim, pl_sum)) {
		ret = -EINVAL;
		dsim_err(
			dsim,
			"DSIM don't available pl, pl_cnt @ fifo : %d, pl_sum : %d",
			dsim->burst_cmd.pl_count, pl_sum);
		goto err_exit;
	}

	dsim_debug(dsim, "set_cnt:%d, cmd_cnt:%d\n", set.cnt, cmd_cnt);

	reinit_completion(&dsim->ph_wr_comp);
	dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);

	for (i = 0; i < set.cnt; i++) {
		/* packet go enable */

		for (; j < cmd_cnt; j++) {
			cmd = &msg[j];

			if (!cmd->tx_len)
				break;

			switch (cmd->type) {
			/* short packet types of packet types for command. */
			case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
			case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
			case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
			case MIPI_DSI_DCS_SHORT_WRITE:
			case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
			case MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE:
			case MIPI_DSI_COMPRESSION_MODE:
			case MIPI_DSI_COLOR_MODE_OFF:
			case MIPI_DSI_COLOR_MODE_ON:
			case MIPI_DSI_SHUTDOWN_PERIPHERAL:
			case MIPI_DSI_TURN_ON_PERIPHERAL:
				DPU_EVENT_LOG_CMD(exynos_crtc, cmd->type, ((char *)cmd->tx_buf)[0]);

				if (cmd->tx_len == 1)
					dsim_reg_wr_tx_header(dsim->id,
							      cmd->type,
							      ((unsigned long *)cmd->tx_buf)[0],
							      0, false);
				else
					dsim_reg_wr_tx_header(dsim->id,
							      cmd->type,
							      ((unsigned long *)cmd->tx_buf)[0],
							      ((u32 *)cmd->tx_buf)[1],
							      false);
				break;

			/* long packet types of packet types for command. */
			case MIPI_DSI_GENERIC_LONG_WRITE:
			case MIPI_DSI_DCS_LONG_WRITE:
			case MIPI_DSI_PICTURE_PARAMETER_SET:
				DPU_EVENT_LOG_CMD(exynos_crtc, cmd->type, (unsigned long)cmd->tx_buf);

				dsim_long_data_wr(dsim,
						  (unsigned long)cmd->tx_buf,
						  cmd->tx_len);
				dsim_reg_wr_tx_header(
					dsim->id, cmd->type,
					cmd->tx_len & 0xff,
					(cmd->tx_len & 0xff00) >> 8, false);
				break;

			default:
				dsim_err(dsim, "data id %x is not supported.\n", cmd->type);
				ret = -EINVAL;
				goto err_exit;
			}

			if (j == set.index[i])
				break;
		}

		if (set.cnt == 1) {
			if (wait_vsync)
				drm_crtc_wait_one_vblank(crtc);
		}

		/* set packet go ready*/
		dsim_reg_set_packetgo_ready(dsim->id);

		ret = dsim_wait_for_cmd_fifo_empty(dsim, wait_fifo);
		if (ret < 0)
			dsim_err(dsim, "MIPI cmd set tx timeout\n");
	}

	dsim_debug(dsim, "%s-\n", __func__);

err_exit:
	mutex_unlock(&dsim->cmd_lock);

	return ret;
}

int dsim_host_cmdset_transfer(struct mipi_dsi_host *host,
			      struct mipi_dsi_msg *msg, int cmd_cnt,
			      bool wait_vsync, bool wait_fifo)
{
	struct dsim_device *dsim = host_to_dsi(host);
	const struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	int ret = 0;

	if (exynos_crtc)
		hibernation_block_exit(exynos_crtc->hibernation);

	ret = dsim_write_cmd_set(dsim, msg, cmd_cnt, wait_vsync, wait_fifo);

	if (exynos_crtc)
		hibernation_unblock(exynos_crtc->hibernation);

	return ret;
}
EXPORT_SYMBOL(dsim_host_cmdset_transfer);

static int dsim_write_data(struct dsim_device *dsim, u32 id, unsigned long d0,
		u32 d1)
{
	int ret = 0;
	bool must_wait = true;
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	const struct exynos_drm_crtc_ops *ops;

	mutex_lock(&dsim->cmd_lock);
	if (!is_dsim_enabled(dsim)) {
		dsim_err(dsim, "Not ready(%d)\n", dsim->state);
		ret = -EINVAL;
		goto err_exit;
	}

	if (exynos_crtc) {
		DPU_EVENT_LOG_CMD(exynos_crtc, id, d0);

		ops = exynos_crtc->ops;
		/* force to exit pll sleep before starting command transfer */
		if (ops->pll_sleep_mask)
			ops->pll_sleep_mask(exynos_crtc, true);
	}

	if (dsim->config.burst_cmd_en && !dsim->burst_cmd.init_done) {
		dsim_reg_set_burst_cmd(dsim->id, dsim->config.mode);
		dsim->burst_cmd.init_done = 1;
	}

	reinit_completion(&dsim->ph_wr_comp);
	dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);

	/* Check available status of PH FIFO before writing command */
	if (!dsim_check_ph_threshold(dsim, 1)) {
		ret = -EINVAL;
		dsim_err(
			dsim,
			"ID(%d): DSIM cmd wr timeout @ don't available ph 0x%lx\n",
			id, d0);
		goto err_exit;
	}

	/* Run write-fail dectector */
	mod_timer(&dsim->cmd_timer, jiffies + MIPI_WR_TIMEOUT);

	switch (id) {
	/* short packet types of packet types for command. */
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	case MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE:
	case MIPI_DSI_COMPRESSION_MODE:
	case MIPI_DSI_COLOR_MODE_OFF:
	case MIPI_DSI_COLOR_MODE_ON:
	case MIPI_DSI_SHUTDOWN_PERIPHERAL:
	case MIPI_DSI_TURN_ON_PERIPHERAL:
		dsim_reg_wr_tx_header(dsim->id, id, d0, d1, false);
		must_wait = dsim_fifo_empty_needed(dsim, id, d0);
		break;

	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
	case MIPI_DSI_DCS_READ:
		dsim_reg_wr_tx_header(dsim->id, id, d0, d1, true);
		must_wait = dsim_fifo_empty_needed(dsim, id, d0);
		break;

	/* long packet types of packet types for command. */
	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
	case MIPI_DSI_PICTURE_PARAMETER_SET:
		if (d1 > DSIM_PL_FIFO_THRESHOLD) {
			dsim_err(
				dsim,
				"Don't support payload size that exceeds 2048byte\n");
			ret = -EINVAL;
			goto err_exit;
		}
		if (!dsim_check_pl_threshold(dsim, ALIGN(d1, 4))) {
			ret = -EINVAL;
			dsim_err(
				dsim,
				"ID(%d): DSIM don't available pl 0x%lx\n, pl_cnt : %d, wc : %d",
				id, d0, dsim->burst_cmd.pl_count, d1);
			goto err_exit;
		}

		dsim_long_data_wr(dsim, d0, d1);
		dsim_reg_wr_tx_header(dsim->id, id, d1 & 0xff,
				      (d1 & 0xff00) >> 8, false);
		must_wait = dsim_fifo_empty_needed(dsim, id, *(u8 *)d0);
		break;

	default:
		dsim_info(dsim, "data id %x is not supported.\n", id);
		ret = -EINVAL;
		goto err_exit;
	}

	/* set packet go ready*/
	if (dsim->config.burst_cmd_en)
		dsim_reg_set_packetgo_ready(dsim->id);

	ret = dsim_wait_for_cmd_fifo_empty(dsim, must_wait);
	if (ret < 0)
		dsim_err(dsim, "ID(%d): DSIM cmd wr timeout 0x%lx\n", id, d0);

err_exit:
	mutex_unlock(&dsim->cmd_lock);

	return ret;
}

static int dsim_wr_data(struct dsim_device *dsim, u32 type, const u8 data[],
		u32 len)
{
	u32 t;
	int ret = 0;

	switch (len) {
	case 0:
		return -EINVAL;
	case 1:
		t = type ? type : MIPI_DSI_DCS_SHORT_WRITE;
		ret = dsim_write_data(dsim, t, (unsigned long)data[0], 0);
		break;
	case 2:
		t = type ? type : MIPI_DSI_DCS_SHORT_WRITE_PARAM;
		ret = dsim_write_data(dsim, t, (unsigned long)data[0],
				(u32)data[1]);
		break;
	default:
		t = type ? type : MIPI_DSI_DCS_LONG_WRITE;
		ret = dsim_write_data(dsim, t, (unsigned long)data, len);
		break;
	}

	return ret ? ret : len;
}

#define DSIM_RX_PHK_HEADER_SIZE	4
static int dsim_read_data(struct dsim_device *dsim, u32 id, u32 addr, u32 cnt,
		u8 *buf)
{
	u32 rx_fifo, rx_size = 0;
	int i = 0, ret = 0;

	if (!is_dsim_enabled(dsim)) {
		dsim_err(dsim, "Not ready(%d)\n", dsim->state);
		return -EINVAL;
	}

	if (cnt > DSIM_RX_FIFO_MAX_DEPTH * 4 - DSIM_RX_PHK_HEADER_SIZE) {
		dsim_err(dsim, "requested rx size is wrong(%d)\n", cnt);
		return -EINVAL;
	}

	dsim_debug(dsim, "type[0x%x], cmd[0x%x], rx cnt[%d]\n", id, addr, cnt);

	/* Init RX FIFO before read and clear DSIM_INTSRC */
	dsim_reg_clear_int(dsim->id, DSIM_INTSRC_RX_DATA_DONE);

	reinit_completion(&dsim->rd_comp);

	/* Set the maximum packet size returned */
	dsim_write_data(dsim,
		MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, cnt, 0);

	/* Read request */
	if (id == MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM)
		dsim_write_data(dsim, id, addr & 0xff, (addr >> 8) & 0xff);
	else
		dsim_write_data(dsim, id, addr, 0);

	if (!wait_for_completion_timeout(&dsim->rd_comp, MIPI_RD_TIMEOUT)) {
		dsim_err(dsim, "read timeout\n");
		return -ETIMEDOUT;
	}

	mutex_lock(&dsim->cmd_lock);

	rx_fifo = dsim_reg_get_rx_fifo(dsim->id);
	dsim_debug(dsim, "rx fifo:0x%8x, response:0x%x, rx_size:%d\n", rx_fifo,
		 rx_fifo & 0xff, rx_size);

	/* Parse the RX packet data types */
	switch (rx_fifo & 0xff) {
	case MIPI_DSI_RX_ACKNOWLEDGE_AND_ERROR_REPORT:
		ret = dsim_reg_rx_err_handler(dsim->id, rx_fifo);
		if (ret < 0) {
			dsim_dump(dsim);
			goto exit;
		}
		break;
	case MIPI_DSI_RX_END_OF_TRANSMISSION:
		dsim_debug(dsim, "EoTp was received\n");
		break;
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE:
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE:
		buf[1] = (rx_fifo >> 16) & 0xff;
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE:
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE:
		buf[0] = (rx_fifo >> 8) & 0xff;
		dsim_debug(dsim, "short packet was received\n");
		rx_size = cnt;
		break;
	case MIPI_DSI_RX_DCS_LONG_READ_RESPONSE:
	case MIPI_DSI_RX_GENERIC_LONG_READ_RESPONSE:
		dsim_debug(dsim, "long packet was received\n");
		rx_size = (rx_fifo & 0x00ffff00) >> 8;

		while (i < rx_size) {
			const u32 rx_max =
				min_t(u32, rx_size, i + sizeof(rx_fifo));

			rx_fifo = dsim_reg_get_rx_fifo(dsim->id);
			dsim_debug(dsim, "payload: 0x%x i=%d max=%d\n", rx_fifo,
					i, rx_max);
			for (; i < rx_max; i++, rx_fifo >>= 8)
				buf[i] = rx_fifo & 0xff;
		}
		break;
	default:
		dsim_err(dsim, "packet format is invalid.\n");
		dsim_dump(dsim);
		ret = -EBUSY;
		goto exit;
	}

	if (!dsim_reg_rx_fifo_is_empty(dsim->id)) {
		dsim_err(dsim, "RX FIFO is not empty\n");
		dsim_dump(dsim);
		ret = -EBUSY;
	} else  {
		ret = rx_size;
	}
exit:
	mutex_unlock(&dsim->cmd_lock);

	return ret;
}

static int dsim_rd_data(struct dsim_device *dsim, u32 type, const u8 tx_data[],
		u8 rx_data[], u32 rx_len)
{
	u32 cmd = 0;

	switch (type) {
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
		cmd = tx_data[1] << 8;
	case MIPI_DSI_DCS_READ:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
		cmd |= tx_data[0];
	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
		break;
	default:
		dsim_err(dsim, "Invalid rx type (%d)\n", type);
	}
	return dsim_read_data(dsim, type, cmd, rx_len, rx_data);
}

static ssize_t dsim_host_transfer(struct mipi_dsi_host *host,
			    const struct mipi_dsi_msg *msg)
{
	struct dsim_device *dsim = host_to_dsi(host);
	const struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	int ret;

	if (exynos_crtc)
		hibernation_block_exit(exynos_crtc->hibernation);

	switch (msg->type) {
	case MIPI_DSI_DCS_READ:
	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
		ret = dsim_rd_data(dsim, msg->type, msg->tx_buf,
				   msg->rx_buf, msg->rx_len);
		break;
	default:
		ret = dsim_wr_data(dsim, msg->type, msg->tx_buf, msg->tx_len);
		break;
	}

	if (exynos_crtc)
		hibernation_unblock(exynos_crtc->hibernation);

	return ret;
}

/* TODO: Below operation will be registered after panel driver is created. */
static const struct mipi_dsi_host_ops dsim_host_ops = {
	.attach = dsim_host_attach,
	.detach = dsim_host_detach,
	.transfer = dsim_host_transfer,
};

static ssize_t bist_mode_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{

	struct dsim_device *dsim = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", dsim->bist_mode);
}

static ssize_t bist_mode_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	struct dsim_device *dsim = dev_get_drvdata(dev);
	int rc;
	unsigned int bist_mode;
	bool bist_en;

	rc = kstrtouint(buf, 0, &bist_mode);
	if (rc < 0)
		return rc;

	/*
	 * BIST modes:
	 * 0: Disable, 1: Color Bar, 2: GRAY Gradient, 3: User-Defined,
	 * 4: Prbs7 Random
	 */
	if (bist_mode > DSIM_BIST_MODE_MAX) {
		dsim_err(dsim, "invalid bist mode\n");
		return -EINVAL;
	}

	bist_en = bist_mode > 0;

	if (bist_en && dsim->state == DSIM_STATE_SUSPEND)
		dsim_enable(&dsim->encoder);

	dsim_reg_set_bist(dsim->id, bist_en, bist_mode - 1);
	dsim->bist_mode = bist_mode;

	if (!bist_en && dsim->state == DSIM_STATE_HSCLKEN)
		dsim_disable(&dsim->encoder);

	dsim_info(dsim, "0:Disable 1:ColorBar 2:GRAY Gradient 3:UserDefined\n");
	dsim_info(dsim, "4:Prbs7 Random (%d)\n", dsim->bist_mode);

	return len;
}
static DEVICE_ATTR_RW(bist_mode);

static int dsim_probe(struct platform_device *pdev)
{
	struct dsim_device *dsim;
	int ret;

	pr_info("%s +\n", __func__);

	dsim = devm_kzalloc(&pdev->dev, sizeof(*dsim), GFP_KERNEL);
	if (!dsim)
		return -ENOMEM;

	dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));

	dsim->dsi_host.ops = &dsim_host_ops;
	dsim->dsi_host.dev = &pdev->dev;
	dsim->dev = &pdev->dev;

	ret = dsim_parse_dt(dsim);
	if (ret)
		goto err;

	dsim_drvdata[dsim->id] = dsim;

	dsim->output_type = (dsim->id == 0) ?
			EXYNOS_DISPLAY_TYPE_DSI0 : EXYNOS_DISPLAY_TYPE_DSI1;

	spin_lock_init(&dsim->slock);
	mutex_init(&dsim->cmd_lock);
	init_completion(&dsim->ph_wr_comp);
	init_completion(&dsim->rd_comp);
#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	init_completion(&dsim->fcmd_wr_comp);
#endif

	/* prevent sleep enter during dsim on */
	ret = device_init_wakeup(dsim->dev, true);
	if (ret) {
		dev_err(dsim->dev, "failed to init wakeup device\n");
		goto err;
	}

	ret = dsim_init_resources(dsim);
	if (ret)
		goto err;

	ret = device_create_file(dsim->dev, &dev_attr_bist_mode);
	if (ret < 0)
		dsim_err(dsim, "failed to add sysfs entries\n");

	platform_set_drvdata(pdev, &dsim->encoder);

	timer_setup(&dsim->cmd_timer, dsim_cmd_fail_detector, 0);

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	timer_setup(&dsim->fcmd_timer, dsim_fcmd_fail_detector, 0);
	dsim->dsimfc = get_dsimfc_drvdata(dsim->id);
#endif

#if defined(CONFIG_CPU_IDLE)
	dsim->idle_ip_index = exynos_get_idle_ip_index(dev_name(&pdev->dev), 1);
	dsim_info(dsim, "dsim idle_ip_index[%d]\n", dsim->idle_ip_index);
	if (dsim->idle_ip_index < 0)
		dsim_warn(dsim, "idle ip index is not provided\n");
	exynos_update_ip_idle_status(dsim->idle_ip_index, 1);
#endif

	dsim->state = DSIM_STATE_SUSPEND;
	pm_runtime_enable(dsim->dev);

	pm_runtime_get_sync(dsim->dev);
	/* check dsim already init from LK */
	if (dsim_reg_get_link_clock(dsim->id)) {
		dsim->state = DSIM_STATE_INIT;
		enable_irq(dsim->irq);
	}
	pm_runtime_put_sync(dsim->dev);

	if (!IS_ENABLED(CONFIG_BOARD_EMULATOR)) {
		phy_init(dsim->res.phy);
		if (dsim->res.phy_ex)
			phy_init(dsim->res.phy_ex);
	}

	if (!dsim->id)
		dpu_init_freq_hop(dsim);

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	if (!dsim->id) {
		ret = device_create_file(dsim->dev, &dev_attr_fcmd_wr);
		if (ret)
			dsim_err(dsim, "failed to create fast command write sysfs\n");
		else
			dsim_info(dsim, "succeed in creating fast command write sysfs");

		ret = dsim_alloc_fcmd_memory(dsim->id);
		if (ret < 0)
			dsim_err(dsim, "failed to alloc fast command memory\n");
	}
#endif

	dsim_info(dsim, "driver has been probed.\n");
	return component_add(dsim->dev, &dsim_component_ops);

err:
	dsim_err(dsim, "failed to probe exynos dsim driver\n");
	return ret;
}

static u32 mask_value(u32 data, u32 mask, u32 shift)
{
	u32 ret = ((data & mask) >> shift);

	pr_debug(":recovery:data=0x%08x, mask=0x%08x, shift=%d, ret=0x%08x\n",
			data, mask, shift, ret);
	return ret;
}

static bool dsim_check_clklane_stuck(u32 link, u32 dphy)
{
	u32 data = 0x0;

	data = mask_value(link, DSIM_LINK_STATUS2_HSCLK_STATUS,
			DSIM_LINK_STATUS2_HSCLK_STATUS_SHIFT);

	if (data == 0x1) {
		if (mask_value(dphy, DSIM_DPHY_STATUS_STOPSTATE_CLK,
				DSIM_DPHY_STATUS_STOPSTATE_CLK_SHIFT) != 0x0)
			goto exit;

		if (mask_value(dphy, DSIM_DPHY_STATUS_TX_READY_HSCLK,
				DSIM_DPHY_STATUS_TX_READY_HSCLK_SHIFT) != 0x0)
			goto exit;

		pr_debug(":recovery:matched=lp2hs\n");
	} else if (data == 0x05) {
		if (mask_value(dphy, DSIM_DPHY_STATUS_STOPSTATE_CLK,
				DSIM_DPHY_STATUS_STOPSTATE_CLK_SHIFT) != 0x0)
			goto exit;

		if (mask_value(dphy, DSIM_DPHY_STATUS_TX_READY_HSCLK,
				DSIM_DPHY_STATUS_TX_READY_HSCLK_SHIFT) != 0x1)
			goto exit;

		pr_debug(":recovery:matched=hs2lp(included datalane)\n");
	} else {
		goto exit;
	}

	return true;
exit:
	pr_debug(":recovery: not matched\n");
	return false;
}

static bool dsim_check_datalane_stuck(u32 link, u32 dphy)
{
	u32 data = 0x0;

	data = mask_value(link, DSIM_LINK_STATUS2_HSDT_STATUS,
			DSIM_LINE_STATUS2_HSDT_STATUS_SHIFT);

	if (data == 0x02) {
		if (mask_value(dphy, DSIM_DPHY_STATUS_TX_REQUEST_HS_DATA,
				DSIM_DPHY_STATUS_TX_REQUEST_HS_DATA_SHIFT) != 0x1)
			goto exit;

		if (mask_value(dphy, DSIM_DPHY_STATUS_STOPSTATE_DATA,
				DSIM_DPHY_STATUS_STOPSTATE_DATA_SHIFT) != 0x0)
			goto exit;

		if (mask_value(dphy, DSIM_DPHY_STATUS_TX_READY_HS_DATA,
				DSIM_DPHY_STATUS_TX_READY_HS_DATA_SHIFT) != 0x0)
			goto exit;
		pr_err("recovery:%s:matched=lp2hs\n", __func__);
	} else if (data == 0x03) {
		if (mask_value(dphy, DSIM_DPHY_STATUS_TX_REQUEST_HS_DATA,
				DSIM_DPHY_STATUS_TX_REQUEST_HS_DATA_SHIFT) != 0x0)
			goto exit;

		if (mask_value(dphy, DSIM_DPHY_STATUS_STOPSTATE_DATA,
				DSIM_DPHY_STATUS_STOPSTATE_DATA_SHIFT) != 0x0)
			goto exit;

		if (mask_value(dphy, DSIM_DPHY_STATUS_TX_READY_HS_DATA,
				DSIM_DPHY_STATUS_TX_READY_HS_DATA_SHIFT) != 0x1)
			goto exit;
		pr_err("recovery:%s:matched=hs2lp\n", __func__);
	} else {
		goto exit;
	}

	return true;
exit:
	pr_info("recovery:%s:not matched\n", __func__);
	return false;
}

bool dsim_check_stuck(const struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
	struct dsim_device *dsim = decon_get_dsim(exynos_crtc->ctx);
	u32 link = 0x0;
	u32 dphy = 0x0;
	bool ret = false;

	if (!dsim)
		return ret;

	if (exynos_crtc)
		hibernation_block_exit(exynos_crtc->hibernation);

	if (!is_dsim_enabled(dsim))
		goto exit;

	spin_lock(&dsim->slock);
	link = dsim_reg_get_link_status2(dsim->id);
	dphy = dsim_reg_get_dphy_status(dsim->id);
	spin_unlock(&dsim->slock);

	if (dsim_check_clklane_stuck(link, dphy) ||
		dsim_check_datalane_stuck(link, dphy))
		ret = true;

exit:
	if (exynos_crtc)
		hibernation_unblock(exynos_crtc->hibernation);

	return ret;
}

static int dsim_remove(struct platform_device *pdev)
{
	struct dsim_device *dsim = platform_get_drvdata(pdev);

	device_remove_file(dsim->dev, &dev_attr_bist_mode);
	pm_runtime_disable(&pdev->dev);

	component_del(&pdev->dev, &dsim_component_ops);

	iounmap(dsim->res.ss_reg_base);
	iounmap(dsim->res.phy_regs_ex);
	iounmap(dsim->res.phy_regs);
	iounmap(dsim->res.regs);

	return 0;
}

struct platform_driver dsim_driver = {
	.probe = dsim_probe,
	.remove = dsim_remove,
	.driver = {
		   .name = "exynos-drmdsim",
		   .owner = THIS_MODULE,
		   .of_match_table = dsim_of_match,
	},
};

MODULE_SOFTDEP("pre: samsung_dma_heap");
MODULE_SOFTDEP("pre: phy-exynos-mipi");
MODULE_AUTHOR("Donghwa Lee <dh09.lee@samsung.com>");
MODULE_DESCRIPTION("Samsung SoC MIPI DSI Master");
MODULE_LICENSE("GPL v2");
