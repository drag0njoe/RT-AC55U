#ifndef GPIO_H
#define GPIO_H

enum gpio_idx_e {
	RST_BTN = 0,
	WPS_BTN,
	PWR_LED,
	WPS_LED,
	WIFI_2G_LED,
	WIFI_5G_LED,
#if defined(MAPAC1750)
	WIFI_B_LED,
	WIFI_G_LED,
	WIFI_R_LED,
#endif
	USB_LED,
#if defined(RTAC55U)
	USB3_LED,
	WAN_RED_LED,
#endif
	WAN_BLUE_LED,
	LAN_LED,

#if defined(RTAC55U)
	USB3_POWER,
#endif
#if defined(RT4GAC55U)
	WIFI_BTN,
	LTE_BTN,
	LTE_LED,
	SIG1_LED,
	SIG2_LED,
	SIG3_LED,
#elif (defined(PLN12) || defined(PLAC56))
	WIFI_2G_GREEN_LED,
	WIFI_2G_ORANGE_LED,
	WIFI_2G_RED_LED,
	WIFI_5G_GREEN_LED,
	WIFI_5G_ORANGE_LED,
	WIFI_5G_RED_LED,
#elif defined(RPAC66)
	PWR_GREEN_LED,
	PWR_ORANGE_LED,
	WIFI_2G_GREEN_LED,
	WIFI_2G_BLUE_LED,
	WIFI_2G_RED_LED,
	WIFI_5G_GREEN_LED,
	WIFI_5G_BLUE_LED,
	WIFI_5G_RED_LED,
#endif	/* RT4GAC55U */

	GPIO_IDX_MAX,	/* Last item */
};

extern void led_init(void);
extern void gpio_init(void);
extern void led_onoff(enum gpio_idx_e gpio_idx, int onoff);
extern void power_led_on(void);
extern void power_led_off(void);
extern void leds_on(void);
extern void leds_off(void);
extern void all_leds_on(void);
extern void all_leds_off(void);
#if defined(MAPAC1750)
extern void blue_led_on(void);
extern void green_led_on(void);
extern void red_led_on(void);
extern void purple_led_on(void);
#endif
extern unsigned long DETECT(void);
extern unsigned long DETECT_WPS(void);

#if defined(ALL_LED_OFF)
extern void enable_all_leds(void);
extern void disable_all_leds(void);
#else
static inline void enable_all_leds(void) { }
static inline void disable_all_leds(void) { }
#endif

#if defined(HAVE_WAN_RED_LED)
static inline void wan_red_led_on(void)
{
	led_onoff(WAN_RED_LED, 1);
}

static inline void wan_red_led_off(void)
{
	led_onoff(WAN_RED_LED, 0);
}
#else
static inline void wan_red_led_on(void) { }
static inline void wan_red_led_off(void) { }
#endif

#if defined(RESCUE_BLINK_WIFI_LED)
static inline void wifi_led_on(void)
{
	led_onoff(WIFI_2G_GREEN_LED, 1);
	led_onoff(WIFI_2G_ORANGE_LED, 1);
	led_onoff(WIFI_2G_RED_LED, 1);
	led_onoff(WIFI_5G_GREEN_LED, 1);
	led_onoff(WIFI_5G_ORANGE_LED, 1);
	led_onoff(WIFI_5G_RED_LED, 1);
}

static inline void wifi_led_off(void)
{
	led_onoff(WIFI_2G_GREEN_LED, 0);
	led_onoff(WIFI_2G_ORANGE_LED, 0);
	led_onoff(WIFI_2G_RED_LED, 0);
	led_onoff(WIFI_5G_GREEN_LED, 0);
	led_onoff(WIFI_5G_ORANGE_LED, 0);
	led_onoff(WIFI_5G_RED_LED, 0);
}
#endif

#endif
