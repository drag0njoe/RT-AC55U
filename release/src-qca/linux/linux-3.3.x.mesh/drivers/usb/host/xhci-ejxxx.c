#include <linux/pci.h>

#include "xhci.h"
#include "xhci-ej168v0.0510.c"
#include "xhci-ej168v0.0660.c"

void xhci_init_ejxxx(struct xhci_hcd *xhci)
{
	struct usb_hcd *hcd = xhci_to_hcd(xhci);
	struct pci_dev *pdev = to_pci_dev(hcd->self.controller);
	u8 reg8 = 0;
	u32 reg32 = 0;

	switch (xhci->hcc_params1 & 0xff) {
	case 0x00:
		xhci_warn(xhci, "xHCI chip: EJ168V1\n");
		xhci_init_ej168_v00510(xhci);

		pci_read_config_byte(pdev, 0x44, &reg8);
		reg8 |= 0x01;
		pci_write_config_byte(pdev, 0x44, reg8);
		pci_read_config_byte(pdev, 0xec, &reg8);
		reg8 |= 0x04;
		pci_write_config_byte(pdev, 0xec, reg8);
		pci_write_config_dword(pdev, 0x68, 0x198001c1);
		mdelay(1);
		pci_write_config_dword(pdev, 0x68, 0x18005000);
		pci_write_config_dword(pdev, 0x68, 0x190001d5);
		mdelay(1);
		pci_write_config_dword(pdev, 0x68, 0x18005000);
		break;
	case 0x30:
		xhci_warn(xhci, "xHCI chip: EJ168V2\n");
		xhci_init_ej168_v00660(xhci);

		reg32 = xhci_readl(xhci, hcd->regs + 0x40c0);
		reg32 = (reg32 & 0xffff00ff) | 0x0100;
		xhci_writel(xhci, reg32, hcd->regs + 0x40c0);
		reg32 = xhci_readl(xhci, hcd->regs + 0x40d4);
		reg32 = (reg32 & 0xfffffffe) | 0x01;
		xhci_writel(xhci, reg32, hcd->regs + 0x40d4);
		break;
	default:
		break;
	}

	pci_read_config_dword(pdev, 0xa8, &reg32);
	reg32 &= 0xffff8fff;
	pci_write_config_dword(pdev, 0xa8, reg32);
}

