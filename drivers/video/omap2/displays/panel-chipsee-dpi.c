/*
 * Generic DPI Panels support
 *
 * Copyright (C) 2010 Canonical Ltd.
 * Author: Bryan Wu <bryan.wu@canonical.com>
 *
 * LCD panel driver for Sharp LQ043T1DG01
 *
 * Copyright (C) 2009 Texas Instruments Inc
 * Author: Vaibhav Hiremath <hvaibhav@ti.com>
 *
 * LCD panel driver for Toppoly TDO35S
 *
 * Copyright (C) 2009 CompuLab, Ltd.
 * Author: Mike Rapoport <mike@compulab.co.il>
 *
 * Copyright (C) 2008 Nokia Corporation
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <video/omapdss.h>

#include <video/omap-panel-data.h>

struct panel_config {
	struct omap_video_timings timings;

	int power_on_delay;
	int power_off_delay;

	/*
	 * Used to match device to panel configuration
	 * when use generic panel driver
	 */
	const char *name;
};

/* Panel configurations */
static struct panel_config generic_dpi_panels[] = {
	/* INNOLUX AT070TN92*/
	{
		{
			.x_res				= 800,
			.y_res				= 480,
			.pixel_clock		= 36000,
			.hfp				= 1,
			.hsw				= 48,
			.hbp				= 46,
			.vfp				= 12,
			.vsw				= 3,
			.vbp				= 23,

			.vsync_level		= OMAPDSS_SIG_ACTIVE_HIGH,
			.hsync_level		= OMAPDSS_SIG_ACTIVE_HIGH,
			.data_pclk_edge		= OMAPDSS_DRIVE_SIG_RISING_EDGE,
			.de_level			= OMAPDSS_SIG_ACTIVE_HIGH,
			.sync_pclk_edge		= OMAPDSS_DRIVE_SIG_OPPOSITE_EDGES,
		},
		
		.power_on_delay		= 0,
		.power_off_delay	= 0,
		.name				= "innolux_at070tn92",
	},
	/* INNOLUX AT070TNA2*/
	{
		{
			.x_res				= 1024,
			.y_res				= 600,
			.pixel_clock		= 36000,
			.hfp				= 1,
			.hsw				= 48,
			.hbp				= 46,
			.vfp				= 12,
			.vsw				= 3,
			.vbp				= 23,
			
			.vsync_level		= OMAPDSS_SIG_ACTIVE_LOW,
			.hsync_level		= OMAPDSS_SIG_ACTIVE_LOW,
			.data_pclk_edge		= OMAPDSS_DRIVE_SIG_RISING_EDGE,
			.de_level			= OMAPDSS_SIG_ACTIVE_HIGH,
			.sync_pclk_edge		= OMAPDSS_DRIVE_SIG_OPPOSITE_EDGES,
		},
		
		.power_on_delay		= 0,
		.power_off_delay	= 0,
		.name				= "innolux_hj070na13a",
	},
};

struct panel_drv_data {

	struct omap_dss_device *dssdev;

	struct panel_config *panel_config;
	
	struct mutex lock;
};

static inline struct panel_generic_dpi_data
*get_panel_data(const struct omap_dss_device *dssdev)
{
	return (struct panel_generic_dpi_data *) dssdev->data;
}

static int generic_dpi_panel_power_on(struct omap_dss_device *dssdev)
{
	int r;
	struct panel_generic_dpi_data *panel_data = get_panel_data(dssdev);
	struct panel_drv_data *drv_data = dev_get_drvdata(&dssdev->dev);
	struct panel_config *panel_config = drv_data->panel_config;

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		return 0;

	omapdss_dpi_set_timings(dssdev, &dssdev->panel.timings);
	omapdss_dpi_set_data_lines(dssdev, dssdev->phy.dpi.data_lines);
	
	r = omapdss_dpi_display_enable(dssdev);
	if (r)
		goto err0;

	/* wait couple of vsyncs until enabling the LCD */
	if (panel_config->power_on_delay)
		msleep(panel_config->power_on_delay);

	if (panel_data->platform_enable) {
		r = panel_data->platform_enable(dssdev);
		if (r)
			goto err1;
	}

	return 0;
err1:
	omapdss_dpi_display_disable(dssdev);
err0:
	return r;
}

