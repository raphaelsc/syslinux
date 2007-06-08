/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2006-2007 Erwan Velu - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------------------------------------------------------- */

/*
 * pci.c
 *
 * A module to extract pci informations
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <console.h>
#include <sys/pci.h>
#include <com32.h>
#include <stdbool.h>

#ifdef DEBUG
# define dprintf printf
#else
# define dprintf(...) ((void)0)
#endif

#define MAX_LINE 512
static char *skipspace(char *p)
{
  while (*p && *p <= ' ')
    p++;

  return p;
}

struct match *find_pci_device(s_pci_device_list * pci_device_list,
			      struct match *list)
{
  int pci_dev;
  uint32_t did, sid;
  struct match *m;
  for (m = list; m; m = m->next) {
    for (pci_dev = 0; pci_dev < pci_device_list->count; pci_dev++) {
      s_pci_device *pci_device =
	&pci_device_list->pci_device[pci_dev];
      sid =
	((pci_device->sub_product) << 16 | (pci_device->
					    sub_vendor));
      did = ((pci_device->product << 16) | (pci_device->vendor));
      if (((did ^ m->did) & m->did_mask) == 0 &&
	  ((sid ^ m->sid) & m->sid_mask) == 0 &&
	  pci_device->revision >= m->rid_min
	  && pci_device->revision <= m->rid_max) {
	dprintf("PCI Match: Vendor=%04x Product=%04x Sub_vendor=%04x Sub_Product=%04x Release=%02x\n",
		pci_device->vendor, pci_device->product,
		pci_device->sub_vendor,
		pci_device->sub_product,
		pci_device->revision);
	return m;
      }
    }
  }
  return NULL;
}

int pci_scan(s_pci_bus_list * pci_bus_list, s_pci_device_list * pci_device_list)
{
  unsigned int bus, dev, func, maxfunc;
  uint32_t did, sid;
  uint8_t hdrtype, rid;
  pciaddr_t a;
  int cfgtype;

  pci_device_list->count = 0;

#ifdef DEBUG
  outl(~0, 0xcf8);
  printf("Poking at port CF8 = %#08x\n", inl(0xcf8));
  outl(0, 0xcf8);
#endif

  cfgtype = pci_set_config_type(PCI_CFG_AUTO);
  (void)cfgtype;

  dprintf("PCI configuration type %d\n", cfgtype);
  printf("Scanning PCI Buses\n");

  for (bus = 0; bus <= 0xff; bus++) {

    dprintf("Probing bus 0x%02x... \n", bus);

    pci_bus_list->pci_bus[bus].id = bus;
    pci_bus_list->pci_bus[bus].pci_device_count = 0;
    pci_bus_list->count = 0;;

    for (dev = 0; dev <= 0x1f; dev++) {
      maxfunc = 0;
      for (func = 0; func <= maxfunc; func++) {
	a = pci_mkaddr(bus, dev, func, 0);

	did = pci_readl(a);

	if (did == 0xffffffff || did == 0xffff0000 ||
	    did == 0x0000ffff || did == 0x00000000)
	  continue;

	hdrtype = pci_readb(a + 0x0e);

	if (hdrtype & 0x80)
	  maxfunc = 7;	/* Multifunction device */

	rid = pci_readb(a + 0x08);
	sid = pci_readl(a + 0x2c);
	s_pci_device *pci_device =
	  &pci_device_list->
	  pci_device[pci_device_list->count];
	pci_device->product = did >> 16;
	pci_device->sub_product = sid >> 16;
	pci_device->vendor = (did << 16) >> 16;
	pci_device->sub_vendor = (sid << 16) >> 16;
	pci_device->revision = rid;
	pci_device_list->count++;
	dprintf
	  ("Scanning: BUS %02x DID %08x (%04x:%04x) SID %08x RID %02x\n",
	   bus, did, did >> 16, (did << 16) >> 16,
	   sid, rid);
	/* Adding the detected pci device to the bus */
	pci_bus_list->pci_bus[bus].
	  pci_device[pci_bus_list->pci_bus[bus].
		     pci_device_count] = pci_device;
	pci_bus_list->pci_bus[bus].pci_device_count++;
      }
    }
  }

  /* Detecting pci buses that have pci devices connected */
  for (bus = 0; bus <= 0xff; bus++) {
    if (pci_bus_list->pci_bus[bus].pci_device_count > 0) {
      pci_bus_list->count++;
    }
  }
  return 0;
}