int xhci_gpio_init(struct xhci_hcd *xhci, int gpio, int mode, int data)
{
	struct usb_hcd *hcd = xhci_to_hcd(xhci);
	struct pci_dev *pdev = to_pci_dev(hcd->self.controller);
	u32 gpio_mode, gpio_ctrl;
	u32 temp = 0, mask;
	int ret = 0;

	if (gpio < 0 || gpio > XHCI_GPIO_PIN3)
		return -EINVAL;
	if (mode < 0 || mode > XHCI_GPIO_MODE_OUTPUT)
		return -EINVAL;
	if (data < 0 || data > XHCI_GPIO_HIGH)
		return -EINVAL;

	pci_read_config_dword(pdev, 0x44, &temp);
	temp |= 0x01;
	pci_write_config_dword(pdev, 0x44, temp);
	pci_write_config_dword(pdev, 0x68, 0x18005020);
	mdelay(1);
	pci_read_config_dword(pdev, 0x6c, &temp);
	if (temp == 0xffffffff) {
		ret = -EIO;
		goto error;
	}

	gpio_ctrl = (temp & 0xff) << 16;
	pci_write_config_dword(pdev, 0x68, 0x18005022);
	mdelay(1);
	pci_read_config_dword(pdev, 0x6c, &temp);
	if (temp == 0xffffffff) {
		ret = -EIO;
		goto error;
	}

	gpio_mode = temp & 0x00ff0000;
	switch (mode) {
	case XHCI_GPIO_MODE_DISABLE:
		xhci->gpio_mode &= ~(0x03 << (2 * gpio));
		xhci->gpio_mode |= XHCI_GPIO_MODE_DISABLE << (2 * gpio);

		mask = 0x03 << (2 * gpio + 16);
		gpio_mode = gpio_mode & ~mask;
		gpio_mode |= 0x19005022;
		pci_write_config_dword(pdev, 0x68, gpio_mode);
		mdelay(1);
		break;
	case XHCI_GPIO_MODE_INPUT:
		xhci->gpio_mode &= ~(0x03 << (2 * gpio));
		xhci->gpio_mode |= XHCI_GPIO_MODE_INPUT << (2 * gpio);

		mask = 0x03 << (2 * gpio + 16);
		temp = 0x02 << (2 * gpio + 16);
		gpio_ctrl = (gpio_ctrl & ~mask) | temp;
		gpio_ctrl |= 0x19005020;
		pci_write_config_dword(pdev, 0x68, gpio_ctrl);
		mdelay(1);

		mask = 0x03 << (2 * gpio + 16);
		temp = 0x01 << (2 * gpio + 16);
		gpio_mode = (gpio_mode & ~mask) | temp;
		gpio_mode |= 0x19005022;
		pci_write_config_dword(pdev, 0x68, gpio_mode);
		mdelay(1);
		break;
	case XHCI_GPIO_MODE_OUTPUT:
		xhci->gpio_mode &= ~(0x03 << (2 * gpio));
		xhci->gpio_mode |= XHCI_GPIO_MODE_OUTPUT << (2 * gpio);

		mask = 0x03 << (2 * gpio + 16);
		temp = (data == XHCI_GPIO_HIGH) ? 0x01 : 0x00;
		temp = temp << (2 * gpio + 16);
		gpio_ctrl = (gpio_ctrl & ~mask) | temp;
		gpio_ctrl |= 0x19005020;
		pci_write_config_dword(pdev, 0x68, gpio_ctrl);
		mdelay(1);

		mask = 0x03 << (2 * gpio + 16);
		temp = 0x01 << (2 * gpio + 16);
		gpio_mode = (gpio_mode & ~mask) | temp;
		gpio_mode |= 0x19005022;
		pci_write_config_dword(pdev, 0x68, gpio_mode);
		mdelay(1);
		break;
	default:
		break;
	}

error:
	return ret;
}

int xhci_get_gpio_mode(struct xhci_hcd *xhci, int gpio)
{
	if (gpio < 0 || gpio > XHCI_GPIO_PIN3)
		return -EINVAL;
	else
		return (xhci->gpio_mode >> (2 * gpio)) & 0x03;
}

int xhci_gpio_set(struct xhci_hcd *xhci, int gpio, int data)
{
	struct usb_hcd *hcd = xhci_to_hcd(xhci);
	struct pci_dev *pdev = to_pci_dev(hcd->self.controller);
	u32 gpio_ctrl;
	u32 temp = 0, mask;
	int ret = 0;

	if (gpio < 0 || gpio > XHCI_GPIO_PIN3)
		return -EINVAL;
	if (data < 0 || data > XHCI_GPIO_HIGH)
		return -EINVAL;
	if (xhci_get_gpio_mode(xhci, gpio) != XHCI_GPIO_MODE_OUTPUT)
		return -EINVAL;

	pci_read_config_dword(pdev, 0x44, &temp);
	temp |= 0x01;
	pci_write_config_dword(pdev, 0x44, temp);
	pci_write_config_dword(pdev, 0x68, 0x18005020);
	mdelay(1);
	pci_read_config_dword(pdev, 0x6c, &temp);
	if (temp == 0xffffffff) {
		ret = -EIO;
		goto error;
	}

	gpio_ctrl = (temp & 0xff) << 16;
	mask = 0x03 << (2 * gpio + 16);
	temp = (data == XHCI_GPIO_HIGH) ? 0x01 : 0x00;
	temp = temp << (2 * gpio + 16);
	gpio_ctrl = (gpio_ctrl & ~mask) | temp;
	gpio_ctrl |= 0x19005020;
	pci_write_config_dword(pdev, 0x68, gpio_ctrl);
	mdelay(1);

error:
	return ret;
}