static void generic_dpi_panel_power_off(struct omap_dss_device *dssdev)
{
	struct panel_generic_dpi_data *panel_data = get_panel_data(dssdev);
	struct panel_drv_data *drv_data = dev_get_drvdata(&dssdev->dev);
	struct panel_config *panel_config = drv_data->panel_config;

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		return;

	if (panel_data->platform_disable)
		panel_data->platform_disable(dssdev);

	/* wait couple of vsyncs after disabling the LCD */
	if (panel_config->power_off_delay)
		msleep(panel_config->power_off_delay);

	omapdss_dpi_display_disable(dssdev);
}

static int generic_dpi_panel_probe(struct omap_dss_device *dssdev)
{
	struct panel_generic_dpi_data *panel_data = get_panel_data(dssdev);
	struct panel_config *panel_config = NULL;
	struct panel_drv_data *drv_data = NULL;
	int i;

	dev_dbg(&dssdev->dev, "probe\n");

	if (!panel_data || !panel_data->name)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(generic_dpi_panels); i++) {
		if (strcmp(panel_data->name, generic_dpi_panels[i].name) == 0) {
			panel_config = &generic_dpi_panels[i];
			break;
		}
	}

	printk("MMIS: generic_dpi_panel_probe:panel_config:name: %s\n", panel_config->name);
	if (!panel_config)
		return -EINVAL;

	dssdev->panel.timings = panel_config->timings;

	drv_data = devm_kzalloc(&dssdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	drv_data->dssdev = dssdev;
	drv_data->panel_config = panel_config;
	
	mutex_init(&drv_data->lock);
	
	dev_set_drvdata(&dssdev->dev, drv_data);

	return 0;
}

static void __exit generic_dpi_panel_remove(struct omap_dss_device *dssdev)
{
	dev_dbg(&dssdev->dev, "remove\n");

	dev_set_drvdata(&dssdev->dev, NULL);
}

static int generic_dpi_panel_enable(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *drv_data = dev_get_drvdata(&dssdev->dev);
	int r;

	mutex_lock(&drv_data->lock);
	
	r = generic_dpi_panel_power_on(dssdev);
	if (r)
		goto err;

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
err:
	mutex_unlock(&drv_data->lock);

	return r;
}

static void generic_dpi_panel_disable(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *drv_data = dev_get_drvdata(&dssdev->dev);
	
	mutex_lock(&drv_data->lock);
	
	generic_dpi_panel_power_off(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	
	mutex_unlock(&drv_data->lock);
}

static void generic_dpi_panel_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	struct panel_drv_data *drv_data = dev_get_drvdata(&dssdev->dev);

	mutex_lock(&drv_data->lock);
	
	omapdss_dpi_set_timings(dssdev, timings);

	dssdev->panel.timings = *timings;
	
	mutex_unlock(&drv_data->lock);
}

static void generic_dpi_panel_get_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	struct panel_drv_data *drv_data = dev_get_drvdata(&dssdev->dev);

	mutex_lock(&drv_data->lock);
	
	*timings = dssdev->panel.timings;
	
	mutex_unlock(&drv_data->lock);
}

static int generic_dpi_panel_check_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	struct panel_drv_data *drv_data = dev_get_drvdata(&dssdev->dev);
	int r;
	
	mutex_lock(&drv_data->lock);
	
	r = dpi_check_timings(dssdev, timings);
	
	mutex_unlock(&drv_data->lock);
	
	return r;
}

static struct omap_dss_driver dpi_driver = {
	.probe		= generic_dpi_panel_probe,
	.remove		= __exit_p(generic_dpi_panel_remove),

	.enable		= generic_dpi_panel_enable,
	.disable	= generic_dpi_panel_disable,

	.set_timings	= generic_dpi_panel_set_timings,
	.get_timings	= generic_dpi_panel_get_timings,
	.check_timings	= generic_dpi_panel_check_timings,

	.driver         = {
		.name   = "chipsee_dpi_panel",
		.owner  = THIS_MODULE,
	},
};

static int __init generic_dpi_panel_drv_init(void)
{
	return omap_dss_register_driver(&dpi_driver);
}

static void __exit generic_dpi_panel_drv_exit(void)
{
	omap_dss_unregister_driver(&dpi_driver);
}

module_init(generic_dpi_panel_drv_init);
module_exit(generic_dpi_panel_drv_exit);
MODULE_LICENSE("GPL");
