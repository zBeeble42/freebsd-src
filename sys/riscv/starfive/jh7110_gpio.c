/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@FreeBSD.org> (???)
 * Copyright (c) 2021 Soren Schmidt <sos@deepcore.dk>   (???)
 * Copyright (c) 2023 Jari Sihvola <jsihv@gmx.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* This is not yet modified for JH7110 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/extres/clk/clk.h>

#include "gpio_if.h"

#define JH7100_GPIO_PINS 64

#define GPIOEN          0x0
#define GPIOE_0         0x28
#define GPIOE_1         0x2c
#define GPIO_DIN_LOW	0x48
#define GPIO_DIN_HIGH	0x4c
#define GP0_DOUT_CFG	0x50
#define GP0_DOEN_CFG    0x54

struct jh7100_gpio_softc {
	device_t              dev; 
	device_t              busdev;
	struct mtx            mtx; 
	struct resource       *res; 
	clk_t                 clk;
};

static struct ofw_compat_data compat_data[] = { 
	{"starfive,jh7100-pinctrl", 1},
	{NULL,             0} 
};

static struct resource_spec jh7100_gpio_spec[] = {
	{ SYS_RES_MEMORY,     0,      RF_ACTIVE },
	{ -1, 0 } 
};

#define JH7100_GPIO_LOCK(_sc)               mtx_lock(&(_sc)->mtx)
#define JH7100_GPIO_UNLOCK(_sc)             mtx_unlock(&(_sc)->mtx)

#define JH7100_GPIO_READ(sc, reg)           bus_read_4((sc)->res, (reg))
#define JH7100_GPIO_WRITE(sc, reg, val)     bus_write_4((sc)->res, (reg), (val))

static device_t
jh7100_gpio_get_bus(device_t dev)
{
	struct jh7100_gpio_softc *sc;
	printf("jh7100_gpio_get_bus\n");

	sc = device_get_softc(dev);

	return (sc->busdev);
}

static int
jh7100_gpio_pin_max(device_t dev, int *maxpin)
{
	*maxpin = JH7100_GPIO_PINS - 1;

	return (0);
}

static int
jh7100_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct jh7100_gpio_softc *sc;
	uint32_t reg;
	printf("jh7100_gpio_get_get\n");
	
	sc = device_get_softc(dev);

	if (pin >= JH7100_GPIO_PINS)
		return (EINVAL);

	JH7100_GPIO_LOCK(sc);
	if (pin < 32) {
		reg = JH7100_GPIO_READ(sc, GPIO_DIN_LOW);
		*val = (reg >> pin) & 0x1;
	}
	else {
		reg = JH7100_GPIO_READ(sc, GPIO_DIN_HIGH);
		*val = (reg >> (pin - 32)) & 0x1;
	}
	JH7100_GPIO_UNLOCK(sc);

	return (0);
}

static int
jh7100_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct jh7100_gpio_softc *sc;
	printf("jh7100_gpio_pin_set\n");

	sc = device_get_softc(dev);

	if (pin >= JH7100_GPIO_PINS)
		return (EINVAL);

	JH7100_GPIO_LOCK(sc);
	JH7100_GPIO_WRITE(sc, GP0_DOUT_CFG + pin * 8, value);
	JH7100_GPIO_UNLOCK(sc);

	return (0);
}

static int
jh7100_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct jh7100_gpio_softc *sc;
	printf("jh7100_gpio_pin_getflags\n");
	
	sc = device_get_softc(dev);

	if (pin >= JH7100_GPIO_PINS)
		return (EINVAL);

	/* Reading the direction */

	JH7100_GPIO_LOCK(sc);
	if (JH7100_GPIO_READ(sc, GP0_DOEN_CFG + pin * 8) & 0x1)
		*flags |= GPIO_PIN_INPUT;
	else
		*flags |= GPIO_PIN_OUTPUT;
	JH7100_GPIO_UNLOCK(sc);
	
	return (0);
}


static int
jh7100_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct jh7100_gpio_softc *sc;
	printf("jh7100_gpio_pin_setflags\n");

	sc = device_get_softc(dev);
		
	if (pin >= JH7100_GPIO_PINS)
		return (EINVAL);

	/* Setting the direction, enable or disable output */

	JH7100_GPIO_LOCK(sc);
	if (flags & GPIO_PIN_INPUT) {
		JH7100_GPIO_WRITE(sc, GP0_DOEN_CFG + pin * 8, 0x1);
	}

	else if (flags & GPIO_PIN_OUTPUT) {
		JH7100_GPIO_WRITE(sc, GP0_DOEN_CFG + pin * 8, 0x0);
	}
	JH7100_GPIO_UNLOCK(sc); 
	
	return (0);
}

