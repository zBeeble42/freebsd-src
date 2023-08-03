#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/condvar.h>
#include <sys/rman.h>
#include <sys/gpio.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h> 
#include <dev/usb/usb_busdma.h> 
#include <dev/usb/usb_process.h>

#include <dev/usb/usb_controller.h> 
#include <dev/usb/usb_bus.h>
#include <dev/usb/controller/xhci.h>

#include <machine/bus.h>
//#include <machine/resource.h> //pitäiskö tämäkin olla?

#include <dev/fdt/fdt_common.h>  //?
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>  //?
#include <dev/fdt/simplebus.h>  //?

#include <dev/extres/regulator/regulator.h>
//#include <dev/extres/phy/phy_usb.h>

//#include "usb_if.h" //?
#include "dev/usb/controller/generic_xhci.h"

#define BASEADR         0x104c0000
#define HOSTREG_START   0x104c0000
#define DEVREG_START    0x104d0000
#define OTGREG_START    0x104e0000
#define REG_SIZE        0x10000

#define DMA_64BIT       0

//#include "phynode_if.h"

static struct ofw_compat_data compat_data[] = { 
	{ "cdns,usb3",                          1 },
	{ NULL,                                 0 } 
};

/*
static struct resource_spec cdns_usb3_spec[] = { 
	{ SYS_RES_MEMORY,       0,      RF_ACTIVE },
	//	{ SYS_RES_IRQ,          0,      RF_ACTIVE | RF_SHAREABLE },
	{ -1, 0 } 
};
*/

struct cdns_usb3_softc {
	struct xhci_softc       sc;
	device_t                dev;
//	struct resource         *mem_res;
//	bus_space_tag_t         bst;
//	bus_space_handle_t      bsh;
//	void                    *ih_cookie;
//	regulator_t             phy_supply;
//	phy_t                   phy;  //tuleeko tämä?
//	int                     mode;
};

static int
cdns_usb3_attach_xhci(device_t dev)
{
	struct cdns_usb3_softc *cdns_sc = device_get_softc(dev);
	struct xhci_softc *sc = &cdns_sc->sc;
	int err, rid;

	printf("cdns_usb3_attach_xhci 1\n");

	/*
	sc->sc_io_res = cdns_sc->mem_res;
	printf("cdns_usb3_attach_xhci 2\n");
	sc->sc_io_tag = cdns_sc->bst;
	printf("cdns_usb3_attach_xhci 3\n");
	sc->sc_io_hdl = cdns_sc->bsh;
	printf("cdns_usb3_attach_xhci 4\n");
	sc->sc_io_size = rman_get_size(cdns_sc->mem_res);
	*/

	printf("cdns_usb3_attach_xhci 5\n");

	rid = 0;
	sc->sc_io_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
	    HOSTREG_START, HOSTREG_START+REG_SIZE, REG_SIZE, RF_ACTIVE);
	if (sc->sc_io_res == NULL) {
		device_printf(dev, "Unable to allocate memory\n"); 
		return (ENXIO); 
	}

	sc->sc_io_tag = rman_get_bustag(sc->sc_io_res);  //Tämä (struct bus_space, riscv:n bus.h) näyttää koostuvan vain funktiopointtereista
	sc->sc_io_hdl = rman_get_bushandle(sc->sc_io_res); //bus_space_handle_t (u_long)
	sc->sc_io_size = rman_get_size(sc->sc_io_res);  //datatyyppi (#definen takaa, sys/riscv/include/_bus.h) u_long
	printf("sc_io_hdl: %lu\n", sc->sc_io_hdl);
	printf("sc_io_size: %lu\n", sc->sc_io_size);

	rid = 1;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	printf("cdns_usb3_attach_xhci 6\n");
	if (sc->sc_irq_res == NULL) {
		device_printf(dev, "Unable to allocate irq resources\n");
		return (ENOMEM);
	}

	printf("cdns_usb3_attach_xhci 7\n");
	sc->sc_bus.bdev = device_add_child(dev, "usbus", -1); 
	if (sc->sc_bus.bdev == NULL) {
		device_printf(dev, "Failed to add USB device\n"); 
		return (ENXIO); 
	} 
        device_set_desc(sc->sc_bus.bdev, "Cadence USB 3.0 DRD controller");

	printf("cdns_usb3_attach_xhci 8\n");

	
	err = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
            NULL, (driver_intr_t *)xhci_interrupt, sc, &sc->sc_intr_hdl);
	if (err) {
		device_printf(dev, "Failed to setup interrupts\n");
		return (err);
	}

	printf("cdns_usb3_attach_xhci 9\n");
	
	err = xhci_init(sc, dev, DMA_64BIT); //1 olis 32-bit
	printf("cdns_usb3_attach_xhci 9b\n");
	if (err != 0) { 
		device_printf(dev, "Failed to init xHCI, with error %d\n", err);
		return (ENXIO); 
	} 

	printf("cdns_usb3_attach_xhci 10\n");

	err = xhci_start_controller(sc);
	if (err != 0) { 
		device_printf(dev, "Failed to start xHCI controller, with error %d\n", err);
		return (ENXIO); 
	} 

	printf("cdns_usb3_attach_xhci 11\n");
	
	err = device_probe_and_attach(sc->sc_bus.bdev); 
	if (err != 0) { 
		device_printf(dev, "Failed to initialize USB, with error %d\n", err); 
		return (ENXIO); 
	}

	printf("cdns_usb3_attach_xhci 12\n");
                                      
	return (0);
}

