/******************************************************************************
* Filename : gpio.c
* This part is used to control LED and detect button-press
******************************************************************************/

#include <common.h>
#include <command.h>
#include <gpio.h>
#include <atheros.h>

#define INVALID_GPIO_NR	0xFFFFFFFF

#define GPIO_OE		0x18040000
#define GPIO_IN		0x18040004
#define GPIO_OUT	0x18040008
#define GPIO_SET	0x1804000C
#define GPIO_CLEAR	0x18040010
#define GPIO_OUT_FUNC0	0x1804002C
#define GPIO_FUNCTION	0x1804006C

#define LED_ON 1
#define LED_OFF 0
/* RT-AC55U LED/BTN definitions */
static const struct gpio_s {
	unsigned int	gpio_nr;	/* GPIO# */
	unsigned int	active_low;	/* low active if non-zero */
	unsigned int	dir;		/* direction. 0: output; 1: input */
	unsigned int	def_onoff;		/* default value of LEDs */
	unsigned int	is_led;		/* 0: NOT LED; 1: is LED */
	char		*name;
} gpio_tbl[] = {
#if defined(RTAC55U)
	[RST_BTN] =		{ 17, 1, 1, 0      , 0, "Reset button" },	/* GPIO17, Low  active, input  */
	[WPS_BTN] =		{ 16, 1, 1, 0      , 0, "WPS button" },		/* GPIO16, Low  active, input  */
	[WPS_LED] =		{ 20, 1, 0, LED_OFF, 1, "WPS LED" },		/* GPIO20, Low  active, output */
	[PWR_LED] =		{ 19, 1, 0, LED_ON , 1, "Power LED" },		/* GPIO19, Low  active, output */
	[WIFI_2G_LED] =		{ 13, 0, 0, LED_OFF, 1, "WiFi 2G LED" },	/* GPIO13, High active, output */
	[USB_LED] =		{  4, 0, 0, LED_OFF, 1, "USB 2.0 LED" },	/* GPIO4,  High active, output */
	[WAN_RED_LED] =		{ 14, 0, 0, LED_OFF, 1, "WAN Red LED" },	/* GPIO14, High active, output */
	[WAN_BLUE_LED] =	{ 15, 0, 0, LED_OFF, 1, "WAN Blue LED" },	/* GPIO15, High active, output */
	[LAN_LED] =		{ 18, 1, 0, LED_OFF, 1, "LAN LED" },		/* GPIO18, Low  active, output */
#if !defined(RTAC55U_SR1)
	[WIFI_5G_LED] =		{  0, 0, 0, LED_OFF, 1, "WiFi 5G LED" },	/* GPIO0,  High active, output */
	[USB3_LED] =		{  1, 0, 0, LED_OFF, 1, "USB3 LED" },		/* GPIO1,  High active, output */
	[USB3_POWER] =		{ 21, 0, 0, 1      , 0, "USB3 POWER" },		/* GPIO21, High active, output */
#endif
#elif defined(RT4GAC55U)
	/* BTN definition */
	[WIFI_BTN] =		{  1, 1, 1, 0      , 0, "WiFi button" },	/* GPIO1,  Low  active, input  */
	[WPS_BTN] =		{ 16, 1, 1, 0      , 0, "WPS button" },		/* GPIO16, Low  active, input  */
	[RST_BTN] =		{ 17, 1, 1, 0      , 0, "Reset button" },	/* GPIO17, Low  active, input  */
	[LTE_BTN] =		{ 21, 1, 1, 0      , 0, "LTE button" },		/* GPIO21, Low  active, input  */
	/* LED definition */
#if defined(RTAC55U_SR1)
	[WPS_LED] =		{ 19, 1, 0, LED_OFF, 1, "WPS LED" },		/* GPIO19, Low  active, output */
	[PWR_LED] =		{ 19, 1, 0, LED_ON , 1, "Power LED" },		/* GPIO19, Low  active, output */
	[WIFI_2G_LED] =		{ 13, 1, 0, LED_OFF, 1, "WiFi 2G LED" },	/* GPIO13, Low  active, output */
	[WIFI_5G_LED] =		{ 12, 1, 0, LED_OFF, 1, "WiFi 5G LED" },	/* GPIO12, Low  active, output */
	[USB_LED] =		{  4, 1, 0, LED_OFF, 1, "USB 2.0 LED" },	/* GPIO4,  Low  active, output */
	[WAN_BLUE_LED] =	{ 15, 1, 0, LED_OFF, 1, "WAN LED" },		/* GPIO15, Low  active, output */
	[LAN_LED] =		{ 14, 1, 0, LED_OFF, 1, "LAN LED" },		/* GPIO14, Low  active, output */
	[LTE_LED] =		{ 20, 1, 0, LED_OFF, 1, "LTE LED" },		/* GPIO20, Low  active, output */
	[SIG1_LED] =		{ 22, 1, 0, LED_OFF, 1, "Signal 1 LED" },	/* GPIO22, Low  active, output */
	[SIG2_LED] =		{ 23, 1, 0, LED_OFF, 1, "Signal 2 LED" },	/* GPIO23, Low  active, output */
	[SIG3_LED] =		{ 18, 1, 0, LED_OFF, 1, "Signal 3 LED" },	/* GPIO18, Low  active, output */
#else	/* RT4GAC55U_SR2 */
	[WPS_LED] =		{ 18, 1, 0, LED_OFF, 1, "WPS LED" },		/* GPIO18, Low  active, output */
	[PWR_LED] =		{ 18, 1, 0, LED_ON , 1, "Power LED" },		/* GPIO18, Low  active, output */
	[LTE_LED] =		{ 23, 1, 0, LED_OFF, 1, "LTE LED" },		/* GPIO23, Low  active, output */
	[WAN_BLUE_LED] =	{ 22, 1, 0, LED_OFF, 1, "WAN LED" },		/* GPIO22, Low  active, output */
	[WIFI_5G_LED] =		{  4, 1, 0, LED_OFF, 1, "WiFi 5G LED" },	/* GPIO4 , Low  active, output */
	[WIFI_2G_LED] =		{ 14, 1, 0, LED_OFF, 1, "WiFi 2G LED" },	/* GPIO14, Low  active, output */
	[LAN_LED] =		{ 13, 1, 0, LED_OFF, 1, "LAN LED" },		/* GPIO13, Low  active, output */
	[USB_LED] =		{ 12, 1, 0, LED_OFF, 1, "USB 2.0 LED" },	/* GPIO12, Low  active, output */
	[SIG3_LED] =		{ 15, 1, 0, LED_OFF, 1, "Signal 3 LED" },	/* GPIO15, Low  active, output */
	[SIG2_LED] =		{ 20, 1, 0, LED_OFF, 1, "Signal 2 LED" },	/* GPIO20, Low  active, output */
	[SIG1_LED] =		{ 19, 1, 0, LED_OFF, 1, "Signal 1 LED" },	/* GPIO19, Low  active, output */
#endif	/* RTAC55U_SR */
#elif defined(PLN12)
	[RST_BTN] =		{ 	       15, 0, 1, 0      , 0, "Reset button" },		/* GPIO15, High active, input  */
	[WPS_BTN] =		{ 	       11, 1, 1, 0      , 0, "WPS button" },		/* GPIO11, Low  active, input  */
	[PWR_LED] =		{ 	        4, 1, 0, LED_OFF, 1, "Power LED" },		/* GPIO4,  Low  active, output */
	[WPS_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WPS LED" },		/* invalid define */
	[WIFI_2G_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WiFi 2G LED" },		/* invalid define */
	[WIFI_5G_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WiFi 5G LED" },		/* invalid define */
	[USB_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "USB 2.0 LED" },		/* invalid define */
	[WAN_BLUE_LED] =	{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WAN LED" },		/* invalid define */
	[LAN_LED] =		{ 	       16, 1, 0, LED_OFF, 1, "LAN LED" },		/* GPIO16, Low  active, output */
	[WIFI_2G_GREEN_LED] =	{ 	       12, 1, 0, LED_OFF, 1, "WiFi 2G Green LED" },	/* GPIO12, Low  active, output */
	[WIFI_2G_ORANGE_LED] =	{ 	       14, 1, 0, LED_OFF, 1, "WiFi 2G Orange LED" },	/* GPIO14, Low  active, output */
	[WIFI_2G_RED_LED] =	{ 	       17, 1, 0, LED_OFF, 1, "WiFi 2G Red LED" },	/* GPIO17, Low  active, output */
	[WIFI_5G_GREEN_LED] =	{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WiFi 5G Green LED" },	/* invalid define */
	[WIFI_5G_ORANGE_LED] =	{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WiFi 5G Orange LED" },	/* invalid define */
	[WIFI_5G_RED_LED] =	{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WiFi 5G Red LED" },	/* invalid define */
#elif defined(PLAC56)
	[RST_BTN] =		{ 		2, 1, 1, 0      , 0, "Reset button" },		/* GPIO2, Low active, input  */
	[WPS_BTN] =		{ 		1, 1, 1, 0      , 0, "WPS button" },		/* GPIO1, Low active, input  */
	[PWR_LED] =		{ 	       15, 1, 0, LED_OFF, 1, "Power LED" },		/* GPIO15, Low active, output */
	[WPS_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WPS LED" },		/* invalid define */
	[WIFI_2G_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WiFi 2G LED" },		/* invalid define */
	[WIFI_5G_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WiFi 5G LED" },		/* invalid define */
	[USB_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "USB 2.0 LED" },		/* invalid define */
	[WAN_BLUE_LED] =	{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WAN LED" },		/* invalid define */
	[LAN_LED] =		{ 		6, 1, 0, LED_OFF, 1, "LAN LED" },		/* GPIO6,  Low active, output */
	[WIFI_2G_GREEN_LED] =	{ 	       19, 1, 0, LED_OFF, 1, "WiFi 2G Green LED" },	/* GPIO19, Low active, output */
	[WIFI_2G_ORANGE_LED] =	{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WiFi 2G Orange LED" },	/* invalid define */
	[WIFI_2G_RED_LED] =	{ 	       20, 1, 0, LED_OFF, 1, "WiFi 2G Red LED" },	/* GPIO20, Low active, output */
	[WIFI_5G_GREEN_LED] =	{ 	        8, 1, 0, LED_OFF, 1, "WiFi 5G Green LED" },	/* GPIO8,  Low active, output */
	[WIFI_5G_ORANGE_LED] =	{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WiFi 5G Orange LED" },	/* invalid define */
	[WIFI_5G_RED_LED] =	{ 	        7, 1, 0, LED_OFF, 1, "WiFi 5G Red LED" },	/* GPIO7,  Low active, output */
#elif defined(PLAC66U)
	[RST_BTN] =		{ 		2, 1, 1, 0      , 0, "Reset button" },	/* GPIO2, Low active, input  */
	[WPS_BTN] =		{ 		1, 1, 1, 0      , 0, "WPS button" },	/* GPIO1, Low active, input  */
	[PWR_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "Power LED" },	/* invalid define */
	[WPS_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WPS LED" },	/* invalid define */
	[WIFI_2G_LED] =		{ 		5, 1, 0, LED_OFF, 1, "WiFi 2G LED" },	/* GPIO5, Low active, output */
	[WIFI_5G_LED] =		{ 		6, 1, 0, LED_OFF, 1, "WiFi 5G LED" },	/* GPIO6, Low active, output */
	[USB_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "USB 2.0 LED" },	/* invalid define */
	[WAN_BLUE_LED] =	{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WAN LED" },	/* invalid define */
	[LAN_LED] =		{ 		7, 1, 0, LED_OFF, 1, "LAN LED" },	/* GPIO7, Low active, output */
#elif defined(RPAC66)
	[RST_BTN] =		{ 		2, 1, 1, 0      , 0, "Reset button" },		/* GPIO2, Low active, input  */
	[WPS_BTN] =		{ 		5, 1, 1, 0      , 0, "WPS button" },		/* GPIO5, Low active, input  */
	[PWR_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "Power LED" },		/* invalid define */
	[WPS_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WPS LED" },		/* invalid define */
	[WIFI_2G_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WiFi 2G LED" },		/* invalid define */
	[WIFI_5G_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WiFi 5G LED" },		/* invalid define */
	[USB_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "USB 2.0 LED" },		/* invalid define */
	[WAN_BLUE_LED] =	{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WAN LED" },		/* invalid define */
	[LAN_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "LAN LED" },		/* invalid define */
	[PWR_GREEN_LED] =	{ 	       6, 0, 0, LED_OFF, 1, "Power Green LED" },	/* GPIO6, High active, output */
	[PWR_ORANGE_LED] =	{ 	       1, 0, 0, LED_OFF, 1, "Power Orange LED" },	/* GPIO1, High active, output */
	[WIFI_2G_GREEN_LED] =	{ 	       8, 0, 0, LED_OFF, 1, "WiFi 2G Green LED" },	/* GPIO8, High active, output */
	[WIFI_2G_BLUE_LED] =	{ 	       7, 0, 0, LED_OFF, 1, "WiFi 2G Blue LED" },	/* GPIO7, High active, output */
	[WIFI_2G_RED_LED] =	{ 	       9, 0, 0, LED_OFF, 1, "WiFi 2G Red LED" },	/* GPIO9, High active, output */
	[WIFI_5G_GREEN_LED] =	{ 	        15, 0, 0, LED_OFF, 1, "WiFi 5G Green LED" },	/* GPIO15,  High active, output */
	[WIFI_5G_BLUE_LED] =	{ 	        14, 0, 0, LED_OFF, 1, "WiFi 5G Blue LED" },	/* GPIO14,  High active, output */
	[WIFI_5G_RED_LED] =	{ 	        16, 0, 0, LED_OFF, 1, "WiFi 5G Red LED" },	/* GPIO16,  High active, output */
#elif defined(MAPAC1750)
	[RST_BTN] =		{ 		2, 1, 1, 0      , 0, "Reset button" },	/* GPIO2, Low active, input  */
	[WPS_BTN] =		{ 		5, 1, 1, 0      , 0, "WPS button" },	/* GPIO5, Low active, input  */
	[PWR_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "Power LED" },	/* invalid define */
	[WPS_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WPS LED" },	/* invalid define */
	[WIFI_2G_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WiFi 2G LED" },	/* invalid define */
	[WIFI_5G_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WiFi 5G LED" },	/* invalid define */
	[WIFI_B_LED] =		{ 	       14, 0, 0, LED_ON , 1, "WiFi Blue LED" },	/* GPIO14, High active, output */
	[WIFI_G_LED] =		{ 	       15, 0, 0, LED_OFF, 1, "WiFi Green LED" },/* GPIO15, High active, output */
	[WIFI_R_LED] =		{ 	       16, 0, 0, LED_OFF, 1, "WiFi Red LED" },	/* GPIO16, High active, output */
	[USB_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "USB 2.0 LED" },	/* invalid define */
	[WAN_BLUE_LED] =	{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "WAN LED" },	/* invalid define */
	[LAN_LED] =		{ INVALID_GPIO_NR, 1, 0, LED_OFF, 1, "LAN LED" },	/* invalid define */
#else
#error Unknown model
#endif
};

/* Get real GPIO# of gpio_idx
 * @return:	GPIO#
 */
static unsigned int get_gpio_nr(enum gpio_idx_e gpio_idx)
{
	if (gpio_idx < 0 || gpio_idx >= GPIO_IDX_MAX) {
		printf("%s: Invalid GPIO index %d/%d\n", __func__, gpio_idx, GPIO_IDX_MAX);
		return INVALID_GPIO_NR;
	}

	return gpio_tbl[gpio_idx].gpio_nr;
}

/* Whether gpio_idx is active low or not
 * @return:	1:	active low
 * 		0:	active high
 */
static unsigned int get_gpio_active_low(enum gpio_idx_e gpio_idx)
{
	if (gpio_idx < 0 || gpio_idx >= GPIO_IDX_MAX)
		return INVALID_GPIO_NR;

	return !!(gpio_tbl[gpio_idx].active_low);
}

/* Set GPIO# as GPIO PIN and direction.
 * @gpio_nr:	GPIO#
 * @dir:	GPIO direction
 * 		0: output
 * 		1: input.
 */
static void __qca955x_set_gpio_dir(enum gpio_idx_e gpio_idx, int dir)
{
	unsigned int gpio_nr = get_gpio_nr(gpio_idx), mask = 0, reg, shift;

	if (gpio_nr == INVALID_GPIO_NR)
		return;

#if defined(CONFIG_MACH_QCA956x)
	if (gpio_nr >= 14 && gpio_nr <= 17)
#else /* CONFIG_MACH_QCA953x || CONFIG_MACH_QCA955x */
	if (gpio_nr <= 3)
#endif
	{
		/* Disable JTAG */
		ath_reg_rmw_set(GPIO_FUNCTION, 1U << 1);
	}

	if (gpio_nr <= 23) {
		reg = GPIO_OUT_FUNC0 + ((gpio_nr >> 2) << 2);	/* GPIO_OUT_FUNCTION0~5 */
		shift = (gpio_nr % 4) << 3;
		ath_reg_rmw_clear(reg, 0xFFU << shift);
	}

	mask = 1U << gpio_nr;
	if (!dir) {
		/* output */
		ath_reg_rmw_clear(GPIO_OE, mask);
	} else {
		/* input */
		ath_reg_rmw_set(GPIO_OE, mask);
	}
}

/* Set raw value to GPIO#
 * @gpio_nr:	GPIO#
 * @val:	GPIO direction
 * 		0: low-level voltage
 * 		1: high-level voltage
 */
static void __qca955x_set_gpio_pin(enum gpio_idx_e gpio_idx, int val)
{
	unsigned int gpio_nr = get_gpio_nr(gpio_idx), mask = 0, reg;

	if (gpio_nr == INVALID_GPIO_NR)
		return;

	mask = 1U << gpio_nr;
	if (!val) {
		/* output 0 */
		reg = GPIO_CLEAR;
	} else {
		/* output 1 */
		reg = GPIO_SET;
	}

#if (defined(CONFIG_MACH_QCA953x) || defined(CONFIG_MACH_QCA956x))
	ath_reg_wr_nf(reg, mask);
#else /* CONFIG_MACH_QCA955x */
	ath_reg_rmw_set(reg, mask);
#endif
}

/* Read raw value of GPIO#
 * @gpio_nr:	GPIO#
 * @return:
 * 		0: low-level voltage
 * 		1: high-level voltage
 */
static int __qca955x_get_gpio_pin(enum gpio_idx_e gpio_idx)
{
	unsigned int gpio_nr = get_gpio_nr(gpio_idx);

	if (gpio_nr == INVALID_GPIO_NR)
		return 0;

	return !!(ath_reg_rd(GPIO_IN) & (1U << gpio_nr));
}

/* Check button status. (high/low active is handled in this function)
 * @return:	1: key is pressed
 * 		0: key is not pressed
 */
static int check_button(enum gpio_idx_e gpio_idx)
{
	return !!(__qca955x_get_gpio_pin(gpio_idx) ^ get_gpio_active_low(gpio_idx));
}

/* Check button status. (high/low active is handled in this function)
 * @onoff:	1: Turn on LED
 * 		0: Turn off LED
 */
void led_onoff(enum gpio_idx_e gpio_idx, int onoff)
{
	__qca955x_set_gpio_pin(gpio_idx, onoff ^ get_gpio_active_low(gpio_idx));
}

void led_init(void)
{
	int i;

	for (i = 0; i < GPIO_IDX_MAX; i++)
	{
		if (gpio_tbl[i].dir == 0)
		{
			__qca955x_set_gpio_dir(i, 0);
			led_onoff(i, gpio_tbl[i].def_onoff);
		}
	}
}

void gpio_init(void)
{
	printf("ASUS %s gpio init : wps / reset pin\n", model);
	__qca955x_set_gpio_dir(WPS_BTN, 1);
	__qca955x_set_gpio_dir(RST_BTN, 1);
	/* Check serial_init() function to make sure input pins, e.g., GPIO#16,#17, are not re-configured as output pin. */
}

unsigned long DETECT(void)
{
	int key = 0;

	if (check_button(RST_BTN)) {
		key = 1;
		printf("reset buootn pressed!\n");
	}
	return key;
}

unsigned long DETECT_WPS(void)
{
	int key = 0;

#if defined(PLAC66U)
#else
	if (check_button(WPS_BTN)) {
		key = 1;
		printf("wps buootn pressed!\n");
	}
#endif
	return key;
}

void power_led_on(void)
{
	led_onoff(PWR_LED, 1);
}

void power_led_off(void)
{
	led_onoff(PWR_LED, 0);
}


#if defined(RPAC66)
void power_orange_led_on(void)
{
	leds_off();
	led_onoff(PWR_ORANGE_LED, 1);
}

void power_orange_led_off(void)
{
	leds_off();
	led_onoff(PWR_ORANGE_LED, 0);
}
#elif defined(MAPAC1750)
void blue_led_on(void)
{
	led_onoff(WIFI_B_LED, LED_ON);
	led_onoff(WIFI_G_LED, LED_OFF);
	led_onoff(WIFI_R_LED, LED_OFF);
}

void green_led_on(void)
{
	led_onoff(WIFI_B_LED, LED_OFF);
	led_onoff(WIFI_G_LED, LED_ON);
	led_onoff(WIFI_R_LED, LED_OFF);
}

void red_led_on(void)
{
	led_onoff(WIFI_B_LED, LED_OFF);
	led_onoff(WIFI_G_LED, LED_OFF);
	led_onoff(WIFI_R_LED, LED_ON);
}

void purple_led_on(void)
{
	led_onoff(WIFI_B_LED, LED_ON);
	led_onoff(WIFI_G_LED, LED_OFF);
	led_onoff(WIFI_R_LED, LED_ON);
}
#endif
/* Turn on model-specific LEDs */
void leds_on(void)
{
#if defined(RPAC66)
	led_onoff(PWR_GREEN_LED, LED_ON);
	led_onoff(WIFI_2G_GREEN_LED, LED_ON);
	led_onoff(WIFI_5G_GREEN_LED, LED_ON);
#elif defined(MAPAC1750)
	led_onoff(WIFI_B_LED, LED_ON);
	led_onoff(WIFI_G_LED, LED_ON);
	led_onoff(WIFI_R_LED, LED_ON);
#else
	led_onoff(PWR_LED, 1);
	led_onoff(WAN_BLUE_LED, 1);
	led_onoff(LAN_LED, 1);

	/* Don't turn on below LEDs in accordance with PM's request. */
	wan_red_led_off();
	led_onoff(USB_LED, 0);
	led_onoff(WIFI_2G_LED, 0);
	led_onoff(WPS_LED, 0);
#endif
}

/* Turn off model-specific LEDs */
void leds_off(void)
{
#if defined(RPAC66)
	led_onoff(PWR_GREEN_LED, LED_OFF);
	led_onoff(WIFI_2G_GREEN_LED, LED_OFF);
	led_onoff(WIFI_5G_GREEN_LED, LED_OFF);
#elif defined(MAPAC1750)
	led_onoff(WIFI_B_LED, LED_OFF);
	led_onoff(WIFI_G_LED, LED_OFF);
	led_onoff(WIFI_R_LED, LED_OFF);
#else
	led_onoff(PWR_LED, 0);
	led_onoff(WAN_BLUE_LED, 0);
	led_onoff(LAN_LED, 0);
	wan_red_led_off();

	led_onoff(USB_LED, 0);
	led_onoff(WIFI_2G_LED, 0);
	led_onoff(WPS_LED, 0);
#endif
}

/* Turn on all model-specific LEDs */
void all_leds_on(void)
{
	int i;

	for (i = 0; i < GPIO_IDX_MAX; i++)
	{
		if (gpio_tbl[i].is_led == 1)
		{
			led_onoff(i, 1);
		}
	}

	/* WAN RED LED share same position with WAN BLUE LED. Turn on WAN BLUE LED only*/
	wan_red_led_off();
}

/* Turn off all model-specific LEDs */
void all_leds_off(void)
{
	int i;

	for (i = 0; i < GPIO_IDX_MAX; i++)
	{
		if (gpio_tbl[i].is_led == 1)
		{
			led_onoff(i, 0);
		}
	}

	wan_red_led_off();
}

#if defined(ALL_LED_OFF)
void enable_all_leds(void)
{
}

void disable_all_leds(void)
{
}
#endif

#if defined(TEST_GPIO)
int do_test_gpio (cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	const struct gpio_s *p;
	int i, j, stop, old, new, status;
	unsigned int gpio_idx = GPIO_IDX_MAX, gpio_nr = INVALID_GPIO_NR;

	if (argc >= 2) {
		gpio_idx = simple_strtoul(argv[1], 0, 10);
		if (gpio_idx < GPIO_IDX_MAX)
			gpio_nr = get_gpio_nr(gpio_idx);
	}
	if (gpio_idx >= GPIO_IDX_MAX || gpio_nr == INVALID_GPIO_NR) {
		printf("%8s %20s %5s %9s %10s \n", "gpio_idx", "name", "gpio#", "direction", "active low");
		for (i = 0, p = &gpio_tbl[0]; i < ARRAY_SIZE(gpio_tbl); ++i, ++p) {
			printf("%8d %20s %5d %9s %10s \n", i, p->name, p->gpio_nr,
				(!p->dir)?"output":"input", (p->active_low)? "yes":"no");
		}
		return 1;
	}

	p = &gpio_tbl[gpio_idx];
	printf("%s: GPIO index %d GPIO#%d name %s direction %s active_low %s\n",
		p->name, gpio_idx, p->gpio_nr, (!p->dir)?"output":"input", (p->active_low)? "yes":"no");
	printf("Press any key to stop testing ...\n");
	if (!p->dir) {
		/* output */
		for (i = 0, stop = 0; !stop; ++i) {
			printf("%s: %s\n", p->name, (i&1)? "ON":"OFF");
			led_onoff(gpio_idx, i & 1);
			for (j = 0, stop = 0; !stop && j < 40; ++j) {
				udelay(100000);
				if (tstc())
					stop = 1;
			}
		}
	} else {
		/* input */
		for (i = 0, stop = 0; !stop; ++i) {
			new = __qca955x_get_gpio_pin(gpio_idx);
			status = check_button(gpio_idx);
			if (!i || old != new) {
				printf("%s: %d [%s]\n", p->name, new, status? "pressed":"not pressed");
				old = new;
			}
			for (j = 0, stop = 0; !stop && j < 10; ++j) {
				udelay(5000);
				if (tstc())
					stop = 1;
			}
		}
	}

	return 0;
}

U_BOOT_CMD(
    test_gpio, 2, 0, do_test_gpio,
    "test_gpio - Test GPIO.\n",
    "test_gpio [<gpio_idx>] - Test GPIO PIN.\n"
    "                <gpio_idx> is the index of GPIO table.\n"
    "                If gpio_idx is invalid or is not specified,\n"
    "                GPIO table is printed.\n"
);


int do_ledon(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	leds_on();

	return 0;
}

int do_ledoff(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	leds_off();

	return 0;
}

U_BOOT_CMD(
    ledon, 1, 1, do_ledon,
	"ledon\t -set led on\n",
	NULL
);

U_BOOT_CMD(
    ledoff, 1, 1, do_ledoff,
	"ledoff\t -set led off\n",
	NULL
);

#if defined(ALL_LED_OFF)
int do_all_ledon(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	enable_all_leds();

	return 0;
}

int do_all_ledoff(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	disable_all_leds();

	return 0;
}

U_BOOT_CMD(
    all_ledon, 1, 1, do_all_ledon,
	"all_ledon\t -set all_led on\n",
	NULL
);

U_BOOT_CMD(
    all_ledoff, 1, 1, do_all_ledoff,
	"all_ledoff\t -set all_led off\n",
	NULL
);
#endif

#endif	/* DEBUG_LED_GPIO */