static int
jh7100_gpio_probe(device_t dev) 
{

	if (!ofw_bus_status_okay(dev)) {
		return (ENXIO);
	}

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0) {
		return (ENXIO);
	}

	device_set_desc(dev, "jh7100 GPIO generator driver");
	
	return (BUS_PROBE_DEFAULT);
}

static int
jh7100_gpio_detach(device_t dev)
{
	struct jh7100_gpio_softc *sc;
	printf("jh7100_gpio_detach\n");
	
	sc = device_get_softc(dev);

	bus_release_resources(dev, jh7100_gpio_spec, &sc->res);
	gpiobus_detach_bus(dev);
	mtx_destroy(&sc->mtx); 
	
	return (0);
}

static int
jh7100_gpio_attach(device_t dev)
{
	struct jh7100_gpio_softc *sc;
	phandle_t node;
	int err;

	printf("jh7100_gpio_attach\n");
	sc = device_get_softc(dev);
	sc->dev = dev;

	node = ofw_bus_get_node(sc->dev);
	if (!OF_hasprop(node, "gpio-controller"))
		return (ENXIO);

	mtx_init(&sc->mtx, device_get_nameunit(sc->dev), NULL, MTX_DEF);

	if (bus_alloc_resources(dev, jh7100_gpio_spec, &sc->res)) { 
		device_printf(dev, "could not allocate resources\n"); 
		bus_release_resources(dev, jh7100_gpio_spec, &sc->res); 
		mtx_destroy(&sc->mtx); 
		return (ENXIO); 
	} 

	if (clk_get_by_ofw_index(dev, 0, 0, &sc->clk) != 0) { 
		device_printf(dev, "Cannot get clock\n"); 
		jh7100_gpio_detach(dev);
		return (ENXIO); 
	}

	err = clk_enable(sc->clk);
	if (err != 0) { 
		device_printf(dev, "Could not enable clock %s\n", 
			      clk_get_name(sc->clk)); 
		jh7100_gpio_detach(dev);
		return (ENXIO); 
	} 

	sc->busdev = gpiobus_attach_bus(dev);
	if (sc->busdev == NULL) {
		device_printf(dev, "Cannot attach gpiobus\n");
		jh7100_gpio_detach(dev);
		return (ENXIO);
	}

	/* Reseting GPIO interrupts */
	/* jh7100 datasheet [2021-4-21] errorneously claims 1 is to disable */
	JH7100_GPIO_WRITE(sc, GPIOE_0, 0);
	JH7100_GPIO_WRITE(sc, GPIOE_1, 0);
	JH7100_GPIO_WRITE(sc, GPIOEN, 1);
	
	return (0);
}

static phandle_t  
jh7100_gpio_get_node(device_t bus, device_t dev) 
{
	return (ofw_bus_get_node(bus)); 
}

static device_method_t jh7100_gpio_methods[] = {
        /* Device interface */
	DEVMETHOD(device_probe,         jh7100_gpio_probe), 
	DEVMETHOD(device_attach,        jh7100_gpio_attach),
	DEVMETHOD(device_detach,        jh7100_gpio_detach),

        /* GPIO protocol */
	DEVMETHOD(gpio_get_bus,         jh7100_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,         jh7100_gpio_pin_max),
	DEVMETHOD(gpio_pin_getflags,    jh7100_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,    jh7100_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,         jh7100_gpio_pin_get), 
	DEVMETHOD(gpio_pin_set,         jh7100_gpio_pin_set), 

        /* ofw_bus interface */ 
	DEVMETHOD(ofw_bus_get_node,     jh7100_gpio_get_node),

	DEVMETHOD_END 
}; 

DEFINE_CLASS_0(gpio, jh7100_gpio_driver, jh7100_gpio_methods,
    sizeof(struct jh7100_gpio_softc));
EARLY_DRIVER_MODULE(jh7100_gpio, simplebus, jh7100_gpio_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
MODULE_DEPEND(jh7100_gpio, gpiobus, 1, 1, 1);