static int
cdns_usb3_probe(device_t dev) 
{

	printf("cdns_usb3_probe()\n");
	if (!ofw_bus_status_okay(dev)) {  //palauttaa 0 tai 1, kutsuu ofw_bus_get_status()
		printf("ofw_bus_status_okay() returns error\n");
		return (ENXIO);
	}

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0) {
		printf("ofw_bus_search_compatible() returns error\n");
		return (ENXIO);
	}

	device_set_desc(dev, "CDNS USB3");
	return (BUS_PROBE_DEFAULT); 
}

static int
cdns_usb3_attach(device_t dev)
{

	struct cdns_usb3_softc *sc;
	//struct phynode_init_def phy_init;
	//struct phynode *phynode;
	int err = 0;
	//phandle_t node; //onko käytössä?

	printf("cdns_usb3_attach()\n");
	sc = device_get_softc(dev);
	printf("cdns_usb3_attach() 2\n");
	sc->dev = dev;
	printf("cdns_usb3_attach() 3\n");
	//node = ofw_bus_get_node(dev);
	//printf("usb3_attach(), node: %d\n", node);

	/*
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	printf("cdns_usb3_attach() 4\n");
	if (sc->mem_res == NULL) {
		device_printf(dev, "Failed to map memory\n"); 
		return (ENXIO); 
	}
	printf("cdns_usb3_attach() 5\n");
	sc->bst = rman_get_bustag(sc->mem_res); 
	sc->bsh = rman_get_bushandle(sc->mem_res);
	printf("cdns_usb3_attach() 6\n");
	*/
	
	/*
	if (bus_alloc_resources(dev, cdns_usb3_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (ENXIO);
	}
	*/
	
	/*
	rv = phy_get_by_ofw_name(sc->dev, 0, "cdns3,usb3-phy", &sc->phy);  //string kuuluu olla tuollainen
	if (rv != 0) {
		device_printf(sc->dev, "cannot get phy for device 1\n"); 
		rv = phy_get_by_ofw_name(sc->dev, node, "cdns3,usb3-phy", &sc->phy);  //string kuuluu olla tuollainen
		if (rv != 0) {
			device_printf(sc->dev, "cannot get phy for device 2\n"); 
			//return (ENXIO);  //palauta
		}
	}

	rv = phy_get_by_ofw_property(sc->dev, 0, "cdns3,phy", &sc->phy); 
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'nvidia,phy' phy\n");
		//return (ENXIO);  palauta
	}
	*/
	
	//device_t cdev, device_t pdev
	/*
	rv = regulator_get_by_ofw_id(sc->dev, node, phynode_get_id(sc->phy), &sc->phy_supply);
	if (rv != 0) {
		device_printf(sc->dev, "cannot get '%s' regulator\n", buf); 
		//return (ENXIO);  //MIKÄ TÄHÄN??
	}
	*/


	/*  ilmeisesti tämä jää nyt sitten pois, kun ei kerran ole phynodea dt:ssä
	phy_init.ofw_node = ofw_bus_get_node(dev);
	phynode = phynode_create(dev, &cdns_usb3_phynode_class,
	    &phy_init);
	if (phynode == NULL) {
		device_printf(dev, "failed to create USB PHY\n");
		return (ENXIO);
	}
	if (phynode_register(phynode) == NULL) {
		device_printf(dev, "failed to create USB PHY\n");
		return (ENXIO);
	}
	*/

	err = cdns_usb3_attach_xhci(dev);
	if(err != 0) {
		device_printf(dev, "Attaching to xHCI failed\n");
	}

	printf("cdns_usb3_attach() 7\n");
	
	return (err);
}

static device_method_t cdns_usb3_methods[] = {
        /* Device interface */
	DEVMETHOD(device_probe,         cdns_usb3_probe), 
	DEVMETHOD(device_attach,        cdns_usb3_attach),

	DEVMETHOD_END 
};

/*
static driver_t cdns_usb3phy_driver = {
	"cdns_usb3phy",
	cdns_usb3phy_methods,
	sizeof(struct cdns_usb3phy_softc)
};
*/

DEFINE_CLASS_1(cdns_usb3, cdns_usb3_driver, cdns_usb3_methods,
    sizeof(struct cdns_usb3_softc), generic_xhci_driver); 
DRIVER_MODULE(cdns_usb3, simplebus, cdns_usb3_driver, 0, 0);  //vai EARLY_? dwc3.c:ssä ei ole earlyä
MODULE_DEPEND(cdns_usb3, xhci, 1, 1, 1); 

/*
DEFINE_CLASS_0(usb3phy, cdns_usb3phy_driver, cdns_usb3phy_methods,
sizeof(struct cdns_usb3phy_softc));
EARLY_DRIVER_MODULE(cdns_usb3phy, simplebus, cdns_usb3phy_driver, 0, 0,
BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_MIDDLE);
*/
//MODULE_VERSION(cdns_usb3phy, 1);//ilmeisesti ei tule module_dependiä tähän ollenkaan. tämä module_version onkin vanha juttu, että muut voi sitten olla riippuvaisia tästä
