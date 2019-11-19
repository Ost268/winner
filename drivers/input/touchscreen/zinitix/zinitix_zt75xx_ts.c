/*
 *
 * Zinitix zt75xx touchscreen driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 */


#undef TSP_VERBOSE_DEBUG

#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/completion.h>
#include <linux/wakelock.h>

#include <zinitix_zt75xx_ts.h>
#include <linux/input/mt.h>

#if defined(CONFIG_SEC_SYSFS)
#include <linux/sec_sysfs.h>
#elif defined(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#else
extern struct class *sec_class;
#endif

#include <linux/input/sec_cmd.h>
#include <linux/input/sec_tclm_v2.h>
#include <linux/of_gpio.h>
#include <linux/firmware.h>
#ifdef CONFIG_VBUS_NOTIFIER
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#include <linux/vbus_notifier.h>
#endif

#if defined(CONFIG_INPUT_SEC_SECURE_TOUCH)
#include <linux/completion.h>
#include <linux/atomic.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#endif

#if defined(CONFIG_FOLDER_HALL) && defined(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
#include <linux/hall.h>
#endif

#ifdef CONFIG_BATTERY_SAMSUNG
extern unsigned int lpcharge;
#endif

#define CONFIG_INPUT_ENABLED
#define SEC_FACTORY_TEST

#define NOT_SUPPORTED_TOUCH_DUMMY_KEY

//#define GLOVE_MODE

#define MAX_FW_PATH 255
#define TSP_FW_FILENAME "zinitix_fw.bin"

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
#include <linux/trustedui.h>
#endif

#define SUPPORTED_PALM_TOUCH

extern char *saved_command_line;

#define ZINITIX_DEBUG			0
#define PDIFF_DEBUG			1
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
#define USE_MISC_DEVICE
#endif

/* added header file */

#define TOUCH_POINT_MODE		0

#ifdef CONFIG_SEC_GTESY18LTE_PROJECT
#define MAX_SUPPORTED_FINGER_NUM	10 /* max 10 */
#else
#define MAX_SUPPORTED_FINGER_NUM	5 /* max 10 */
#endif

#ifdef NOT_SUPPORTED_TOUCH_DUMMY_KEY
#define MAX_SUPPORTED_BUTTON_NUM	2 /* max 8 */
#define SUPPORTED_BUTTON_NUM		2
#else
#define MAX_SUPPORTED_BUTTON_NUM	6 /* max 8 */
#define SUPPORTED_BUTTON_NUM		2
#endif

/* Upgrade Method*/
#define TOUCH_ONESHOT_UPGRADE		1
/* if you use isp mode, you must add i2c device :
name = "zinitix_isp" , addr 0x50*/

/* resolution offset */
#define ABS_PT_OFFSET			(-1)

#define TOUCH_FORCE_UPGRADE		1
#define USE_CHECKSUM			1
#define CHECK_HWID			0

#define CHIP_OFF_DELAY			50 /*ms*/
#define CHIP_ON_DELAY			50 /*ms*/
#define FIRMWARE_ON_DELAY		150 /*ms*/

#define DELAY_FOR_SIGNAL_DELAY		30 /*us*/
#define DELAY_FOR_TRANSCATION		50
#define DELAY_FOR_POST_TRANSCATION	10

enum power_control {
	POWER_OFF,
	POWER_ON,
	POWER_ON_SEQUENCE,
};

/* Key Enum */
enum key_event {
	ICON_BUTTON_UNCHANGE,
	ICON_BUTTON_DOWN,
	ICON_BUTTON_UP,
};

/* ESD Protection */
/*second : if 0, no use. if you have to use, 3 is recommended*/
#define ESD_TIMER_INTERVAL			1
#define SCAN_RATE_HZ				5000
#define CHECK_ESD_TIMER				7

#define TOUCH_PRINT_INFO_DWORK_TIME		30000 /* 30s */

/*Test Mode (Monitoring Raw Data) */
#define TSP_INIT_TEST_RATIO		100

#define	SEC_MUTUAL_AMP_V_SEL		0x0232

#define	SEC_DND_N_COUNT			11
#define	SEC_DND_U_COUNT			16
#define	SEC_DND_FREQUENCY		139

#define	SEC_HFDND_N_COUNT		11
#define	SEC_HFDND_U_COUNT		16
#define	SEC_HFDND_FREQUENCY		104

#define	SEC_SX_AMP_V_SEL		0x0434
#define	SEC_SX_SUB_V_SEL		0x0055
#define	SEC_SY_AMP_V_SEL		0x0232
#define	SEC_SY_SUB_V_SEL		0x0022
#define	SEC_SHORT_N_COUNT		2
#define	SEC_SHORT_U_COUNT		1

#define SEC_SY_SAT_FREQUENCY		200
#define SEC_SY_SAT_N_COUNT		9
#define SEC_SY_SAT_U_COUNT		9
#define SEC_SY_SAT_RS0_TIME		0x00FF
#define SEC_SY_SAT_RBG_SEL		0x0404
#define SEC_SY_SAT_AMP_V_SEL		0x0434
#define SEC_SY_SAT_SUB_V_SEL		0x0044

#define SEC_SY_SAT2_FREQUENCY		200
#define SEC_SY_SAT2_N_COUNT		9
#define SEC_SY_SAT2_U_COUNT		3
#define SEC_SY_SAT2_RS0_TIME		0x00FF
#define SEC_SY_SAT2_RBG_SEL		0x0404
#define SEC_SY_SAT2_AMP_V_SEL		0x0434
#define SEC_SY_SAT2_SUB_V_SEL		0x0011

#define MAX_RAW_DATA_SZ			792 /* 36x22 */
#define MAX_TRAW_DATA_SZ		(MAX_RAW_DATA_SZ + 4*MAX_SUPPORTED_FINGER_NUM + 2)

#define RAWDATA_DELAY_FOR_HOST		10000

struct raw_ioctl {
	u32 sz;
	u32 buf;
};

struct reg_ioctl {
	u32 addr;
	u32 val;
};

#define TOUCH_SEC_MODE				48
#define TOUCH_REF_MODE				10
#define TOUCH_NORMAL_MODE			5
#define TOUCH_DELTA_MODE			3
//#define TOUCH_SDND_MODE				6
#define TOUCH_REFERENCE_MODE			8
#define TOUCH_DND_MODE				11
#define TOUCH_HFDND_MODE			12
#define TOUCH_TXSHORT_MODE			13
#define TOUCH_RXSHORT_MODE			14
#define TOUCH_JITTER_MODE			15
#define TOUCH_SELF_DND_MODE			17
#define TOUCH_SENTIVITY_MEASUREMENT_MODE	21
#define TOUCH_REF_ABNORMAL_TEST_MODE		33
#define DEF_RAW_SELF_SSR_DATA_MODE		39	/* SELF SATURATION RX */
#define DEF_RAW_SELF_SFR_UNIT_DATA_MODE		40

/*  Other Things */
#define INIT_RETRY_CNT				3
#define I2C_SUCCESS				0
#define I2C_FAIL				1

/*---------------------------------------------------------------------*/

/* chip code */
#define BT43X_CHIP_CODE		0xE200
#define BT53X_CHIP_CODE		0xF400
#define ZT7548_CHIP_CODE	0xE548
#define ZT7538_CHIP_CODE	0xE538
#define ZT7532_CHIP_CODE	0xE532
#define ZT7554_CHIP_CODE	0xE700

/* Register Map*/
#define ZT75XX_SWRESET_CMD			0x0000
#define ZT75XX_WAKEUP_CMD			0x0001

#define ZT75XX_IDLE_CMD				0x0004
#define ZT75XX_SLEEP_CMD			0x0005

#define ZT75XX_CLEAR_INT_STATUS_CMD		0x0003
#define ZT75XX_CALIBRATE_CMD			0x0006
#define ZT75XX_SAVE_STATUS_CMD			0x0007
#define ZT75XX_SAVE_CALIBRATION_CMD		0x0008
#define ZT75XX_RECALL_FACTORY_CMD		0x000f

#define ZT75XX_THRESHOLD			0x0020

#define ZT75XX_DEBUG_REG			0x0115

#define ZT75XX_TOUCH_MODE			0x0010
#define ZT75XX_CHIP_REVISION			0x0011
#define ZT75XX_FIRMWARE_VERSION			0x0012

#define ZT75XX_MINOR_FW_VERSION			0x0121

#define ZT75XX_VENDOR_ID			0x001C
#define ZT75XX_HW_ID				0x0014

#define ZT75XX_DATA_VERSION_REG			0x0013
#define ZT75XX_SUPPORTED_FINGER_NUM		0x0015
#define ZT75XX_EEPROM_INFO			0x0018
#define ZT75XX_INITIAL_TOUCH_MODE		0x0019

#define ZT75XX_TOTAL_NUMBER_OF_X		0x0060
#define ZT75XX_TOTAL_NUMBER_OF_Y		0x0061

#define ZT75XX_CONNECTION_CHECK_REG		0x0062

#define ZT75XX_DELAY_RAW_FOR_HOST		0x007f

#define ZT75XX_BUTTON_SUPPORTED_NUM		0x00B0
#define ZT75XX_BUTTON_SENSITIVITY		0x00B2
#define ZT75XX_DUMMY_BUTTON_SENSITIVITY		0X00C8

#define ZT75XX_X_RESOLUTION			0x00C0
#define ZT75XX_Y_RESOLUTION			0x00C1

#define ZT75XX_POINT_STATUS_REG			0x0080
#define ZT75XX_ICON_STATUS_REG			0x00AA

#define ZT75XX_SET_AOD_X_REG			0x00AB
#define ZT75XX_SET_AOD_Y_REG			0x00AC
#define ZT75XX_SET_AOD_W_REG			0x00AD
#define ZT75XX_SET_AOD_H_REG			0x00AE
#define ZT75XX_LPM_MODE_REG			0x00AF

#define ZT75XX_GET_AOD_X_REG			0x0191
#define ZT75XX_GET_AOD_Y_REG			0x0192

#define ZT75XX_DND_SHIFT_VALUE			0x012B
#define ZT75XX_AFE_FREQUENCY			0x0100
#define ZT75XX_DND_N_COUNT			0x0122
#define ZT75XX_DND_U_COUNT			0x0135

#define ZT75XX_RAWDATA_REG			0x0200

#define ZT75XX_INT_ENABLE_FLAG			0x00f0
#define ZT75XX_PERIODICAL_INTERRUPT_INTERVAL	0x00f1
#define ZT75XX_BTN_WIDTH			0x0316
#define ZT75XX_REAL_WIDTH			0x03A6

#define ZT75XX_CHECKSUM_RESULT			0x012c

#define ZT75XX_INIT_FLASH			0x01d0
#define ZT75XX_WRITE_FLASH			0x01d1
#define ZT75XX_READ_FLASH			0x01d2

#define ZINITIX_INTERNAL_FLAG_03		0x011f

#define ZT75XX_OPTIONAL_SETTING			0x0116
#define ZT75XX_COVER_CONTROL_REG		0x023E

#define ZT75XX_RESOLUTION_EXPANDER		0x0186
#define ZT75XX_MUTUAL_AMP_V_SEL			0x02F9
#define ZT75XX_SX_AMP_V_SEL			0x02DF
#define ZT75XX_SX_SUB_V_SEL			0x02E0
#define ZT75XX_SY_AMP_V_SEL			0x02EC
#define ZT75XX_SY_SUB_V_SEL			0x02ED
#define ZT75XX_CHECKSUM				0x03DF
#define ZT75XX_JITTER_SAMPLING_CNT		0x001F

#define ZT75XX_SY_SAT_FREQUENCY			0x03E0
#define ZT75XX_SY_SAT_N_COUNT			0x03E1
#define ZT75XX_SY_SAT_U_COUNT			0x03E2
#define ZT75XX_SY_SAT_RS0_TIME			0x03E3
#define ZT75XX_SY_SAT_RBG_SEL			0x03E4
#define ZT75XX_SY_SAT_AMP_V_SEL			0x03E5
#define ZT75XX_SY_SAT_SUB_V_SEL			0x03E6

#define ZT75XX_SY_SAT2_FREQUENCY		0x03E7
#define ZT75XX_SY_SAT2_N_COUNT			0x03E8
#define ZT75XX_SY_SAT2_U_COUNT			0x03E9
#define ZT75XX_SY_SAT2_RS0_TIME			0x03EA
#define ZT75XX_SY_SAT2_RBG_SEL			0x03EB
#define ZT75XX_SY_SAT2_AMP_V_SEL		0x03EC
#define ZT75XX_SY_SAT2_SUB_V_SEL		0x03ED

/* Interrupt & status register flag bit
-------------------------------------------------
*/
#define BIT_PT_CNT_CHANGE	0
#define BIT_DOWN		1
#define BIT_MOVE		2
#define BIT_UP			3
#define BIT_PALM		4
#define BIT_PALM_REJECT		5
#define BIT_GESTURE		6
#define RESERVED_1		7
#define BIT_WEIGHT_CHANGE	8
#define BIT_PT_NO_CHANGE	9
#define BIT_REJECT		10
#define BIT_PT_EXIST		11
#define RESERVED_2		12
#define BIT_MUST_ZERO		13
#define BIT_DEBUG		14
#define BIT_ICON_EVENT		15

/* button */
#define BIT_O_ICON0_DOWN	0
#define BIT_O_ICON1_DOWN	1
#define BIT_O_ICON2_DOWN	2
#define BIT_O_ICON3_DOWN	3
#define BIT_O_ICON4_DOWN	4
#define BIT_O_ICON5_DOWN	5
#define BIT_O_ICON6_DOWN	6
#define BIT_O_ICON7_DOWN	7

#define BIT_O_ICON0_UP		8
#define BIT_O_ICON1_UP		9
#define BIT_O_ICON2_UP		10
#define BIT_O_ICON3_UP		11
#define BIT_O_ICON4_UP		12
#define BIT_O_ICON5_UP		13
#define BIT_O_ICON6_UP		14
#define BIT_O_ICON7_UP		15


#define SUB_BIT_EXIST		0
#define SUB_BIT_DOWN		1
#define SUB_BIT_MOVE		2
#define SUB_BIT_UP		3
#define SUB_BIT_UPDATE		4
#define SUB_BIT_WAIT		5

/* ZT75XX_DEBUG_REG */
#define DEF_DEVICE_STATUS_NPM			0
#define DEF_DEVICE_STATUS_WALLET_COVER_MODE	1
#define DEF_DEVICE_STATUS_NOISE_MODE		2
#define DEF_DEVICE_STATUS_WATER_MODE		3
#define DEF_DEVICE_STATUS_LPM__MODE		4
#define BIT_GLOVE_TOUCH				5
#define DEF_DEVICE_STATUS_PALM_DETECT		10
#define DEF_DEVICE_STATUS_SVIEW_MODE		11

/* ZT75XX_COVER_CONTROL_REG */
#define WALLET_COVER_CLOSE	0x0000
#define VIEW_COVER_CLOSE	0x0100
#define COVER_OPEN		0x0200
#define LED_COVER_CLOSE		0x0700
#define CLEAR_COVER_CLOSE	0x0800

enum zt_cover_id {
	ZT_FLIP_WALLET = 0,
	ZT_VIEW_COVER,
	ZT_COVER_NOTHING1,
	ZT_VIEW_WIRELESS,
	ZT_COVER_NOTHING2,
	ZT_CHARGER_COVER,
	ZT_VIEW_WALLET,
	ZT_LED_COVER,
	ZT_CLEAR_FLIP_COVER,
	ZT_QWERTY_KEYBOARD_EUR,
	ZT_QWERTY_KEYBOARD_KOR,
	ZT_NEON_COVER,
	ZT_MONTBLANC_COVER = 100,
};

#define zinitix_bit_set(val, n)		((val) &= ~(1<<(n)), (val) |= (1<<(n)))
#define zinitix_bit_clr(val, n)		((val) &= ~(1<<(n)))
#define zinitix_bit_test(val, n)	((val) & (1<<(n)))
#define zinitix_swap_v(a, b, t)		((t) = (a), (a) = (b), (b) = (t))
#define zinitix_swap_16(s)		(((((s) & 0xff) << 8) | (((s) >> 8) & 0xff)))

/* REG_USB_STATUS : optional setting from AP */
#define DEF_OPTIONAL_MODE_USB_DETECT_BIT	0
#define	DEF_OPTIONAL_MODE_SVIEW_DETECT_BIT	1
#define	DEF_OPTIONAL_MODE_SENSITIVE_BIT		2
#define DEF_OPTIONAL_MODE_EDGE_SELECT		3
#define	DEF_OPTIONAL_MODE_DUO_TOUCH		4
/* end header file */

#define DEF_MIS_CAL_SPEC_MIN 40
#define DEF_MIS_CAL_SPEC_MAX 160
#define DEF_MIS_CAL_SPEC_MID 100

#define BIT_EVENT_SPAY	1
#define BIT_EVENT_AOD	2
#define BIT_EVENT_AOT	2

typedef enum {
	SPONGE_EVENT_TYPE_SPAY			= 0x04,
	SPONGE_EVENT_TYPE_AOD			= 0x08,
	SPONGE_EVENT_TYPE_AOD_PRESS		= 0x09,
	SPONGE_EVENT_TYPE_AOD_LONGPRESS		= 0x0A,
	SPONGE_EVENT_TYPE_AOD_DOUBLETAB		= 0x0B
} SPONGE_EVENT_TYPE;

#ifdef SEC_FACTORY_TEST
/* Touch Screen */
#define TSP_CMD_STR_LEN			32
#define TSP_CMD_RESULT_STR_LEN		3240	//30*18*6
#define TSP_CMD_PARAM_NUM		8
#define TSP_CMD_X_NUM			30
#define TSP_CMD_Y_NUM			18
#define TSP_CMD_NODE_NUM		(TSP_CMD_Y_NUM * TSP_CMD_X_NUM)
#define tostring(x) #x

struct tsp_raw_data {
	s16 dnd_data[TSP_CMD_NODE_NUM];
	s16 hfdnd_data[TSP_CMD_NODE_NUM];
	s16 delta_data[TSP_CMD_NODE_NUM];
	s16 vgap_data[TSP_CMD_NODE_NUM];
	s16 hgap_data[TSP_CMD_NODE_NUM];
	s16 rxshort_data[TSP_CMD_NODE_NUM];
	s16 txshort_data[TSP_CMD_NODE_NUM];
	s16 selfdnd_data[TSP_CMD_NODE_NUM];
	u16 ssr_data[TSP_CMD_NODE_NUM];
	s16 self_sat_dnd_data[TSP_CMD_NODE_NUM];
	s16 self_hgap_data[TSP_CMD_NODE_NUM];
	s16 jitter_data[TSP_CMD_NODE_NUM];
	s16 reference_data[TSP_CMD_NODE_NUM];
	s16 reference_data_abnormal[TSP_CMD_NODE_NUM];
};

/* ----------------------------------------
 * write 0xE4 [ 11 | 10 | 01 | 00 ]
 * MSB <-------------------> LSB
 * read 0xE4
 * mapping sequnce : LSB -> MSB
 * struct sec_ts_test_result {
 * * assy : front + OCTA assay
 * * module : only OCTA
 *	 union {
 *		 struct {
 *			 u8 assy_count:2;	-> 00
 *			 u8 assy_result:2;	-> 01
 *			 u8 module_count:2;	-> 10
 *			 u8 module_result:2;	-> 11
 *		 } __attribute__ ((packed));
 *		 unsigned char data[1];
 *	 };
 *};
 * ----------------------------------------
 */
struct ts_test_result {
	union {
		struct {
			u8 assy_count:2;
			u8 assy_result:2;
			u8 module_count:2;
			u8 module_result:2;
		} __attribute__ ((packed));
		unsigned char data[1];
	};
};
#define TEST_OCTA_MODULE	1
#define TEST_OCTA_ASSAY		2

#define TEST_OCTA_NONE		0
#define TEST_OCTA_FAIL		1
#define TEST_OCTA_PASS		2

#endif /* SEC_FACTORY_TEST */

#define TSP_NORMAL_EVENT_MSG 1
static int m_ts_debug_mode = ZINITIX_DEBUG;
struct tsp_callbacks {
	void (*inform_charger)(struct tsp_callbacks *tsp_cb, bool mode);
};

static bool g_ta_connected = 0;
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
#define SECURE_TOUCH_ENABLED	1
#define SECURE_TOUCH_DISABLED	0
static bool old_ta_status;
#endif

typedef union {
	u16 optional_mode;
	struct select_mode {
		u8 flag;
		u8 cover_type;
	} select_mode;
} zt7538_setting;

zt7538_setting m_optional_mode;
zt7538_setting m_prev_optional_mode;

struct zt7538_lpm_setting {
	u8 flag;
	u8 data;
};
struct zt7538_lpm_setting lpm_mode_reg;

#if ESD_TIMER_INTERVAL
static struct workqueue_struct *esd_tmr_workqueue;
#endif

struct coord {
	u16 x;
	u16 y;
	u8 width;
	u8 sub_status;
#if (TOUCH_POINT_MODE == 2)
	u8 minor_width;
	u8 angle;
#endif
};

struct point_info {
	u16 status;
#if (TOUCH_POINT_MODE == 1)
	u16 event_flag;
#else
	u8 finger_cnt;
	u8 fw_status;
#endif
	struct coord coord[MAX_SUPPORTED_FINGER_NUM];
};

#define TOUCH_V_FLIP	0x01
#define TOUCH_H_FLIP	0x02
#define TOUCH_XY_SWAP	0x04

struct capa_info {
	u16 vendor_id;
	u16 ic_revision;
	u16 fw_version;
	u16 fw_minor_version;
	u16 reg_data_version;
	u16 threshold;
	u16 key_threshold;
	u16 dummy_threshold;
	u32 ic_fw_size;
	u32 MaxX;
	u32 MaxY;
	u8 gesture_support;
	u16 multi_fingers;
	u16 button_num;
	u16 ic_int_mask;
	u16 x_node_num;
	u16 y_node_num;
	u16 total_node_num;
	u16 hw_id;
	u16 afe_frequency;
	u16 shift_value;
	u16 mutual_amp_v_sel;
	u16 N_cnt;
	u16 u_cnt;
	u16 is_zmt200;
	u16 sx_amp_v_sel;
	u16 sx_sub_v_sel;
	u16 sy_amp_v_sel;
	u16 sy_sub_v_sel;
	u16 current_touch_mode;
};

enum work_state {
	NOTHING = 0,
	NORMAL,
	ESD_TIMER,
	EALRY_SUSPEND,
	SUSPEND,
	RESUME,
	LATE_RESUME,
	UPGRADE,
	REMOVE,
	SET_MODE,
	HW_CALIBRAION,
	RAW_DATA,
	PROBE,
	SLEEP_MODE_IN,
	SLEEP_MODE_OUT,
};

enum {
	BUILT_IN = 0,
	UMS,
	REQ_FW,
};

struct zt75xx_ts_info {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct zt75xx_ts_platform_data *pdata;
	char phys[32];
	/*struct task_struct *task;*/
	/*wait_queue_head_t wait;*/

	/*struct semaphore update_lock;*/
	/*u32 i2c_dev_addr;*/
	struct capa_info cap_info;
	struct point_info touch_info;
	struct point_info reported_touch_info;
	unsigned char *fw_data;

	u16 fw_ver_bin;
	u16 fw_minor_ver_bin;
	u16 fw_reg_ver_bin;
	u16 fw_hw_id_bin;

	u16 icon_event_reg;
	u16 prev_icon_event;
	/*u16 event_type;*/
	int irq;
	u8 button[MAX_SUPPORTED_BUTTON_NUM];
	u8 work_state;
	struct semaphore work_lock;
	u8 finger_cnt1;
	unsigned int move_count[MAX_SUPPORTED_FINGER_NUM];
	struct mutex set_reg_lock;
	struct mutex modechange;

	/*u16 debug_reg[8];*/ /* for debug */
	void (*register_cb)(void *);
	struct tsp_callbacks callbacks;

#if ESD_TIMER_INTERVAL
	struct work_struct tmr_work;
	struct timer_list esd_timeout_tmr;
	struct timer_list *p_esd_timeout_tmr;
	spinlock_t lock;
#endif
	struct semaphore raw_data_lock;
	u16 touch_mode;
	s16 cur_data[MAX_TRAW_DATA_SZ];
	u8 update;

#ifdef SEC_FACTORY_TEST
	struct tsp_raw_data *raw_data;
	struct sec_cmd_data sec;
#endif

#ifdef CONFIG_TOUCHSCREEN_DUAL_FOLDABLE
	int flip_status;
	int flip_status_current;
	int change_flip_status;
	struct mutex switching_mutex;
	struct delayed_work switching_work;
	struct notifier_block hall_ic_nb;
#endif

	struct delayed_work work_read_info;
	bool info_work_done;

	struct delayed_work ghost_check;
	u8 tsp_dump_lock;

	struct delayed_work work_print_info;
	u32 print_info_cnt_open;
	u32 print_info_cnt_release;

	struct completion resume_done;
	struct wake_lock wakelock;

#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	atomic_t secure_enabled;
	atomic_t secure_pending_irqs;
	struct completion secure_powerdown;
	struct completion secure_interrupt;
	struct clk *core_clk;
	struct clk *iface_clk;
#endif

	struct ts_test_result	test_result;

	s16 Gap_max_x;
	s16 Gap_max_y;
	s16 Gap_max_val;
	s16 Gap_min_x;
	s16 Gap_min_y;
	s16 Gap_min_val;
	s16 Gap_Gap_val;
	s16 Gap_node_num;
	struct pinctrl *pinctrl;
	bool tsp_pwr_enabled;
#ifdef CONFIG_VBUS_NOTIFIER
	struct notifier_block vbus_nb;
#endif
	u8 cover_type;
	bool flip_enable;
	bool spay_enable;
	bool aod_enable;
	bool aot_enable;
	bool sleep_mode;
	bool glove_touch;

	unsigned int scrub_id;
	unsigned int scrub_x;
	unsigned int scrub_y;

	u8 check_multi;
	unsigned int multi_count;
	unsigned int wet_count;
	bool wet_mode;
	unsigned int comm_err_count;
	u16 pressed_x[MAX_SUPPORTED_FINGER_NUM];
	u16 pressed_y[MAX_SUPPORTED_FINGER_NUM];
	long prox_power_off;

	struct sec_tclm_data *tdata;
	bool is_cal_done;
};

/* Dummy touchkey code */
#define KEY_DUMMY_HOME1	249
#define KEY_DUMMY_HOME2	250
#define KEY_DUMMY_MENU	251
#define KEY_DUMMY_HOME	252
#define KEY_DUMMY_BACK	253

/*<= you must set key button mapping*/
#ifdef NOT_SUPPORTED_TOUCH_DUMMY_KEY
u32 BUTTON_MAPPING_KEY[MAX_SUPPORTED_BUTTON_NUM] = {
	KEY_RECENT,KEY_BACK};
#else
u32 BUTTON_MAPPING_KEY[MAX_SUPPORTED_BUTTON_NUM] = {
	KEY_DUMMY_MENU, KEY_RECENT,// KEY_DUMMY_HOME1,
	/*KEY_DUMMY_HOME2,*/ KEY_BACK, KEY_DUMMY_BACK};
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
struct zt75xx_ts_info *tui_tsp_info;
extern int tui_force_close(uint32_t arg);
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
extern unsigned int lcdtype;
#endif
#ifdef CONFIG_DISPLAY_SAMSUNG
extern int get_lcd_attached(char *mode);
#endif

/* define i2c sub functions*/
static inline s32 read_data(struct i2c_client *client,
	u16 reg, u8 *values, u16 length)
{
	struct zt75xx_ts_info *info = i2c_get_clientdata(client);
	s32 ret;
	int count = 0;
	struct i2c_msg msg[2];

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &info->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif

retry:

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = (u8 *)(&reg);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = length;
	msg[1].buf = values;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret != 2) {
		usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

		if (++count < 8)
			goto retry;

		info->comm_err_count++;
		input_err(true, &info->client->dev, "%s: failed %d\n", __func__, ret);

		return -EIO;
	}

	return length;
}

#ifdef TCLM_CONCEPT
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
static s32 read_data_only(struct i2c_client *client, u8 *values, u16 length)
{
	struct zt75xx_ts_info *info = i2c_get_clientdata(client);
	s32 ret;
	int count = 0;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &info->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif
retry:
	ret = i2c_master_recv(client, values, length);
	if (ret < 0) {
		input_err(true, &client->dev, "%s: failed to recv. ret:%d, try:%d\n",
			__func__, ret, count + 1);
		usleep_range(1 * 1000, 1 * 1000);
		if (++count < 8)
			goto retry;

		info->comm_err_count++;
		return ret;
	}
	usleep_range(DELAY_FOR_TRANSCATION, DELAY_FOR_TRANSCATION);

	return length;
}
#endif
#endif

static inline s32 write_data(struct i2c_client *client,
	u16 reg, u8 *values, u16 length)
{
	struct zt75xx_ts_info *info = i2c_get_clientdata(client);
	s32 ret;
	int count = 0;
	u8 pkt[66]; /* max packet */

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &info->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif

	pkt[0] = (reg) & 0xff; /* reg addr */
	pkt[1] = (reg >> 8) & 0xff;
	memcpy((u8 *)&pkt[2], values, length);

retry:
	ret = i2c_master_send(client, pkt, length + 2);
	if (ret < 0) {
		usleep_range(1 * 1000, 1 * 1000);

		if (++count < 8)
			goto retry;

		info->comm_err_count++;
		return ret;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	return length;
}

static inline s32 write_reg(struct i2c_client *client, u16 reg, u16 value)
{
	if (write_data(client, reg, (u8 *)&value, 2) < 0)
		return I2C_FAIL;

	return I2C_SUCCESS;
}

static inline s32 write_cmd(struct i2c_client *client, u16 reg)
{
	struct zt75xx_ts_info *info = i2c_get_clientdata(client);
	s32 ret;
	int count = 0;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &info->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif

retry:
	ret = i2c_master_send(client, (u8 *)&reg, 2);
	if (ret < 0) {
		usleep_range(1 * 1000, 1 * 1000);

		if (++count < 8)
			goto retry;

		info->comm_err_count++;
		return ret;
	}
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	return I2C_SUCCESS;
}

static inline s32 read_raw_data(struct i2c_client *client,
		u16 reg, u8 *values, u16 length)
{
	struct zt75xx_ts_info *info = i2c_get_clientdata(client);
	s32 ret;
	int count = 0;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &info->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif

retry:
	/* select register */
	ret = i2c_master_send(client, (u8 *)&reg, 2);
	if (ret < 0) {
		usleep_range(1 * 1000, 1 * 1000);

		if (++count < 8)
			goto retry;

		info->comm_err_count++;
		return ret;
	}

	/* for setup tx transaction. */
	usleep_range(200, 200);

	ret = i2c_master_recv(client, values, length);
	if (ret < 0) {
		info->comm_err_count++;
		return ret;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	return length;
}

static inline s32 read_firmware_data(struct i2c_client *client,
	u16 addr, u8 *values, u16 length)
{
	struct zt75xx_ts_info *info = i2c_get_clientdata(client);
	s32 ret;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &info->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif

	/* select register*/
	ret = i2c_master_send(client, (u8 *)&addr, 2);
	if (ret < 0) {
		info->comm_err_count++;
		return ret;
	}

	/* for setup tx transaction. */
	usleep_range(1 * 1000, 1 * 1000);

	ret = i2c_master_recv(client, values, length);
	if (ret < 0) {
		info->comm_err_count++;
		return ret;
	}
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);

	return length;
}

#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
static irqreturn_t zt75xx_touch_work(int irq, void *data);
static void clear_report_data(struct zt75xx_ts_info *info);
static void esd_timer_stop(struct zt75xx_ts_info *info);
static void esd_timer_start(u16 sec, struct zt75xx_ts_info *info);
static void zt75xx_set_ta_status(struct zt75xx_ts_info *info);

static irqreturn_t secure_filter_interrupt(struct zt75xx_ts_info *info)
{
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		if (atomic_cmpxchg(&info->secure_pending_irqs, 0, 1) == 0) {
			sysfs_notify(&info->input_dev->dev.kobj, NULL, "secure_touch");
		} else {
			input_info(true, &info->client->dev, "%s: pending irq:%d\n",
					__func__, (int)atomic_read(&info->secure_pending_irqs));
		}

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int secure_touch_clk_prepare_enable(struct zt75xx_ts_info *info)
{
	int ret;

	if (!info->core_clk || !info->iface_clk) {
		input_err(true, &info->client->dev, "%s: error clk\n", __func__);
		return -ENODEV;
	}

	ret = clk_prepare_enable(info->core_clk);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed core clk\n", __func__);
		goto err_core_clk;
	}

	ret = clk_prepare_enable(info->iface_clk);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed iface clk\n", __func__);
		goto err_iface_clk;
	}

	return 0;

err_iface_clk:
	clk_disable_unprepare(info->core_clk);
err_core_clk:
	return -ENODEV;
}

static void secure_touch_clk_unprepare_disable(struct zt75xx_ts_info *info)
{
	if (!info->core_clk || !info->iface_clk) {
		input_err(true, &info->client->dev, "%s: error clk\n", __func__);
		return;
	}

	clk_disable_unprepare(info->core_clk);
	clk_disable_unprepare(info->iface_clk);
}

/**
 * Sysfs attr group for secure touch & interrupt handler for Secure world.
 * @atomic : syncronization for secure_enabled
 * @pm_runtime : set rpm_resume or rpm_ilde
 */
static ssize_t secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zt75xx_ts_info *info = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d", atomic_read(&info->secure_enabled));
}

static ssize_t secure_touch_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct zt75xx_ts_info *info = dev_get_drvdata(dev);
	int ret;
	unsigned long data;

	if (count > 2) {
		input_err(true, &info->client->dev,
				"%s: cmd length is over (%s,%d)!!\n",
				__func__, buf, (int)strlen(buf));
		return -EINVAL;
	}

	ret = kstrtoul(buf, 10, &data);
	if (ret != 0) {
		input_err(true, &info->client->dev, "%s: failed to read:%d\n",
				__func__, ret);
		return -EINVAL;
	}

	if (data == 1) {
		/* Enable Secure World */
		if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
			input_err(true, &info->client->dev, "%s: already enabled\n", __func__);
			return -EBUSY;
		}

		/* syncronize_irq -> disable_irq + enable_irq
		 * concern about timing issue.
		 */
		down(&info->work_lock);
		disable_irq(info->client->irq);

		/* zinitix timer stop, release all finger */
		write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
		esd_timer_stop(info);
		clear_report_data(info);

		if (pm_runtime_get_sync(info->client->adapter->dev.parent) < 0) {
			enable_irq(info->client->irq);
			esd_timer_start(CHECK_ESD_TIMER, info);
			write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
			input_err(true, &info->client->dev, "%s: failed to get pm_runtime\n", __func__);
			up(&info->work_lock);
			return -EIO;
		}

		if (secure_touch_clk_prepare_enable(info) < 0) {
			pm_runtime_put(info->client->adapter->dev.parent);
			enable_irq(info->client->irq);
			esd_timer_start(CHECK_ESD_TIMER, info);
			write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
			input_err(true, &info->client->dev, "%s: failed to clk enable\n", __func__);
			up(&info->work_lock);
			return -ENXIO;
		}

		reinit_completion(&info->secure_powerdown);
		reinit_completion(&info->secure_interrupt);

		msleep(10);

		atomic_set(&info->secure_enabled, 1);
		atomic_set(&info->secure_pending_irqs, 0);

		enable_irq(info->client->irq);

		input_info(true, &info->client->dev, "%s: secure touch enable\n", __func__);
		up(&info->work_lock);
	} else if (data == 0) {

		/* Disable Secure World */
		if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_DISABLED) {
			input_err(true, &info->client->dev, "%s: already disabled\n", __func__);
			return count;
		}

		secure_touch_clk_unprepare_disable(info);
		pm_runtime_put_sync(info->client->adapter->dev.parent);
		atomic_set(&info->secure_enabled, 0);
		sysfs_notify(&info->input_dev->dev.kobj, NULL, "secure_touch");
		msleep(10);

		clear_report_data(info);
		zt75xx_touch_work(info->client->irq, info);

		complete(&info->secure_powerdown);
		complete(&info->secure_interrupt);

		if (old_ta_status != g_ta_connected)
			zt75xx_set_ta_status(info);

		/* zinitix timer start */
		esd_timer_start(CHECK_ESD_TIMER, info);
		write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
		input_info(true, &info->client->dev, "%s: secure touch disable\n", __func__);

	} else {
		input_err(true, &info->client->dev, "%s: unsupported value, %d\n", __func__, data);
		return -EINVAL;
	}

	return count;
}

static ssize_t secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zt75xx_ts_info *info = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_DISABLED) {
		input_err(true, &info->client->dev, "%s: disabled\n", __func__);
		return -EBADF;
	}

	if (atomic_cmpxchg(&info->secure_pending_irqs, -1, 0) == -1) {
		input_err(true, &info->client->dev, "%s: pending irq -1\n", __func__);
		return -EINVAL;
	}

	if (atomic_cmpxchg(&info->secure_pending_irqs, 1, 0) == 1)
		val = 1;

	input_err(true, &info->client->dev, "%s: pending irq is %d\n",
			__func__, atomic_read(&info->secure_pending_irqs));

	complete(&info->secure_interrupt);

	return snprintf(buf, PAGE_SIZE, "%u", val);
}

static DEVICE_ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
		secure_touch_enable_show, secure_touch_enable_store);
static DEVICE_ATTR(secure_touch, S_IRUGO, secure_touch_show, NULL);

static struct attribute *secure_attr[] = {
	&dev_attr_secure_touch_enable.attr,
	&dev_attr_secure_touch.attr,
	NULL,
};

static struct attribute_group secure_attr_group = {
	.attrs = secure_attr,
};

static int secure_touch_init(struct zt75xx_ts_info *info)
{
	input_info(true, &info->client->dev, "%s\n", __func__);

	init_completion(&info->secure_powerdown);
	init_completion(&info->secure_interrupt);

	info->core_clk = clk_get(&info->client->adapter->dev, "core_clk");
	if (IS_ERR_OR_NULL(info->core_clk)) {
		input_err(true, &info->client->dev, "%s: failed to get core_clk: %ld\n",
				__func__, PTR_ERR(info->core_clk));
		goto err_core_clk;
	}

	info->iface_clk = clk_get(&info->client->adapter->dev, "iface_clk");
	if (IS_ERR_OR_NULL(info->iface_clk)) {
		input_err(true, &info->client->dev, "%s: failed to get iface_clk: %ld\n",
				__func__, PTR_ERR(info->iface_clk));
		goto err_iface_clk;
	}

	return 0;

err_iface_clk:
	clk_put(info->core_clk);
err_core_clk:
	info->core_clk = NULL;
	info->iface_clk = NULL;

	return -ENODEV;
}

static void secure_touch_remove(struct zt75xx_ts_info *info)
{
	if (!IS_ERR_OR_NULL(info->core_clk))
		clk_put(info->core_clk);

	if (!IS_ERR_OR_NULL(info->iface_clk))
		clk_put(info->iface_clk);
}

static void secure_touch_stop(struct zt75xx_ts_info *info, bool stop)
{
	if (atomic_read(&info->secure_enabled)) {
		atomic_set(&info->secure_pending_irqs, -1);

		sysfs_notify(&info->input_dev->dev.kobj, NULL, "secure_touch");

		if (stop)
			wait_for_completion_interruptible(&info->secure_powerdown);

		input_info(true, &info->client->dev, "%s: %d\n", __func__, stop);
	}
}
#endif

#ifdef CONFIG_INPUT_ENABLED
static int  zt75xx_ts_open(struct input_dev *dev);
static void zt75xx_ts_close(struct input_dev *dev);
#endif

static bool zt75xx_power_control(struct zt75xx_ts_info *info, u8 ctl);
static int zt75xx_pinctrl_configure(struct zt75xx_ts_info *info, bool active);

static bool init_touch(struct zt75xx_ts_info *info);
static bool mini_init_touch(struct zt75xx_ts_info *info);
static void clear_report_data(struct zt75xx_ts_info *info);
#if ESD_TIMER_INTERVAL
static void esd_timer_start(u16 sec, struct zt75xx_ts_info *info);
static void esd_timer_stop(struct zt75xx_ts_info *info);
static void esd_timer_init(struct zt75xx_ts_info *info);
static void esd_timeout_handler(unsigned long data);
#endif

#ifdef TCLM_CONCEPT
int get_zt_tsp_nvm_data(struct zt75xx_ts_info *info, u8 addr, u8 *values, u16 length);
int set_zt_tsp_nvm_data(struct zt75xx_ts_info *info, u8 addr, u8 *values, u16 length);
#endif

#ifdef USE_MISC_DEVICE
static long ts_misc_fops_ioctl(struct file *filp, unsigned int cmd,
								unsigned long arg);
static int ts_misc_fops_open(struct inode *inode, struct file *filp);
static int ts_misc_fops_close(struct inode *inode, struct file *filp);

static const struct file_operations ts_misc_fops = {
	.owner = THIS_MODULE,
	.open = ts_misc_fops_open,
	.release = ts_misc_fops_close,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ts_misc_fops_ioctl,
#else
	.unlocked_ioctl = ts_misc_fops_ioctl,
#endif
};

static struct miscdevice touch_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "zinitix_touch_misc",
	.fops = &ts_misc_fops,
};

#define TOUCH_IOCTL_BASE			0xbc
#define TOUCH_IOCTL_GET_DEBUGMSG_STATE		_IOW(TOUCH_IOCTL_BASE, 0, int)
#define TOUCH_IOCTL_SET_DEBUGMSG_STATE		_IOW(TOUCH_IOCTL_BASE, 1, int)
#define TOUCH_IOCTL_GET_CHIP_REVISION		_IOW(TOUCH_IOCTL_BASE, 2, int)
#define TOUCH_IOCTL_GET_FW_VERSION		_IOW(TOUCH_IOCTL_BASE, 3, int)
#define TOUCH_IOCTL_GET_REG_DATA_VERSION	_IOW(TOUCH_IOCTL_BASE, 4, int)
#define TOUCH_IOCTL_VARIFY_UPGRADE_SIZE		_IOW(TOUCH_IOCTL_BASE, 5, int)
#define TOUCH_IOCTL_VARIFY_UPGRADE_DATA		_IOW(TOUCH_IOCTL_BASE, 6, int)
#define TOUCH_IOCTL_START_UPGRADE		_IOW(TOUCH_IOCTL_BASE, 7, int)
#define TOUCH_IOCTL_GET_X_NODE_NUM		_IOW(TOUCH_IOCTL_BASE, 8, int)
#define TOUCH_IOCTL_GET_Y_NODE_NUM		_IOW(TOUCH_IOCTL_BASE, 9, int)
#define TOUCH_IOCTL_GET_TOTAL_NODE_NUM		_IOW(TOUCH_IOCTL_BASE, 10, int)
#define TOUCH_IOCTL_SET_RAW_DATA_MODE		_IOW(TOUCH_IOCTL_BASE, 11, int)
#define TOUCH_IOCTL_GET_RAW_DATA		_IOW(TOUCH_IOCTL_BASE, 12, int)
#define TOUCH_IOCTL_GET_X_RESOLUTION		_IOW(TOUCH_IOCTL_BASE, 13, int)
#define TOUCH_IOCTL_GET_Y_RESOLUTION		_IOW(TOUCH_IOCTL_BASE, 14, int)
#define TOUCH_IOCTL_HW_CALIBRAION		_IOW(TOUCH_IOCTL_BASE, 15, int)
#define TOUCH_IOCTL_GET_REG			_IOW(TOUCH_IOCTL_BASE, 16, int)
#define TOUCH_IOCTL_SET_REG			_IOW(TOUCH_IOCTL_BASE, 17, int)
#define TOUCH_IOCTL_SEND_SAVE_STATUS		_IOW(TOUCH_IOCTL_BASE, 18, int)
#define TOUCH_IOCTL_DONOT_TOUCH_EVENT		_IOW(TOUCH_IOCTL_BASE, 19, int)
#endif

struct zt75xx_ts_info *misc_info;

static void set_cover_type(struct zt75xx_ts_info *info, bool enable)
{
	struct i2c_client *client = info->client;

	write_cmd(info->client, 0x0A);
	if (enable) {
		switch (info->cover_type) {
		case ZT_FLIP_WALLET:
			write_reg(client, ZT75XX_COVER_CONTROL_REG, WALLET_COVER_CLOSE);
			break;
		case ZT_VIEW_COVER:
			write_reg(client, ZT75XX_COVER_CONTROL_REG, VIEW_COVER_CLOSE);
			break;
		case ZT_CLEAR_FLIP_COVER:
			write_reg(client, ZT75XX_COVER_CONTROL_REG, CLEAR_COVER_CLOSE);
			break;
		case ZT_NEON_COVER:
			write_reg(client, ZT75XX_COVER_CONTROL_REG, LED_COVER_CLOSE);
			break;
		default:
			input_err(true, &info->client->dev, "%s: touch is not supported for %d cover\n",
				__func__, info->cover_type);
		}
	} else {
		write_reg(client, ZT75XX_COVER_CONTROL_REG, COVER_OPEN);
	}

	write_cmd(info->client, 0x0B);
	input_info(true, &info->client->dev, "%s: type %d enable %d\n", __func__, info->cover_type, enable);
}

static void zt75xx_set_optional_mode(struct zt75xx_ts_info *info, bool force)
{
	u16 reg_val;

	if (m_prev_optional_mode.optional_mode == m_optional_mode.optional_mode && !force)
		return;

	mutex_lock(&info->set_reg_lock);
	reg_val = m_optional_mode.optional_mode;
	mutex_unlock(&info->set_reg_lock);

	if (write_reg(info->client, ZT75XX_OPTIONAL_SETTING, reg_val) == I2C_SUCCESS) {
		m_prev_optional_mode.optional_mode = reg_val;
	}
}

#ifdef SEC_FACTORY_TEST
static bool get_raw_data(struct zt75xx_ts_info *info, u8 *buff, int skip_cnt)
{
	struct i2c_client *client = info->client;
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	u32 total_node = info->cap_info.total_node_num;
	u32 sz;
	int i, j = 0;

	disable_irq(info->irq);

	down(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_info(true, &client->dev, "%s: other process occupied(%d)\n",
			__func__, info->work_state);
		enable_irq(info->irq);
		up(&info->work_lock);
		return false;
	}

	info->work_state = RAW_DATA;

	for (i = 0; i < skip_cnt; i++) {
		while (gpio_get_value(pdata->gpio_int)) {
			usleep_range(1 * 1000, 1 * 1000);
			if (++j > 3000) {
				input_err(true, &info->client->dev, "%s: (skip_cnt) wait int timeout\n", __func__);
				break;
			}
		}

		write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
		usleep_range(1 * 1000, 1 * 1000);
	}

	input_info(true, &client->dev, "%s: read raw data\n", __func__);
	sz = total_node * 2;

	j = 0;
	while (gpio_get_value(pdata->gpio_int)) {
		usleep_range(1 * 1000, 1 * 1000);
		if (++j > 3000) {
			input_err(true, &info->client->dev, "%s: wait int timeout\n", __func__);
			break;
		}
	}

	if (read_raw_data(client, ZT75XX_RAWDATA_REG, (char *)buff, sz) < 0) {
		input_err(true, &info->client->dev, "%s: error read zinitix tc raw data\n", __func__);
		info->work_state = NOTHING;
		enable_irq(info->irq);
		up(&info->work_lock);
		return false;
	}

	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);

	return true;
}
#endif

static bool ts_get_raw_data(struct zt75xx_ts_info *info)
{
	struct i2c_client *client = info->client;
	u32 total_node = info->cap_info.total_node_num;
	u32 sz;

	if (down_trylock(&info->raw_data_lock)) {
		input_err(true, &client->dev, "%s: Failed to occupy sema\n", __func__);
		info->touch_info.status = 0;
		return true;
	}

	sz = total_node * 2 + sizeof(struct point_info);

	if (read_raw_data(info->client, ZT75XX_RAWDATA_REG, (char *)info->cur_data, sz) < 0) {
		input_err(true, &client->dev, "%s: Failed to read raw data\n", __func__);
		up(&info->raw_data_lock);
		return false;
	}

	info->update = 1;
	memcpy((u8 *)(&info->touch_info), (u8 *)&info->cur_data[total_node], sizeof(struct point_info));
	up(&info->raw_data_lock);

	return true;
}

static bool ts_read_coord(struct zt75xx_ts_info *info)
{
	struct i2c_client *client = info->client;
	int retry_cnt;
	u16* u16_point_info;
	int i;

	/* for  Debugging Tool */

	if (info->touch_mode != TOUCH_POINT_MODE) {
		if (ts_get_raw_data(info) == false)
			return false;

		input_err(true, &client->dev, "%s: status = 0x%04X\n", __func__, info->touch_info.status);

		goto out;
	}

#if (TOUCH_POINT_MODE == 1)
	memset(&info->touch_info, 0x0, sizeof(struct point_info));

	if (read_data(info->client, ZT75XX_POINT_STATUS_REG, (u8 *)(&info->touch_info), 4) < 0) {
		input_err(true, &client->dev, "%s: Failed to read point info\n", __func__);

		return false;
	}

	input_info(true, &client->dev, "%s: status reg = 0x%x , event_flag = 0x%04x\n",
			__func__, info->touch_info.status, info->touch_info.event_flag);

	if (info->touch_info.event_flag == 0)
		goto out;

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		if (zinitix_bit_test(info->touch_info.event_flag, i)) {
			usleep_range(20, 20);

			if (read_data(info->client, ZT75XX_POINT_STATUS_REG + 2 + (i * 4),
					(u8 *)(&info->touch_info.coord[i]), sizeof(struct coord)) < 0) {
				input_err(true, &client->dev, "%s: Failed to read point info\n", __func__);

				return false;
			}
		}
	}

#else
	u16_point_info = &(info->touch_info.status);
	retry_cnt = 0;
	while (retry_cnt < 10) {
		if (read_data(info->client, ZT75XX_POINT_STATUS_REG,
				(u8 *)(&info->touch_info), sizeof(struct point_info)) < 0) {
			input_err(true, &client->dev, "%s: Failed to read point info, retry_cnt = %d\n",
				__func__, ++retry_cnt);
			continue;
		}

		if (zinitix_bit_test(info->touch_info.status, BIT_MUST_ZERO) || info->touch_info.status == 0x1) {
			input_err(true, &client->dev, "%s: abnormal point info read, retry_cnt = %d\n",
				__func__, ++retry_cnt);
			continue;
		}

		for (i = 0; i < sizeof(struct point_info) / 2; i++) {
			if (*(u16_point_info + i) == 0xffff) {
				input_err(true, &client->dev, "%s: point info 0xffff, retry_cnt = %d\n",
					__func__, ++retry_cnt);
				info->touch_info.status = 0xffff;
				break;
			}
		}
		if (info->touch_info.status != 0xffff)
			break;

		retry_cnt++;
		continue;
	}

	if (retry_cnt >= 10)
		return false;
#endif

	/* LPM mode : Spay, AOT */
	if (info->pdata->support_lpm_mode && (zinitix_bit_test(info->touch_info.status, BIT_GESTURE))) {
		if (read_data(info->client, ZT75XX_LPM_MODE_REG, (u8 *)&lpm_mode_reg, 2) < 0)
			input_err(true, &client->dev, "%s: lpm_mode_reg read fail\n", __func__);
		input_info(true, &client->dev, "%s: lpm_mode_reg read 0x%02x%02x\n",
			__func__, lpm_mode_reg.data, lpm_mode_reg.flag);

		if (zinitix_bit_test(lpm_mode_reg.data, BIT_EVENT_SPAY)) {
			input_info(true, &client->dev, "%s: Spay Gesture\n", __func__);

			info->scrub_id = SPONGE_EVENT_TYPE_SPAY;
			info->scrub_x = 0;
			info->scrub_y = 0;

			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
			input_sync(info->input_dev);
			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
			input_sync(info->input_dev);
		} else if (zinitix_bit_test(lpm_mode_reg.data, BIT_EVENT_AOD)) {
			if (info->aot_enable) {
				input_info(true, &client->dev, "%s: AOT Doubletab\n", __func__);

				input_report_key(info->input_dev, KEY_HOMEPAGE, 1);
				input_sync(info->input_dev);
				input_report_key(info->input_dev, KEY_HOMEPAGE, 0);
				input_sync(info->input_dev);
			} else {
				input_info(true, &client->dev, "%s: AOD Doubletab\n", __func__);

				info->scrub_id = SPONGE_EVENT_TYPE_AOD_DOUBLETAB;
				if (read_data(info->client, ZT75XX_GET_AOD_X_REG, (u8 *)&info->scrub_x, 2) < 0)
					input_err(true, &client->dev, "%s: aod_x_reg read fail\n", __func__);
				if (read_data(info->client, ZT75XX_GET_AOD_Y_REG, (u8 *)&info->scrub_y, 2) < 0)
					input_err(true, &client->dev, "%s: aod_y_reg read fail\n", __func__);

				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				input_sync(info->input_dev);
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
				input_sync(info->input_dev);
			}
		}
	}

	zt75xx_set_optional_mode(info, false);

out:
	/* error */
#if (TOUCH_POINT_MODE == 1)
	if (zinitix_bit_test(info->touch_info.status, BIT_MUST_ZERO)) {
		input_err(true, &client->dev, "%s: Invalid must zero bit(%04x)\n",
			__func__, info->touch_info.status);
		return false;
	}
#endif
	write_cmd(info->client, ZT75XX_CLEAR_INT_STATUS_CMD);

	return true;
}

#if ESD_TIMER_INTERVAL
static void esd_timeout_handler(unsigned long data)
{
	struct zt75xx_ts_info *info = (struct zt75xx_ts_info *)data;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &info->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		esd_timer_stop(info);
		return;
	}
#endif
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &info->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		esd_timer_stop(info);
		return;
	}
#endif

	info->p_esd_timeout_tmr = NULL;
	queue_work(esd_tmr_workqueue, &info->tmr_work);
}

static void esd_timer_start(u16 sec, struct zt75xx_ts_info *info)
{
	unsigned long flags;

	if (info->sleep_mode) {
		input_info(true, &info->client->dev, "%s: skip (sleep_mode)!\n", __func__);
		return;
	}

	spin_lock_irqsave(&info->lock, flags);
	if (info->p_esd_timeout_tmr != NULL)
#ifdef CONFIG_SMP
		del_singleshot_timer_sync(info->p_esd_timeout_tmr);
#else
		del_timer(info->p_esd_timeout_tmr);
#endif
	info->p_esd_timeout_tmr = NULL;
	init_timer(&(info->esd_timeout_tmr));
	info->esd_timeout_tmr.data = (unsigned long)(info);
	info->esd_timeout_tmr.function = esd_timeout_handler;
	info->esd_timeout_tmr.expires = jiffies + (HZ * sec);
	info->p_esd_timeout_tmr = &info->esd_timeout_tmr;
	add_timer(&info->esd_timeout_tmr);
	spin_unlock_irqrestore(&info->lock, flags);
}

static void esd_timer_stop(struct zt75xx_ts_info *info)
{
	unsigned long flags;

	spin_lock_irqsave(&info->lock, flags);
	if (info->p_esd_timeout_tmr)
#ifdef CONFIG_SMP
		del_singleshot_timer_sync(info->p_esd_timeout_tmr);
#else
		del_timer(info->p_esd_timeout_tmr);
#endif

	info->p_esd_timeout_tmr = NULL;
	spin_unlock_irqrestore(&info->lock, flags);
}

static void esd_timer_init(struct zt75xx_ts_info *info)
{
	unsigned long flags;

	spin_lock_irqsave(&info->lock, flags);
	init_timer(&(info->esd_timeout_tmr));
	info->esd_timeout_tmr.data = (unsigned long)(info);
	info->esd_timeout_tmr.function = esd_timeout_handler;
	info->p_esd_timeout_tmr = NULL;
	spin_unlock_irqrestore(&info->lock, flags);
}

static void ts_tmr_work(struct work_struct *work)
{
	struct zt75xx_ts_info *info = container_of(work, struct zt75xx_ts_info, tmr_work);
	struct i2c_client *client = info->client;

#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "%s: tmr queue work ++\n", __func__);
#endif

	if (down_trylock(&info->work_lock)) {
		input_err(true, &client->dev, "%s: Failed to occupy work lock\n", __func__);
		esd_timer_start(CHECK_ESD_TIMER, info);

		return;
	}

	if (info->work_state != NOTHING) {
		input_info(true, &client->dev, "%s: Other process occupied (%d)\n",
			__func__, info->work_state);
		up(&info->work_lock);

		return;
	}

#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &client->dev, "%s: ignored, because touch is in secure mode\n", __func__);
		up(&info->work_lock);
		return;
	}
#endif

	info->work_state = ESD_TIMER;

	disable_irq(info->irq);
	zt75xx_power_control(info, POWER_OFF);
	zt75xx_power_control(info, POWER_ON_SEQUENCE);

	clear_report_data(info);
	if (mini_init_touch(info) == false)
		goto fail_time_out_init;

	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "%s: tmr queue work--\n", __func__);
#endif

	return;

fail_time_out_init:
	input_err(true, &client->dev, "%s: Failed to restart\n", __func__);
	esd_timer_start(CHECK_ESD_TIMER, info);
	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);

	return;
}
#endif

static bool zt75xx_power_sequence(struct zt75xx_ts_info *info)
{
	struct i2c_client *client = info->client;
	int retry = 0;
	u16 chip_code;

	info->cap_info.ic_fw_size = 32 * 1024;

retry_power_sequence:
	if (write_reg(client, 0xc000, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev,
			"%s: Failed to send power sequence(vendor cmd enable)\n", __func__);
		goto fail_power_sequence;
	}
	usleep_range(10, 10);

	if (read_data(client, 0xcc00, (u8 *)&chip_code, 2) < 0) {
		input_err(true, &client->dev, "%s: Failed to read chip code\n", __func__);
		goto fail_power_sequence;
	}

	input_info(true, &client->dev, "%s: chip code = 0x%x\n", __func__, chip_code);
	usleep_range(10, 10);

	if (chip_code == ZT7554_CHIP_CODE)
		info->cap_info.ic_fw_size = 64 * 1024;
	else if (chip_code == ZT7548_CHIP_CODE)
		info->cap_info.ic_fw_size = 48 * 1024;
	else if (chip_code == ZT7538_CHIP_CODE)
		info->cap_info.ic_fw_size = 44 * 1024;
	else if (chip_code == BT43X_CHIP_CODE)
		info->cap_info.ic_fw_size = 24 * 1024;
	else if (chip_code == BT53X_CHIP_CODE)
		info->cap_info.ic_fw_size = 32 * 1024;

	if (write_cmd(client, 0xc004) != I2C_SUCCESS) {
		input_err(true, &client->dev, "Failed to send power sequence(intn clear)\n");
		goto fail_power_sequence;
	}
	usleep_range(10, 10);

	if (write_reg(client, 0xc002, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "Failed to send power sequence(nvm init)\n");
		goto fail_power_sequence;
	}
	usleep_range(2 * 1000, 2 * 1000);

	if (write_reg(client, 0xc001, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "Failed to send power sequence(program start)\n");
		goto fail_power_sequence;
	}

	msleep(FIRMWARE_ON_DELAY);	/* wait for checksum cal */

	return true;

fail_power_sequence:
	if (retry++ < 3) {
		input_info(true, &client->dev, "%s: retry = %d\n", __func__, retry);
		msleep(CHIP_ON_DELAY);

		goto retry_power_sequence;
	}

	return false;
}

static bool zt75xx_power_control(struct zt75xx_ts_info *info, u8 ctl)
{
	struct i2c_client *client = info->client;
	int ret = 0;

	input_info(true, &client->dev, "%s: %d\n", __func__, ctl);

	ret = info->pdata->tsp_power(info, ctl);
	if (ret)
		return false;

	zt75xx_pinctrl_configure(info, ctl);

	if (ctl == POWER_ON_SEQUENCE) {
		msleep(CHIP_ON_DELAY);
		return zt75xx_power_sequence(info);
	} else if (ctl == POWER_OFF) {
		msleep(CHIP_OFF_DELAY);
	} else if (ctl == POWER_ON) {
		msleep(CHIP_ON_DELAY);
	}

	return true;
}

static void zt75xx_set_ta_status(struct zt75xx_ts_info *info)
{
	input_info(true, &info->client->dev,
		"%s: g_ta_connected %d\n", __func__, g_ta_connected);

	if (g_ta_connected) {
		mutex_lock(&info->set_reg_lock);
		zinitix_bit_set(m_optional_mode.select_mode.flag, DEF_OPTIONAL_MODE_USB_DETECT_BIT);
		mutex_unlock(&info->set_reg_lock);
	} else {
		mutex_lock(&info->set_reg_lock);
		zinitix_bit_clr(m_optional_mode.select_mode.flag, DEF_OPTIONAL_MODE_USB_DETECT_BIT);
		mutex_unlock(&info->set_reg_lock);
	}
}

#ifdef CONFIG_VBUS_NOTIFIER
int tsp_vbus_notification(struct notifier_block *nb,
				unsigned long cmd, void *data)
{
	struct zt75xx_ts_info *info = container_of(nb, struct zt75xx_ts_info, vbus_nb);
	vbus_status_t vbus_type = *(vbus_status_t *)data;

	input_info(true, &info->client->dev, "%s: cmd=%lu, vbus_type=%d\n", __func__, cmd, vbus_type);

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:
		input_info(true, &info->client->dev, "%s: attach\n",__func__);
		g_ta_connected = true;
		break;
	case STATUS_VBUS_LOW:
		input_info(true, &info->client->dev, "%s: detach\n",__func__);
		g_ta_connected = false;
		break;
	default:
		break;
	}

#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&misc_info->secure_enabled)) {
		input_info(true, &info->client->dev,
			"%s: ignored, because secure mode, old:%d, TA:%d\n",
			__func__, old_ta_status, g_ta_connected);
		return 0;
	} else {
		old_ta_status = g_ta_connected;
	}
#endif

	zt75xx_set_ta_status(info);
	return 0;
}
#endif

static void zt75xx_charger_status_cb(struct tsp_callbacks *cb, bool ta_status)
{
	struct zt75xx_ts_info *info = container_of(cb, struct zt75xx_ts_info, callbacks);

	if (!ta_status)
		g_ta_connected = false;
	else
		g_ta_connected = true;

#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&misc_info->secure_enabled)) {
		input_info(true, &info->client->dev,
			"%s: ignored, because secure mode, old:%d, TA:%d\n",
			__func__, old_ta_status, g_ta_connected);
		return;
	} else {
		old_ta_status = g_ta_connected;
	}
#endif

	zt75xx_set_ta_status(info);
	input_info(true, &info->client->dev, "%s: TA %s\n",
		__func__, ta_status ? "connected" : "disconnected");
}

static bool crc_check(struct zt75xx_ts_info *info)
{
	u16 chip_check_sum = 0;

	if (read_data(info->client, ZT75XX_CHECKSUM_RESULT,
					(u8 *)&chip_check_sum, 2) < 0) {
		input_err(true, &info->client->dev, "%s: read crc fail", __func__);
	}

	input_info(true, &info->client->dev, "%s: Check checksum 0x%04X\n", __func__, chip_check_sum);

	if (chip_check_sum == 0x55aa)
		return true;
	else
		return false;
}

#if TOUCH_ONESHOT_UPGRADE
static bool ts_check_need_upgrade(struct zt75xx_ts_info *info,
	u16 cur_version, u16 cur_minor_version, u16 cur_reg_version, u16 cur_hw_id)
{
	info->fw_ver_bin = (u16) (info->fw_data[52] | (info->fw_data[53] << 8));
	info->fw_minor_ver_bin = (u16) (info->fw_data[56] | (info->fw_data[57] << 8));
	info->fw_reg_ver_bin = (u16) (info->fw_data[60] | (info->fw_data[61] << 8));

#if CHECK_HWID
	info->fw_hw_id_bin = (u16) (fw_data[0x7528] | (fw_data[0x7529] << 8));
	input_info(true, &info->client->dev, "cur HW_ID = 0x%x, new HW_ID = 0x%x\n",
			cur_hw_id, info->fw_hw_id_bin);
	if (cur_hw_id != info->fw_hw_id_bin)
		return false;
#endif

	input_info(true, &info->client->dev, "cur version = 0x%x, new version = 0x%x\n",
			cur_version, info->fw_ver_bin);
	input_info(true, &info->client->dev, "cur minor version = 0x%x, new minor version = 0x%x\n",
			cur_minor_version, info->fw_minor_ver_bin);
	input_info(true, &info->client->dev, "cur reg data version = 0x%x, new reg data version = 0x%x\n",
			cur_reg_version, info->fw_reg_ver_bin);

	if (cur_version > 0xFF)
		return true;

	if (cur_version < info->fw_ver_bin)
		return true;
	else if (cur_version > info->fw_ver_bin)
		return false;

	if (cur_minor_version < info->fw_minor_ver_bin)
		return true;
	else if (cur_minor_version > info->fw_minor_ver_bin)
		return false;

	if (cur_reg_version < info->fw_reg_ver_bin)
		return true;

	return false;
}
#endif

#define TC_SECTOR_SZ		8
#define TC_NVM_SECTOR_SZ	64
#ifdef TCLM_CONCEPT
#define TC_SECTOR_SZ_WRITE	64
#define TC_SECTOR_SZ_READ	8
#endif

#if TOUCH_ONESHOT_UPGRADE || TOUCH_FORCE_UPGRADE \
	|| defined(SEC_FACTORY_TEST) || defined(USE_MISC_DEVICE)
static u8 ts_upgrade_firmware(struct zt75xx_ts_info *info,
	const u8 *firmware_data, u32 size)
{
	struct i2c_client *client = info->client;
	u16 flash_addr;
	u8 *verify_data;
	int retry_cnt = 0;
	int i;
	int page_sz = 128;
	u16 chip_code;
#ifndef TCLM_CONCEPT
	int fuzing_udelay = 8000;
#endif

	verify_data = kzalloc(size, GFP_KERNEL);
	if (verify_data == NULL) {
		input_err(true, &info->client->dev, "%s: cannot alloc verify buffer\n", __func__);
		return false;
	}

retry_upgrade:
	zt75xx_power_control(info, POWER_OFF);
	zt75xx_power_control(info, POWER_ON);
	usleep_range(10 * 1000, 10 * 1000);

	if (write_reg(client, 0xc000, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: power sequence error (vendor cmd enable)\n", __func__);
		goto fail_upgrade;
	}

	usleep_range(10, 10);

	if (read_data(client, 0xcc00, (u8 *)&chip_code, 2) < 0) {
		input_err(true, &client->dev, "%s: failed to read chip code\n", __func__);
		goto fail_upgrade;
	}

	input_info(true, &client->dev, "%s: chip code = 0x%x\n", __func__, chip_code);

#ifdef TCLM_CONCEPT
	if (chip_code == ZT7538_CHIP_CODE || chip_code == ZT7548_CHIP_CODE || chip_code == ZT7532_CHIP_CODE) {
		flash_addr = (firmware_data[0x61] << 16) | (firmware_data[0x62] << 8) | firmware_data[0x63];
		flash_addr += ((firmware_data[0x65] << 16) | (firmware_data[0x66] << 8) | firmware_data[0x67]);

		if (flash_addr != 0)
			size = flash_addr;

		page_sz = 64;

		if (write_reg(client, 0xc201, 0x00be) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: power sequence error (set clk speed)\n", __func__);
			goto fail_upgrade;
		}
		usleep_range(200, 200);
	}
	input_info(true, &client->dev, "%s: f/w size = 0x%x Page_sz = %d\n", __func__, size, page_sz);
#else
	if ((chip_code == ZT7538_CHIP_CODE) || (chip_code == ZT7548_CHIP_CODE) || (chip_code == BT43X_CHIP_CODE))
		page_sz = 64;
#endif
	usleep_range(10, 10);

	if (write_cmd(client, 0xc004) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: power sequence error (intn clear)\n", __func__);
		goto fail_upgrade;
	}

	usleep_range(10, 10);

	if (write_reg(client, 0xc002, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: power sequence error (nvm init)\n", __func__);
		goto fail_upgrade;
	}

	usleep_range(5 * 1000, 5 * 1000);

	input_info(true, &client->dev, "%s: init flash\n", __func__);

	if (write_reg(client, 0xc003, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to write nvm vpp on\n", __func__);
		goto fail_upgrade;
	}

	if (write_reg(client, 0xc104, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to write nvm wp disable\n", __func__);
		goto fail_upgrade;
	}

#ifdef TCLM_CONCEPT
	if (write_reg(client, ZT75XX_INIT_FLASH, 2) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to enter burst upgrade mode\n", __func__);
		goto fail_upgrade;
	}

	for (flash_addr = 0; flash_addr < size; ) {
		for (i = 0; i < page_sz / TC_SECTOR_SZ_WRITE; i++) {
			if (write_data(client, ZT75XX_WRITE_FLASH,
					(u8 *)&firmware_data[flash_addr], TC_SECTOR_SZ_WRITE) < 0) {
				input_err(true, &client->dev, "%s: error write zinitix tc firmare\n", __func__);
				goto fail_upgrade;
			}
			flash_addr += TC_SECTOR_SZ_WRITE;
		}
		i = 0;
		while (1) {
			if (flash_addr >= size)
				break;
			if (gpio_get_value(info->pdata->gpio_int))
				break;
			msleep(30);
			if (++i > 100)
				break;
		}

		if (i > 100) {
			input_err(true, &client->dev, "%s: write timeout\n", __func__);
			goto fail_upgrade;
		}
	}

	if (write_cmd(client, 0x01DD) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to flush cmd\n", __func__);
		goto fail_upgrade;
	}
	msleep(100);
	i = 0;

	while (1) {
		if (gpio_get_value(info->pdata->gpio_int))
			break;
		msleep(30);
		if (++i > 1000) {
			input_err(true, &client->dev, "%s: flush timeout\n", __func__);
			goto fail_upgrade;
		}
	}
#else
	if ((chip_code == ZT7538_CHIP_CODE) || (chip_code == ZT7548_CHIP_CODE) || (chip_code == ZT7554_CHIP_CODE)) {
		if (write_cmd(client, ZT75XX_INIT_FLASH) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: failed to init flash\n", __func__);
			goto fail_upgrade;
		}

		// Mass Erase
		//====================================================
		if (write_cmd(client, 0x01DF) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: failed to mass erase\n", __func__);
			goto fail_upgrade;
		}

		msleep(100);

		// Mass Erase End
		//====================================================

		if (write_reg(client, 0x01DE, 0x0001) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: failed to enter upgrade mode\n", __func__);
			goto fail_upgrade;
		}

		usleep_range(1000, 1000);

		if (write_reg(client, 0x01D3, 0x0008) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: failed to init upgrade mode\n", __func__);
			goto fail_upgrade;
		}
	} else if (chip_code == BT43X_CHIP_CODE) {
		// Mass Erase
		//====================================================
		if (write_reg(client, 0xc108, 0x0007) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: failed to write 0xc108 - 7\n", __func__);
			goto fail_upgrade;
		}

		if (write_reg(client, 0xc109, 0x0000) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: failed to write 0xc109\n", __func__);
			goto fail_upgrade;
		}

		if (write_reg(client, 0xc10A, 0x0000) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: failed to write nvm wp disable\n", __func__);
			goto fail_upgrade;
		}

		if (write_cmd(client, 0xc10B) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: failed to write mass erease\n", __func__);
			goto fail_upgrade;
		}

		msleep(20);

		if (write_reg(client, 0xc108, 0x0008) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: failed to write 0xc108 - 8\n", __func__);
			goto fail_upgrade;
		}

		if (write_cmd(client, ZT75XX_INIT_FLASH) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: failed to init flash\n", __func__);
			goto fail_upgrade;
		}

	} else {
		fuzing_udelay = 30000;
		if (write_cmd(client, ZT75XX_INIT_FLASH) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: failed to init flash\n", __func__);
			goto fail_upgrade;
		}
	}

	for (flash_addr = 0; flash_addr < size; ) {
		for (i = 0; i < page_sz / TC_SECTOR_SZ; i++) {
			if (write_data(client, ZT75XX_WRITE_FLASH,
					(u8 *)&firmware_data[flash_addr],TC_SECTOR_SZ) < 0) {
				input_err(true, &client->dev, "%s: error write zinitix tc firmare\n", __func__);
				goto fail_upgrade;
			}
			flash_addr += TC_SECTOR_SZ;
			usleep_range(100, 100);
		}
		usleep_range(fuzing_udelay, fuzing_udelay);	/* for fuzing delay */
	}
#endif

	if (write_reg(client, 0xc003, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: nvm write vpp off\n", __func__);
		goto fail_upgrade;
	}

	if (write_reg(client, 0xc104, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: nvm wp enable\n", __func__);
		goto fail_upgrade;
	}

	input_info(true, &client->dev, "%s: init flash\n", __func__);

	if (write_cmd(client, ZT75XX_INIT_FLASH) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to init flash\n", __func__);
		goto fail_upgrade;
	}

	input_info(true, &client->dev, "%s: read firmware data\n", __func__);

#ifdef TCLM_CONCEPT
	if (write_reg(client, 0x01D3, 0x0008) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to init upgrade mode\n", __func__);
		goto fail_upgrade;
	}
	usleep_range(1 * 1000, 1 * 1000);

	for (flash_addr = 0; flash_addr < size; ) {
		for (i = 0; i < page_sz / TC_SECTOR_SZ_READ; i++) {
			if (read_firmware_data(client, ZT75XX_READ_FLASH,
					(u8 *)&verify_data[flash_addr], TC_SECTOR_SZ_READ) < 0) {
				input_err(true, &client->dev, "%s: Failed to read firmware\n", __func__);
				goto fail_upgrade;
			}
			flash_addr += TC_SECTOR_SZ_READ;
		}
	}
#else
	for (flash_addr = 0; flash_addr < size; ) {
		for (i = 0; i < page_sz / TC_SECTOR_SZ; i++) {
			if (read_firmware_data(client, ZT75XX_READ_FLASH,
					(u8*)&verify_data[flash_addr], TC_SECTOR_SZ) < 0) {
				input_err(true, &client->dev, "%s: Failed to read firmware\n", __func__);
				goto fail_upgrade;
			}
			flash_addr += TC_SECTOR_SZ;
		}
	}
#endif

	/* verify */
	input_info(true, &client->dev, "%s: verify firmware data\n", __func__);
	if (memcmp((u8 *)&firmware_data[0], (u8 *)&verify_data[0], size) == 0) {
		input_info(true, &client->dev, "%s: upgrade finished\n", __func__);

		zt75xx_power_control(info, POWER_OFF);
		zt75xx_power_control(info, POWER_ON_SEQUENCE);

		if (!crc_check(info))
			goto fail_upgrade;

		if (verify_data) {
			kfree(verify_data);
			verify_data = NULL;
		}

		return true;
	}

fail_upgrade:
	zt75xx_power_control(info, POWER_OFF);

	if (retry_cnt++ < INIT_RETRY_CNT) {
		input_err(true, &client->dev,
			"%s: upgrade failed : so retry... (%d)\n", __func__, retry_cnt);
		goto retry_upgrade;
	}

	if (verify_data) {
		kfree(verify_data);
	}

	input_err(true, &client->dev, "%s: Failed to upgrade\n", __func__);

	return false;
}
#endif

static bool ts_hw_calibration(struct zt75xx_ts_info *info)
{
	struct i2c_client *client = info->client;
	u16 chip_eeprom_info;
	int time_out = 0;

	input_info(true, &client->dev, "%s: start\n", __func__);

	if (write_reg(client, ZT75XX_TOUCH_MODE, 0x07) != I2C_SUCCESS)
		return false;

	usleep_range(10 * 1000, 10 * 1000);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
	usleep_range(10 * 1000, 10 * 1000);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
	msleep(50);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
	usleep_range(10 * 1000, 10 * 1000);

	if (write_cmd(client, ZT75XX_CALIBRATE_CMD) != I2C_SUCCESS)
		return false;

	if (write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD) != I2C_SUCCESS)
		return false;

	usleep_range(10 * 1000, 10 * 1000);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);

	/* wait for h/w calibration*/
	do {
		msleep(200);
		write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);

		if (read_data(client, ZT75XX_EEPROM_INFO, (u8 *)&chip_eeprom_info, 2) < 0)
			return false;

		input_info(true, &client->dev, "%s: touch eeprom info = 0x%04X\n",
			__func__, chip_eeprom_info);
		if (!zinitix_bit_test(chip_eeprom_info, 0))
			break;

		if (time_out++ == 4) {
			write_cmd(client, ZT75XX_CALIBRATE_CMD);
			usleep_range(10 * 1000, 10 * 1000);
			write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
			input_err(true, &client->dev, "%s: retry timeout\n", __func__);
		}

		if (time_out++ > 10) {
			input_err(true, &client->dev, "%s: finally timeout\n", __func__);
			break;
		}
	} while (1);

	write_reg(client, 0xc003, 0x0001);
	write_reg(client, 0xc104, 0x0001);
	usleep_range(100, 100);

	if (write_cmd(client, ZT75XX_SAVE_CALIBRATION_CMD) != I2C_SUCCESS)
		return false;

	msleep(1100);
	write_reg(client, 0xc003, 0x0000);
	write_reg(client, 0xc104, 0x0000);

	info->is_cal_done = true;

	return true;
}

static int ic_version_check(struct zt75xx_ts_info *info)
{
	struct i2c_client *client = info->client;
	struct capa_info *cap = &(info->cap_info);
	int ret;
	u8 data[8] = {0};

	/* get chip information */
	ret = read_data(client, ZT75XX_VENDOR_ID, (u8 *)&cap->vendor_id, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail vendor id\n", __func__);
		goto error;
	}

	ret = read_data(client, ZT75XX_MINOR_FW_VERSION, (u8 *)&cap->fw_minor_version, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail fw_minor_version\n", __func__);
		goto error;
	}

	ret = read_data(client, ZT75XX_CHIP_REVISION, data, 8);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: fail chip_revision\n", __func__);
		goto error;
	}

	cap->ic_revision = data[0] | (data[1] << 8);
	cap->fw_version = data[2] | (data[3] << 8);
	cap->reg_data_version = data[4] | (data[5] << 8);
	cap->hw_id = data[6] | (data[7] << 8);

error:
	return ret;
}

static int fw_update_work(struct zt75xx_ts_info *info, bool force_update)
{
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct capa_info *cap = &(info->cap_info);
	int ret;
	bool need_update = false;
	const struct firmware *tsp_fw = NULL;
	char fw_path[MAX_FW_PATH];
	u16 chip_eeprom_info = 0;
#ifdef TCLM_CONCEPT
	int restore_cal = 0;
	int retry = 3;
#endif

#ifdef TCLM_CONCEPT
	if (info->tdata->support_tclm_test) {
		ret = sec_tclm_test_on_probe(info->tdata);
		if (ret < 0)
			input_err(true, &info->client->dev, "%s: sec_tclm_test_on_probe fail", __func__);
	}
#endif

	if (pdata->bringup) {
		input_info(true, &info->client->dev, "%s: bringup\n", __func__);
		return 0;
	}

	if (!pdata->firmware_name) {
		input_err(true, &info->client->dev, "%s: firmware_name is NULL\n", __func__);
		return 0;
	}

	input_info(true, &info->client->dev, "%s: start\n", __func__);

	snprintf(fw_path, MAX_FW_PATH, "%s", pdata->firmware_name);

	ret = request_firmware(&tsp_fw, fw_path, &(info->client->dev));
	if (ret < 0) {
		input_info(true, &info->client->dev,
			"%s: Firmware image %s not available\n", __func__, fw_path);
		goto fw_request_fail;
	}
	info->fw_data = (unsigned char *)tsp_fw->data;

	need_update = ts_check_need_upgrade(info, cap->fw_version,
		cap->fw_minor_version, cap->reg_data_version, cap->hw_id);
	if (!need_update) {
		if (!crc_check(info))
			need_update = true;
	}

	if (need_update == true || force_update == true) {
		ret = ts_upgrade_firmware(info, info->fw_data, cap->ic_fw_size);
		if (!ret) {
			input_err(true, &info->client->dev, "%s: failed fw update\n", __func__);
			ret = -1;
			goto fw_request_fail;
		}

		ret = ic_version_check(info);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: failed ic version check\n", __func__);
		}

		ret = read_data(info->client, ZT75XX_EEPROM_INFO, (u8 *)&chip_eeprom_info, 2);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: read eeprom_info i2c fail!\n", __func__);
		}
		input_info(true, &info->client->dev, "%s: eeprom_info 0x%04X\n", __func__, chip_eeprom_info);

#ifdef TCLM_CONCEPT
		while (retry--) {
			ret = info->tdata->tclm_read(info->tdata->client, SEC_TCLM_NVM_ALL_DATA);
			if (ret >= 0)
				break;
		}
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: SEC_TCLM_NVM_ALL_DATA i2c read fail", __func__);
			ret = 0;
			goto fw_request_fail;
		}

		input_info(true, &info->client->dev, "%s: tune_fix_ver [%04X] afe_base [%04X]\n",
			__func__, info->tdata->nvdata.tune_fix_ver, info->tdata->afe_base);

		if ((info->tdata->tclm_level > TCLM_LEVEL_CLEAR_NV) &&
			((info->tdata->nvdata.tune_fix_ver == 0xffff) || (info->tdata->afe_base > info->tdata->nvdata.tune_fix_ver))) {
			/* tune version up case */
			sec_tclm_root_of_cal(info->tdata, CALPOSITION_TUNEUP);
			restore_cal = 1;
		} else if (info->tdata->tclm_level == TCLM_LEVEL_CLEAR_NV) {
			/* firmup case */
			sec_tclm_root_of_cal(info->tdata, CALPOSITION_FIRMUP);
			restore_cal = 1;
		}

		if (restore_cal == 1) {
			input_err(true, &info->client->dev, "%s: RUN OFFSET CALIBRATION\n", __func__);
			ret = sec_execute_tclm_package(info->tdata, 0);
			if (ret < 0) {
				input_err(true, &info->client->dev, "%s: sec_execute_tclm_package fail\n", __func__);
				ret = 0;
			}
		}

		sec_tclm_root_of_cal(info->tdata, CALPOSITION_NONE);
#else
		if (zinitix_bit_test(chip_eeprom_info, 0)) { /* hw calibration bit */
			input_err(true, &info->client->dev, "%s: need to hw calibration\n", __func__);
			if (!ts_hw_calibration(info)) {
				ret = -1;
				goto fw_request_fail;
			}
		}
#endif
	}

fw_request_fail:
	release_firmware(tsp_fw);
	return ret;
}

static bool init_touch(struct zt75xx_ts_info *info)
{
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct capa_info *cap = &(info->cap_info);
	u16 reg_val = 0;
	u8 data[6] = {0};

	zinitix_bit_set(reg_val, BIT_PT_CNT_CHANGE);
	zinitix_bit_set(reg_val, BIT_DOWN);
	zinitix_bit_set(reg_val, BIT_MOVE);
	zinitix_bit_set(reg_val, BIT_UP);
#ifdef SUPPORTED_PALM_TOUCH
	zinitix_bit_set(reg_val, BIT_PALM);
	zinitix_bit_set(reg_val, BIT_PALM_REJECT);
#endif
	if (pdata->support_touchkey) {
		cap->button_num = SUPPORTED_BUTTON_NUM;
		zinitix_bit_set(reg_val, BIT_ICON_EVENT);
	}
	cap->ic_int_mask = reg_val;

	/* get x,y data */
	read_data(info->client, ZT75XX_TOTAL_NUMBER_OF_X, data, 4);
	info->cap_info.x_node_num = data[0] | (data[1] << 8);
	info->cap_info.y_node_num = data[2] | (data[3] << 8);

	info->cap_info.MaxX = pdata->x_resolution;
	info->cap_info.MaxY = pdata->y_resolution;

	info->cap_info.total_node_num = info->cap_info.x_node_num * info->cap_info.y_node_num;

	input_info(true, &info->client->dev, "%s: node x %d, y %d, resolution x %d, y %d\n",
		__func__, info->cap_info.x_node_num, info->cap_info.y_node_num,
		info->cap_info.MaxX, info->cap_info.MaxY);

	if (read_data(info->client, ZT75XX_SUPPORTED_FINGER_NUM, data, 1) < 0)
		info->cap_info.multi_fingers = MAX_SUPPORTED_FINGER_NUM;
	else
		info->cap_info.multi_fingers = data[0];
	if (info->cap_info.multi_fingers > MAX_SUPPORTED_FINGER_NUM)
		info->cap_info.multi_fingers = MAX_SUPPORTED_FINGER_NUM;
	input_info(true, &info->client->dev, "%s: supported_finger_num %d\n", __func__, info->cap_info.multi_fingers);

#if ESD_TIMER_INTERVAL
	if (write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
			SCAN_RATE_HZ * ESD_TIMER_INTERVAL) != I2C_SUCCESS)
		goto fail_init;

	read_data(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, (u8 *)&reg_val, 2);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &info->client->dev, "%s: Esd timer register = %d\n", __func__, reg_val);
#endif
#endif
	if (!mini_init_touch(info))
		goto fail_init;

	return true;

fail_init:
	return false;
}

static bool mini_init_touch(struct zt75xx_ts_info *info)
{
	struct i2c_client *client = info->client;
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	int i;

	if (write_cmd(client, ZT75XX_SWRESET_CMD) != I2C_SUCCESS) {
		input_info(true, &client->dev, "%s: Failed to write reset command\n", __func__);

		goto fail_mini_init;
	}

	if (write_reg(client, ZT75XX_TOUCH_MODE, info->touch_mode) != I2C_SUCCESS)
		goto fail_mini_init;

	/* cover_set */
	if (write_reg(client, ZT75XX_COVER_CONTROL_REG, COVER_OPEN) != I2C_SUCCESS)
		goto fail_mini_init;

	if (info->flip_enable) {
		set_cover_type(info, info->flip_enable);
	}

	zt75xx_set_optional_mode(info, true);

	if (write_reg(client, ZT75XX_INT_ENABLE_FLAG,
			info->cap_info.ic_int_mask) != I2C_SUCCESS)
		goto fail_mini_init;

	/* read garbage data */
	for (i = 0; i < 10; i++) {
		write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
		usleep_range(10, 10);
	}

#if ESD_TIMER_INTERVAL
	if (write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
			SCAN_RATE_HZ * ESD_TIMER_INTERVAL) != I2C_SUCCESS)
		goto fail_mini_init;

	esd_timer_start(CHECK_ESD_TIMER, info);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "%s: Started esd timer\n", __func__);
#endif
#endif

	if (pdata->support_lpm_mode) {
		write_reg(info->client, ZT75XX_LPM_MODE_REG, 0);
		if (pdata->support_aod && !pdata->support_aot) {
			write_reg(info->client, ZT75XX_SET_AOD_W_REG, 0);
			write_reg(info->client, ZT75XX_SET_AOD_H_REG, 0);
			write_reg(info->client, ZT75XX_SET_AOD_X_REG, 0);
			write_reg(info->client, ZT75XX_SET_AOD_Y_REG, 0);
		}
	}

	if ((pdata->support_lpm_mode) &&
			(info->spay_enable || info->aod_enable || info->aot_enable)) {
		if (info->sleep_mode) {
#if ESD_TIMER_INTERVAL
			esd_timer_stop(info);
#endif
			write_cmd(info->client, ZT75XX_SLEEP_CMD);
			input_info(true, &info->client->dev, "%s: sleep mode\n", __func__);
		}
	}

	input_info(true, &client->dev, "%s: Successfully mini initialized\n", __func__);

	return true;

fail_mini_init:
	input_err(true, &client->dev, "%s: Failed to initialize mini init\n", __func__);

	return false;
}

static void clear_report_data(struct zt75xx_ts_info *info)
{
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	int i;
	u8 reported = 0;
	u8 sub_status;

	if (pdata->support_touchkey) {
		for (i = 0; i < info->cap_info.button_num; i++) {
			if (info->button[i] == ICON_BUTTON_DOWN) {
				info->button[i] = ICON_BUTTON_UP;
				input_report_key(info->input_dev, BUTTON_MAPPING_KEY[i], 0);
				reported = true;
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_info(true, &client->dev, "Button up = %d\n", i);
#else
				input_info(true, &client->dev, "Button up\n");
#endif
			}
		}
		input_report_key(info->input_dev, BTN_TOUCH, 0);
	}

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		sub_status = info->reported_touch_info.coord[i].sub_status;
		if (zinitix_bit_test(sub_status, SUB_BIT_EXIST)) {
			input_mt_slot(info->input_dev, i);
#ifdef CONFIG_SEC_FACTORY
			input_report_abs(info->input_dev, ABS_MT_PRESSURE, 0);
#endif
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
			reported = true;
			if (!m_ts_debug_mode && TSP_NORMAL_EVENT_MSG)
				input_info(true, &client->dev, "[RA] tID:%d mc=%d\n", i, info->move_count[i]);
		}
		info->reported_touch_info.coord[i].sub_status = 0;
		info->move_count[i] = 0;
	}

#ifdef GLOVE_MODE
	input_report_switch(info->input_dev, SW_GLOVE, false);
	info->glove_touch = 0;
#endif

	if (reported) {
		input_sync(info->input_dev);
	}

	info->finger_cnt1 = 0;
	info->check_multi = 0;
}

#define	PALM_REPORT_WIDTH	200
#define	PALM_REJECT_WIDTH	255

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
void trustedui_mode_on(void){
	input_info(true, &tui_tsp_info->client->dev, "%s: release all finger\n", __func__);
	clear_report_data(tui_tsp_info);

	input_info(true, &tui_tsp_info->client->dev, "%s: esd timer disable\n", __func__);
#if ESD_TIMER_INTERVAL
	esd_timer_stop(tui_tsp_info);
	write_reg(tui_tsp_info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
#endif
}
EXPORT_SYMBOL(trustedui_mode_on);

void trustedui_mode_off(void){
	input_info(true, &tui_tsp_info->client->dev, "%s: esd timer enable\n", __func__);
#if ESD_TIMER_INTERVAL
	write_reg(tui_tsp_info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
	esd_timer_start(CHECK_ESD_TIMER, tui_tsp_info);
#endif
}
EXPORT_SYMBOL(trustedui_mode_off);
#endif

static void zt75xx_print_info(struct zt75xx_ts_info *info)
{
	struct irq_desc *desc = irq_to_desc(info->irq);

	info->print_info_cnt_open++;

	if (info->print_info_cnt_open > 0xfff0)
		info->print_info_cnt_open = 0;

	if (info->finger_cnt1 == 0)
		info->print_info_cnt_release++;

#ifdef TCLM_CONCEPT
	input_info(true, &info->client->dev,
			"mode:%02X iq:%d depth:%d lp:%x // v:%X%X%02X cal:%02X,C%02XT%04X.%4s%s // #%d %d\n",
			info->touch_info.fw_status, gpio_get_value(info->pdata->gpio_int),
			desc->depth, lpm_mode_reg.flag,
			info->cap_info.fw_version, info->cap_info.fw_minor_version, info->cap_info.reg_data_version,
			info->test_result.data[0],
			info->tdata->nvdata.cal_count, info->tdata->nvdata.tune_fix_ver,
			info->tdata->tclm_string[info->tdata->nvdata.cal_position].f_name,
			(info->tdata->tclm_level == TCLM_LEVEL_LOCKDOWN) ? ".L" : " ",
			info->print_info_cnt_open, info->print_info_cnt_release);
#else
	input_info(true, &info->client->dev,
			"mode:%02X iq:%d depth:%d lp:%x // v:%X%X%02X // #%d %d\n",
			info->touch_info.fw_status, gpio_get_value(info->pdata->gpio_int),
			desc->depth, lpm_mode_reg.flag,
			info->cap_info.fw_version, info->cap_info.fw_minor_version, info->cap_info.reg_data_version,
			info->print_info_cnt_open, info->print_info_cnt_release);
#endif
}

/************************************************************
*  720  * 1480 : <48 96 60> indicator: 24dp navigator:48dp edge:60px dpi=320
* 1080  * 2220 :  4096 * 4096 : <133 266 341>  (approximately value)
************************************************************/
static void location_detect(struct zt75xx_ts_info *info, char *loc, int x, int y)
{
	memset(loc, 0x00, 7);

	if (x < info->pdata->area_edge)
		strncat(loc, "E.", 2);
	else if (x < (info->pdata->x_resolution - info->pdata->area_edge))
		strncat(loc, "C.", 2);
	else
		strncat(loc, "e.", 2);

	if (y < info->pdata->area_indicator)
		strncat(loc, "S", 1);
	else if (y < (info->pdata->y_resolution - info->pdata->area_navigation))
		strncat(loc, "C", 1);
	else
		strncat(loc, "N", 1);
}

static irqreturn_t zt75xx_touch_work(int irq, void *data)
{
	struct zt75xx_ts_info* info = (struct zt75xx_ts_info*)data;
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	int i;
	u8 reported = false;
	u8 sub_status;
	u8 prev_sub_status;
	u32 x, y, w, maxX, maxY;
#ifdef CONFIG_SEC_FACTORY
	u32 z;
#endif
	u8 palm = 0;
#ifdef SUPPORTED_PALM_TOUCH
	u32 minor_w;
#endif
	u16 ic_status;
	char location[7] = "";

#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (IRQ_HANDLED == secure_filter_interrupt(info)) {
		wait_for_completion_interruptible_timeout(&info->secure_interrupt,
				msecs_to_jiffies(5 * MSEC_PER_SEC));

		input_info(true, &client->dev,
				"%s: secure interrupt handled\n", __func__);

		return IRQ_HANDLED;
	}
#endif

	if ((info->pdata->support_lpm_mode) && (info->sleep_mode)) {
		int ret;

		/* run lpm interrupt handler */
		wake_lock_timeout(&info->wakelock, msecs_to_jiffies(500));

		/* waiting for blsp block resuming, if not occurs i2c error */
		ret = wait_for_completion_interruptible_timeout(&info->resume_done, msecs_to_jiffies(500));
		if (ret == 0) {
			input_err(true, &info->client->dev, "%s: LPM: pm resume is not handled\n", __func__);
			return IRQ_HANDLED;
		} else if (ret < 0) {
			input_err(true, &info->client->dev, "%s: LPM: -ERESTARTSYS if interrupted, %d\n", __func__, ret);
			return IRQ_HANDLED;
		}

		input_info(true, &info->client->dev, "%s: run LPM interrupt handler, %d\n", __func__, ret);
	}

	if (gpio_get_value(info->pdata->gpio_int)) {
		input_err(true, &client->dev, "%s: Invalid interrupt\n", __func__);

		return IRQ_HANDLED;
	}

	if (down_trylock(&info->work_lock)) {
		input_err(true, &client->dev, "%s: Failed to occupy work lock\n", __func__);
		write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);

		return IRQ_HANDLED;
	}

#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&info->secure_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &client->dev, "%s: Igonored. secure touch is enabled\n", __func__);
		up(&info->work_lock);
		return IRQ_HANDLED;
	}
#endif

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
#endif

	if (info->work_state != NOTHING) {
		input_err(true, &client->dev, "%s: Other process occupied (0x%02x)\n", __func__, info->work_state);
		usleep_range(DELAY_FOR_SIGNAL_DELAY, DELAY_FOR_SIGNAL_DELAY);

		if (!gpio_get_value(info->pdata->gpio_int)) {
			write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
			usleep_range(DELAY_FOR_SIGNAL_DELAY, DELAY_FOR_SIGNAL_DELAY);
		}

		goto out;
	}

	info->work_state = NORMAL;

	if (ts_read_coord(info) == false) { /* maybe desirable reset */
		read_data(client, ZT75XX_DEBUG_REG, (u8 *)&ic_status, 2);
		input_err(true, &client->dev, "%s: Failed to read info coord (0x%x)\n", __func__, ic_status);
		zt75xx_power_control(info, POWER_OFF);
		zt75xx_power_control(info, POWER_ON_SEQUENCE);

		clear_report_data(info);
		mini_init_touch(info);

		goto out;
	}

	ic_status = info->touch_info.fw_status;
	if (zinitix_bit_test(ic_status, DEF_DEVICE_STATUS_WATER_MODE)) {
		if (!info->wet_mode) {
			info->wet_count++;
			info->wet_mode = true;
			input_info(true, &client->dev, "%s: wet mode enter, %d\n",
				__func__, info->wet_count);
		}
	} else {
		if (info->wet_mode) {
			input_info(true, &client->dev, "%s: wet mode exit\n", __func__);
			info->wet_mode = false;
		}
	}

	/* invalid : maybe periodical repeated int. */
	if (info->touch_info.status == 0x0) {
		goto out;
	}

	reported = false;
	if (pdata->support_touchkey) {
		if (zinitix_bit_test(info->touch_info.status, BIT_ICON_EVENT)) {
			if (read_data(info->client, ZT75XX_ICON_STATUS_REG,
					(u8 *)(&info->icon_event_reg), 2) < 0) {
				input_err(true, &client->dev, "%s: Failed to read button info\n", __func__);
				write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);

				goto out;
			}

			for (i = 0; i < info->cap_info.button_num; i++) {
				if (zinitix_bit_test(info->icon_event_reg, (BIT_O_ICON0_DOWN + i))) {
					info->button[i] = ICON_BUTTON_DOWN;
					input_report_key(info->input_dev, BUTTON_MAPPING_KEY[i], 1);
					reported = true;
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
					input_info(true, &client->dev, "Button down = %d\n", i);
#else
					input_info(true, &client->dev, "Button down\n");
#endif
				}
			}

			for (i = 0; i < info->cap_info.button_num; i++) {
				if (zinitix_bit_test(info->icon_event_reg, (BIT_O_ICON0_UP + i))) {
					info->button[i] = ICON_BUTTON_UP;
					input_report_key(info->input_dev, BUTTON_MAPPING_KEY[i], 0);
					reported = true;
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
					input_info(true, &client->dev, "Button up = %d\n", i);
#else
					input_info(true, &client->dev, "Button up\n");
#endif
				}
			}
		}
	}

	/* if button press or up event occured... */
	if (!zinitix_bit_test(info->touch_info.status, BIT_PT_EXIST)) {
		for (i = 0; i < info->cap_info.multi_fingers; i++) {
			prev_sub_status = info->reported_touch_info.coord[i].sub_status;
			if (zinitix_bit_test(prev_sub_status, SUB_BIT_EXIST)) {
				location_detect(info, location, info->touch_info.coord[i].x, info->touch_info.coord[i].y);

				input_info(true, &client->dev, "[R] tID:%d %s dd:%d,%d mc=%d (%d)\n",
							i, location, info->touch_info.coord[i].x - info->pressed_x[i],
							info->touch_info.coord[i].y - info->pressed_y[i],
							info->move_count[i], __LINE__);

				if (info->finger_cnt1 > 0)
					info->finger_cnt1--;
				if (info->finger_cnt1 == 0) {
					input_report_key(info->input_dev, BTN_TOUCH, 0);
					info->check_multi = 0;
					info->print_info_cnt_release = 0;
				}
				input_mt_slot(info->input_dev, i);
				input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
				info->move_count[i] = 0;
			}
		}
		memset(&info->reported_touch_info, 0x0, sizeof(struct point_info));
		input_sync(info->input_dev);

		if (reported == true) /* for button event */
			usleep_range(100, 100);

		goto out;
	}

#ifdef GLOVE_MODE
	if (info->glove_touch != zinitix_bit_test(ic_status, BIT_GLOVE_TOUCH)) {
		info->glove_touch = zinitix_bit_test(ic_status, BIT_GLOVE_TOUCH);
		input_report_switch(info->input_dev, SW_GLOVE, info->glove_touch);
	}
#endif

#ifdef SUPPORTED_PALM_TOUCH
	if (zinitix_bit_test(info->touch_info.status, BIT_PALM)) {
		//input_info(true, &client->dev, "Palm report\n");
		palm = 1;
	}

	if (zinitix_bit_test(info->touch_info.status, BIT_PALM_REJECT)) {
		//input_info(true, &client->dev, "Palm reject\n");
		palm = 2;
	}
#endif
	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		sub_status = info->touch_info.coord[i].sub_status;
		prev_sub_status = info->reported_touch_info.coord[i].sub_status;

		if (zinitix_bit_test(sub_status, SUB_BIT_EXIST)) {
			x = info->touch_info.coord[i].x;
			y = info->touch_info.coord[i].y;
			w = info->touch_info.coord[i].width;

			maxX = info->cap_info.MaxX;
			maxY = info->cap_info.MaxY;

			if (x > maxX || y > maxY) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_err(true, &client->dev,
					"%s: Invalid coord %d : x=%d, y=%d\n", __func__, i, x, y);
#endif
				continue;
			}

			info->touch_info.coord[i].x = x;
			info->touch_info.coord[i].y = y;

			if (w == 0)
				w = 1;
#ifdef SUPPORTED_PALM_TOUCH
			if (palm == 0) {
				if (w >= PALM_REPORT_WIDTH)
					w = PALM_REPORT_WIDTH - 10;
				minor_w = w;
			} else if (palm == 1) {	//palm report
				w = PALM_REPORT_WIDTH;
				minor_w = PALM_REPORT_WIDTH / 3;
			} else if (palm == 2){	// palm reject
				w = PALM_REJECT_WIDTH;
				minor_w = PALM_REJECT_WIDTH;
			}
#endif

			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);
			input_report_key(info->input_dev, BTN_TOUCH, 1);

			input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, (u32)w);
#ifdef CONFIG_SEC_FACTORY
			if (read_data(info->client, ZT75XX_REAL_WIDTH + i, (u8*)&z, 2) < 0)
				input_info(true, &client->dev, "Failed to read %d's Real width %s\n", i, __func__);
			z = z & 0x0f;
			if (z < 1)
				z = 1;
			input_report_abs(info->input_dev, ABS_MT_PRESSURE, (u32)z);
#endif
			input_report_abs(info->input_dev, ABS_MT_WIDTH_MAJOR,
						(u32)((palm == 1) ? (w - 40) : w));
#ifdef SUPPORTED_PALM_TOUCH
			input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, minor_w);
			input_report_abs(info->input_dev, ABS_MT_CUSTOM, (palm > 0) ? 1 : 0);
#endif

			input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);

			if (zinitix_bit_test(sub_status, SUB_BIT_DOWN)) {
				info->pressed_x[i] = x; /*for getting coordinates of pressed point*/
				info->pressed_y[i] = y;
				location_detect(info, location, x, y);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_info(true, &client->dev, "[P] tID:%d.%d x=%d y=%d %s w=%d p=%d (0x%x)\n",
							i, (info->input_dev->mt->trkid - 1) & TRKID_MAX,
							x, y, location, w, palm, ic_status);
#else
				input_info(true, &client->dev, "[P] tID:%d.%d %s w=%d p=%d (0x%x)\n",
							i, (info->input_dev->mt->trkid - 1) & TRKID_MAX,
							location, w, palm, ic_status);
#endif

				info->finger_cnt1++;

				if ((info->finger_cnt1 > 4) && (info->check_multi == 0)) {
					info->check_multi = 1;
					info->multi_count++;
					input_info(true, &client->dev, "%s: data : pn=%d mc=%d\n",
						__func__, info->finger_cnt1, info->multi_count);
				}
			} else if (zinitix_bit_test(sub_status, SUB_BIT_MOVE)) {
				info->move_count[i]++;
			}
		} else if (zinitix_bit_test(sub_status, SUB_BIT_UP) ||
				zinitix_bit_test(prev_sub_status, SUB_BIT_EXIST)) {
			location_detect(info, location, info->touch_info.coord[i].x, info->touch_info.coord[i].y);
			input_info(true, &client->dev, "[R] tID:%d %s dd:%d,%d mc=%d (%d)\n", i, location,
						info->touch_info.coord[i].x - info->pressed_x[i],
						info->touch_info.coord[i].y - info->pressed_y[i],
						info->move_count[i], __LINE__);

			if (info->finger_cnt1 > 0)
				info->finger_cnt1--;
			if (info->finger_cnt1 == 0) {
				input_report_key(info->input_dev, BTN_TOUCH, 0);
				info->check_multi = 0;
				info->print_info_cnt_release = 0;
			}
			memset(&info->touch_info.coord[i], 0x0, sizeof(struct coord));
			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
			info->move_count[i] = 0;
		} else {
			memset(&info->touch_info.coord[i], 0x0, sizeof(struct coord));
		}
	}
	memcpy((char *)&info->reported_touch_info, (char *)&info->touch_info,
			sizeof(struct point_info));
	input_sync(info->input_dev);

out:
	if (info->work_state == NORMAL) {
#if ESD_TIMER_INTERVAL
		esd_timer_start(CHECK_ESD_TIMER, info);
#endif
		info->work_state = NOTHING;
	}

	up(&info->work_lock);

	return IRQ_HANDLED;
}

#ifdef CONFIG_INPUT_ENABLED
static int zt75xx_ts_open(struct input_dev *dev)
{
	struct zt75xx_ts_info *info = input_get_drvdata(dev);
	u8 prev_work_state;

	if (info == NULL)
		return 0;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
		input_err(true, &info->client->dev, "%s: TUI cancel event call!\n", __func__);
		//msleep(100);
		tui_force_close(1);
		msleep(300);
		if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
			input_err(true, &info->client->dev, "%s: TUI flag force clear!\n", __func__);
			trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED | TRUSTEDUI_MODE_INPUT_SECURED);
			trustedui_set_mode(TRUSTEDUI_MODE_OFF);
		}
	}
#endif
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	secure_touch_stop(info, 0);
#endif

	if (!info->info_work_done) {
		input_err(true, &info->client->dev, "%s: not finished info work\n", __func__);
		return 0;
	}

	input_info(true, &info->client->dev, "%s: %d\n", __func__, __LINE__);

	if ((info->pdata->support_lpm_mode) && (info->sleep_mode)) {
		down(&info->work_lock);
		prev_work_state = info->work_state;
		info->work_state = SLEEP_MODE_OUT;

		input_info(true, &info->client->dev, "%s: wake up\n", __func__);
		write_cmd(info->client, 0x0A);
		write_cmd(info->client, ZT75XX_WAKEUP_CMD);
		info->sleep_mode = 0;

		zt75xx_set_optional_mode(info, false);
		write_cmd(info->client, 0x0B);
		info->work_state = prev_work_state;
		up(&info->work_lock);

#if ESD_TIMER_INTERVAL
		esd_timer_start(CHECK_ESD_TIMER, info);
#endif
		if (device_may_wakeup(&info->client->dev))
			disable_irq_wake(info->irq);
	} else {
		down(&info->work_lock);
		if (info->work_state != RESUME && info->work_state != EALRY_SUSPEND) {
			input_err(true, &info->client->dev, "%s: invalid work proceedure (%d)\n",
				__func__, info->work_state);
			up(&info->work_lock);
			return 0;
		}

		zt75xx_power_control(info, POWER_ON_SEQUENCE);

		if (!crc_check(info))
			goto fail_late_resume;

		if (mini_init_touch(info) == false)
			goto fail_late_resume;
		enable_irq(info->irq);
		info->work_state = NOTHING;

		if (g_ta_connected)
			zt75xx_set_ta_status(info);

		up(&info->work_lock);

		info->print_info_cnt_open = 0;
		info->print_info_cnt_release = 0;
		schedule_work(&info->work_print_info.work);

		return 0;
	}

fail_late_resume:
	input_err(true, &info->client->dev, "%s: failed to late resume\n", __func__);
	enable_irq(info->irq);
	info->work_state = NOTHING;
	up(&info->work_lock);

	info->print_info_cnt_open = 0;
	info->print_info_cnt_release = 0;
	schedule_work(&info->work_print_info.work);

	return 0;
}

static void zt75xx_ts_close(struct input_dev *dev)
{
	struct zt75xx_ts_info *info = input_get_drvdata(dev);
	int i;
	u8 prev_work_state;

	if (info == NULL)
		return;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
		input_err(true, &info->client->dev, "%s: TUI cancel event call!\n", __func__);
		//msleep(100);
		tui_force_close(1);
		msleep(300);
		if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
			input_err(true, &info->client->dev, "%s: TUI flag force clear!\n", __func__);
			trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED | TRUSTEDUI_MODE_INPUT_SECURED);
			trustedui_set_mode(TRUSTEDUI_MODE_OFF);
		}
	}
#endif
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	secure_touch_stop(info, 1);
#endif

	if (!info->info_work_done) {
		input_err(true, &info->client->dev, "%s: not finished info work\n", __func__);
		return;
	}

	input_info(true, &info->client->dev, "%s, spay:%d aod:%d prox:%d\n",
				__func__, info->spay_enable, info->aod_enable, info->prox_power_off);

#ifdef TCLM_CONCEPT
	sec_tclm_debug_info(info->tdata);
#endif

#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
#endif
	cancel_delayed_work(&info->work_print_info);
	zt75xx_print_info(info);

	if ((info->pdata->support_lpm_mode) &&
			(info->spay_enable ||info->aod_enable || info->aot_enable) &&
			(!info->prox_power_off)) {
		down(&info->work_lock);
		prev_work_state = info->work_state;
		info->work_state = SLEEP_MODE_IN;
		input_info(true, &info->client->dev, "%s: sleep mode\n", __func__);

#if ESD_TIMER_INTERVAL
		esd_timer_stop(info);
#endif
		input_info(true, &info->client->dev,
				"%s: lpm_mode_reg set flag 0x%02x\n", __func__, lpm_mode_reg.flag);

		write_cmd(info->client, 0x0A);
		if (write_reg(info->client, ZT75XX_LPM_MODE_REG, lpm_mode_reg.flag) != I2C_SUCCESS)
			input_info(true, &info->client->dev, "%s: fail lpm mode set\n", __func__);

		write_cmd(info->client, ZT75XX_SLEEP_CMD);
		info->sleep_mode = 1;

		/* clear garbage data */
		for (i = 0; i < 2; i++) {
			usleep_range(10 * 1000, 10 * 1000);
			write_cmd(info->client, ZT75XX_CLEAR_INT_STATUS_CMD);
		}
		clear_report_data(info);

		write_cmd(info->client, 0x0B);
		info->work_state = prev_work_state;
		if (device_may_wakeup(&info->client->dev))
			enable_irq_wake(info->irq);
	} else {
		disable_irq(info->irq);
		down(&info->work_lock);
		if (info->work_state != NOTHING) {
			input_err(true, &info->client->dev, "%s: invalid work proceedure (%d)\n",
				info->work_state);
			up(&info->work_lock);
			enable_irq(info->irq);
			return;
		}
		info->work_state = EALRY_SUSPEND;

		clear_report_data(info);

#if ESD_TIMER_INTERVAL
		/*write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);*/
		esd_timer_stop(info);
#endif
		zt75xx_power_control(info, POWER_OFF);
	}

	info->prox_power_off = 0;
	up(&info->work_lock);

	return;
}
#endif	/* CONFIG_INPUT_ENABLED */

#if defined(SEC_FACTORY_TEST) || defined(USE_MISC_DEVICE)
static int ts_set_touchmode(u16 value)
{
	int i, ret = 1;
	int retry_cnt = 0;
	struct zt75xx_ts_info *info = misc_info;
	struct capa_info *cap = &info->cap_info;

	disable_irq(info->irq);

	down(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_err(true, &info->client->dev, "%s: other process occupied.. (%d)\n",
			__func__, info->work_state);
		enable_irq(info->irq);
		up(&info->work_lock);
		return -1;
	}

retry_ts_set_touchmode:
	//wakeup cmd
	write_cmd(info->client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);
	write_cmd(info->client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);

	info->work_state = SET_MODE;

	if (value == TOUCH_SEC_MODE)
		info->touch_mode = TOUCH_POINT_MODE;
	else
		info->touch_mode = value;

	input_info(true, &info->client->dev, "%s: %d\n",
		__func__, info->touch_mode);

	if (info->touch_mode != TOUCH_POINT_MODE) {
		if (write_reg(info->client, ZT75XX_DELAY_RAW_FOR_HOST, RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set ZT75XX_DELAY_RAW_FOR_HOST\n", __func__);
	}

	if (write_reg(info->client, ZT75XX_TOUCH_MODE, info->touch_mode) != I2C_SUCCESS)
		input_err(true, &info->client->dev,
			"%s: Fail to set ZINITX_TOUCH_MODE %d\n",
			__func__, info->touch_mode);

	input_dbg(true, &info->client->dev, "%s: write regiter end\n", __func__);

	ret = read_data(info->client, ZT75XX_TOUCH_MODE, (u8 *)&cap->current_touch_mode, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev,"%s: fail touch mode read\n", __func__);
		goto out;
	}

	if (cap->current_touch_mode != info->touch_mode) {
		if (retry_cnt < 1) {
			retry_cnt++;
			goto retry_ts_set_touchmode;
		}
		input_err(true, &info->client->dev, "%s: fail to set touch_mode %d (current_touch_mode %d)\n",
				__func__, info->touch_mode, cap->current_touch_mode);
		ret = -1;
		goto out;
	}

	/* clear garbage data */
	for (i = 0; i < 10; i++) {
		usleep_range(20 * 1000, 20 * 1000);
		write_cmd(info->client, ZT75XX_CLEAR_INT_STATUS_CMD);
	}
	input_dbg(true, &info->client->dev, "%s: garbage data end\n", __func__);

out:
	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);

	return ret;
}
#endif

static int ts_set_touchmode3(u16 value)
{
	struct zt75xx_ts_info *info = misc_info;
	int i;

	disable_irq(info->irq);

	down(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_err(true, &info->client->dev, "%s: other process occupied.. (%d)\n",
			__func__, info->work_state);
		enable_irq(info->irq);
		up(&info->work_lock);
		return -1;
	}

	//wakeup cmd
	write_cmd(info->client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);
	write_cmd(info->client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);

	if (info->touch_mode == TOUCH_POINT_MODE) {
		/* factory data */
		read_data(info->client, ZT75XX_MUTUAL_AMP_V_SEL, (u8 *)&info->cap_info.mutual_amp_v_sel, 2);
		read_data(info->client, ZT75XX_SX_AMP_V_SEL, (u8 *)&info->cap_info.sx_amp_v_sel, 2);
		read_data(info->client, ZT75XX_SX_SUB_V_SEL, (u8 *)&info->cap_info.sx_sub_v_sel, 2);
		read_data(info->client, ZT75XX_SY_AMP_V_SEL, (u8 *)&info->cap_info.sy_amp_v_sel, 2);
		read_data(info->client, ZT75XX_SY_SUB_V_SEL, (u8 *)&info->cap_info.sy_sub_v_sel, 2);
		read_data(info->client, ZT75XX_AFE_FREQUENCY, (u8 *)&info->cap_info.afe_frequency, 2);
		read_data(info->client, ZT75XX_DND_SHIFT_VALUE, (u8 *)&info->cap_info.shift_value, 2);
	}
	info->work_state = SET_MODE;

	if (value == TOUCH_RXSHORT_MODE) {
		if (write_reg(info->client, ZT75XX_SY_AMP_V_SEL, SEC_SY_AMP_V_SEL) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SY_AMP_V_SEL %d\n",
				__func__, SEC_SY_AMP_V_SEL);
		if (write_reg(info->client, ZT75XX_SY_SUB_V_SEL, SEC_SY_SUB_V_SEL) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SY_SUB_V_SEL %d\n",
				__func__, SEC_SY_SUB_V_SEL);
		if (write_reg(info->client, ZT75XX_DND_N_COUNT, SEC_SHORT_N_COUNT) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SHORT_N_COUNT %d\n",
				__func__, SEC_SHORT_N_COUNT);
		if (write_reg(info->client, ZT75XX_DND_U_COUNT, SEC_SHORT_U_COUNT) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SHORT_U_COUNT %d\n",
				__func__, SEC_SHORT_U_COUNT);
	} else if (value == TOUCH_TXSHORT_MODE) {
		if (write_reg(info->client, ZT75XX_SX_AMP_V_SEL, SEC_SX_AMP_V_SEL) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SX_AMP_V_SEL %d\n",
				__func__, SEC_SX_AMP_V_SEL);
		if (write_reg(info->client, ZT75XX_SX_SUB_V_SEL, SEC_SX_SUB_V_SEL) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SX_SUB_V_SEL %d\n",
				__func__, SEC_SX_SUB_V_SEL);
		if (write_reg(info->client, ZT75XX_DND_N_COUNT, SEC_SHORT_N_COUNT) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SHORT_N_COUNT %d\n",
				__func__, SEC_SHORT_N_COUNT);
		if (write_reg(info->client, ZT75XX_DND_U_COUNT, SEC_SHORT_U_COUNT) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SHORT_U_COUNT %d\n",
				__func__, SEC_SHORT_U_COUNT);
	} else if (info->touch_mode == TOUCH_RXSHORT_MODE || info->touch_mode == TOUCH_TXSHORT_MODE) {
		if (write_reg(info->client, ZT75XX_MUTUAL_AMP_V_SEL, info->cap_info.mutual_amp_v_sel) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to reset ZT75XX_MUTUAL_AMP_V_SEL %d\n",
				__func__, info->cap_info.mutual_amp_v_sel);
		if (write_reg(info->client, ZT75XX_SY_AMP_V_SEL, info->cap_info.sy_amp_v_sel) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to reset ZT75XX_SY_AMP_V_SEL %d\n",
				__func__, info->cap_info.sy_amp_v_sel);
		if (write_reg(info->client, ZT75XX_SY_SUB_V_SEL, info->cap_info.sy_sub_v_sel) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to reset ZT75XX_SY_SUB_V_SEL %d\n",
				__func__, info->cap_info.sy_sub_v_sel);
		if (write_reg(info->client, ZT75XX_SX_AMP_V_SEL, info->cap_info.sx_amp_v_sel) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to reset ZT75XX_SX_AMP_V_SEL %d\n",
				__func__, info->cap_info.sx_amp_v_sel);
		if (write_reg(info->client, ZT75XX_SX_SUB_V_SEL, info->cap_info.sx_sub_v_sel) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to reset ZT75XX_SX_SUB_V_SEL %d\n",
				__func__, info->cap_info.sx_sub_v_sel);
		if (write_reg(info->client, ZT75XX_DND_SHIFT_VALUE, info->cap_info.shift_value) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to reset ZT75XX_DND_SHIFT_VALUE %d\n",
				__func__, info->cap_info.shift_value);
		if (write_reg(info->client, ZT75XX_AFE_FREQUENCY, info->cap_info.afe_frequency) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to reset ZT75XX_AFE_FREQUENCY %d\n",
				__func__, info->cap_info.afe_frequency);
	}
	if (value == TOUCH_SEC_MODE)
		info->touch_mode = TOUCH_POINT_MODE;
	else
		info->touch_mode = value;

	input_info(true, &info->client->dev,
		"%s: touchkey_testmode = %d\n", __func__, info->touch_mode);

	if (info->touch_mode != TOUCH_POINT_MODE) {
		if (write_reg(info->client, ZT75XX_DELAY_RAW_FOR_HOST, RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set ZT75XX_DELAY_RAW_FOR_HOST\n", __func__);
	}

	if (write_reg(info->client, ZT75XX_TOUCH_MODE, info->touch_mode) != I2C_SUCCESS)
		input_err(true, &info->client->dev,
			"%s: Fail to set ZINITX_TOUCH_MODE %d\n",
			__func__, info->touch_mode);

	/* clear garbage data */
	for (i = 0; i < 10; i++) {
		usleep_range(20 * 1000, 20 * 1000);
		write_cmd(info->client, ZT75XX_CLEAR_INT_STATUS_CMD);
	}

	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);

	return 1;
}

static int ts_set_self_sat_touchmode(u16 value)
{
	struct zt75xx_ts_info *info = misc_info;
	int i;

	disable_irq(info->irq);

	down(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_err(true, &info->client->dev, "%s: other process occupied.. (%d)\n",
			__func__, info->work_state);
		enable_irq(info->irq);
		up(&info->work_lock);
		return -1;
	}

	//wakeup cmd
	write_cmd(info->client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);
	write_cmd(info->client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);

	if (value == TOUCH_SELF_DND_MODE) {
		if (write_reg(info->client, ZT75XX_SY_SAT_FREQUENCY, SEC_SY_SAT_FREQUENCY) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SY_AMP_V_SEL %d\n",
				__func__, SEC_SY_SAT_FREQUENCY);

		if (write_reg(info->client, ZT75XX_SY_SAT_N_COUNT, SEC_SY_SAT_N_COUNT) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SY_SUB_V_SEL %d\n",
				__func__, SEC_SY_SAT_N_COUNT);

		if (write_reg(info->client, ZT75XX_SY_SAT_U_COUNT, SEC_SY_SAT_U_COUNT) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SHORT_N_COUNT %d\n",
				__func__, SEC_SY_SAT_U_COUNT);

		if (write_reg(info->client, ZT75XX_SY_SAT_RBG_SEL, SEC_SY_SAT_RBG_SEL) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SHORT_U_COUNT %d\n",
				__func__, SEC_SY_SAT_RBG_SEL);

		if (write_reg(info->client, ZT75XX_SY_SAT_AMP_V_SEL, SEC_SY_SAT_AMP_V_SEL) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SHORT_N_COUNT %d\n",
				__func__, SEC_SY_SAT_AMP_V_SEL);

		if (write_reg(info->client, ZT75XX_SY_SAT_SUB_V_SEL, SEC_SY_SAT_SUB_V_SEL) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SHORT_U_COUNT %d\n",
				__func__, SEC_SY_SAT_SUB_V_SEL);

		if (write_reg(info->client, ZT75XX_SY_SAT2_FREQUENCY, SEC_SY_SAT2_FREQUENCY) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SHORT_N_COUNT %d\n",
				__func__, SEC_SY_SAT2_FREQUENCY);

		if (write_reg(info->client, ZT75XX_SY_SAT2_N_COUNT, SEC_SY_SAT2_N_COUNT) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SHORT_U_COUNT %d\n",
				__func__, SEC_SY_SAT2_N_COUNT);

		if (write_reg(info->client, ZT75XX_SY_SAT2_U_COUNT, SEC_SY_SAT2_U_COUNT) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SHORT_N_COUNT %d\n",
				__func__, SEC_SY_SAT2_U_COUNT);

		if (write_reg(info->client, ZT75XX_SY_SAT2_RBG_SEL, SEC_SY_SAT2_RBG_SEL) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SHORT_U_COUNT %d\n",
				__func__, SEC_SY_SAT2_RBG_SEL);

		if (write_reg(info->client, ZT75XX_SY_SAT2_AMP_V_SEL, SEC_SY_SAT2_AMP_V_SEL) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SHORT_N_COUNT %d\n",
				__func__, SEC_SY_SAT2_AMP_V_SEL);

		if (write_reg(info->client, ZT75XX_SY_SAT2_SUB_V_SEL, SEC_SY_SAT2_SUB_V_SEL) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set SEC_SHORT_U_COUNT %d\n",
				__func__, SEC_SY_SAT2_SUB_V_SEL);
	}

	if (value == TOUCH_SEC_MODE)
		info->touch_mode = TOUCH_POINT_MODE;
	else
		info->touch_mode = value;

	input_info(true, &info->client->dev,
		"%s: ts_set_self_sat_touchmode, touchkey_testmode = %d\n", __func__, info->touch_mode);

	if (info->touch_mode != TOUCH_POINT_MODE) {
		if (write_reg(info->client, ZT75XX_DELAY_RAW_FOR_HOST, RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS)
			input_err(true, &info->client->dev,
				"%s: Fail to set ZT75XX_DELAY_RAW_FOR_HOST\n", __func__);
	}

	if (write_reg(info->client, ZT75XX_TOUCH_MODE, info->touch_mode) != I2C_SUCCESS)
		input_err(true, &info->client->dev,
			"%s: Fail to set ZINITX_TOUCH_MODE %d\n",
			__func__, info->touch_mode);

	/* clear garbage data */
	for (i = 0; i < 10; i++) {
		usleep_range(20 * 1000, 20 * 1000);
		write_cmd(info->client, ZT75XX_CLEAR_INT_STATUS_CMD);
	}

	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);

	return 1;
}

#if defined(SEC_FACTORY_TEST) || defined(USE_MISC_DEVICE)
static int ts_upgrade_sequence(struct zt75xx_ts_info *info, const u8 *firmware_data, int restore_cal)
{
	int ret = 0;

	disable_irq(info->irq);
	down(&info->work_lock);
	info->work_state = UPGRADE;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
#endif
	clear_report_data(info);

	input_info(true, &info->client->dev, "%s: start upgrade firmware\n", __func__);
	if (!ts_upgrade_firmware(info, firmware_data, info->cap_info.ic_fw_size)) {
		ret = -1;
		goto out;
	}

	if (ic_version_check(info) < 0)
		input_err(true, &info->client->dev, "%s: failed ic version check\n", __func__);

#ifdef TCLM_CONCEPT
	if (restore_cal == 1) {
		input_err(true, &info->client->dev, "%s: RUN OFFSET CALIBRATION\n", __func__);
		ret = sec_execute_tclm_package(info->tdata, 0);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: sec_execute_tclm_package fail\n", __func__);
			goto out;
		}
	}
#else
	if (!ts_hw_calibration(info)) {
		ret = -1;
		goto out;
	}
#endif

	if (!mini_init_touch(info)) {
		ret = -1;
		goto out;
	}

out:
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &info->client->dev, "%s: Started esd timer\n", __func__);
#endif
#endif
	enable_irq(info->irq);
	info->work_state = NOTHING;
	up(&info->work_lock);

	return ret;
}
#endif

#ifdef SEC_FACTORY_TEST
static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	const u8 *buff = 0;
	mm_segment_t old_fs = {0};
	struct file *fp = NULL;
	long fsize = 0, nread = 0;
	char fw_path[MAX_FW_PATH+1];
	char result[16] = {0};
	const struct firmware *tsp_fw = NULL;
	unsigned char *fw_data = NULL;
	int restore_cal = 0;
	int ret;

	sec_cmd_set_default_result(sec);

	switch (sec->cmd_param[0]) {
	case BUILT_IN:
		if (!pdata->firmware_name) {
			input_err(true, &client->dev, "%s: firmware_name is NULL\n", __func__);
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			snprintf(result, sizeof(result), "%s", "NG");
			goto err;
		}

		snprintf(fw_path, MAX_FW_PATH, "%s", pdata->firmware_name);

		ret = request_firmware(&tsp_fw, fw_path, &(client->dev));
		if (ret) {
			input_err(true, &client->dev,
				"%s: Firmware image %s not available\n", __func__, fw_path);
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			snprintf(result, sizeof(result), "%s", "NG");
			goto err;
 		}
		fw_data = (unsigned char *)tsp_fw->data;

#ifdef TCLM_CONCEPT
		sec_tclm_root_of_cal(info->tdata, CALPOSITION_TESTMODE);
		restore_cal = 1;
#endif
		ret = ts_upgrade_sequence(info, (u8*)fw_data, restore_cal);
		release_firmware(tsp_fw);
		if (ret < 0) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			snprintf(result, sizeof(result), "%s", "NG");
			goto err;
		}
		break;

	case UMS:
		old_fs = get_fs();
		set_fs(get_ds());

		snprintf(fw_path, MAX_FW_PATH, "/sdcard/Firmware/TSP/%s", TSP_FW_FILENAME);
		fp = filp_open(fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			input_err(true, &client->dev,
				"file %s open error\n", fw_path);
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			snprintf(result, sizeof(result), "%s", "NG");
			goto err_open;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		if (fsize != info->cap_info.ic_fw_size) {
			input_err(true, &client->dev, "%s: invalid fw size!!\n", __func__);
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			snprintf(result, sizeof(result), "%s", "NG");
			goto err_open;
		}

		buff = kzalloc((size_t)fsize, GFP_KERNEL);
		if (!buff) {
			input_err(true, &client->dev, "%s: failed to alloc buffer for fw\n", __func__);
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			snprintf(result, sizeof(result), "%s", "NG");
			goto err_alloc;
		}

		nread = vfs_read(fp, (char __user *)buff, fsize, &fp->f_pos);
		if (nread != fsize) {
			input_err(true, &client->dev,
					"%s: failed to read firmware file, nread %ld != %ld Bytes\n",
					__func__, nread, fsize);
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			snprintf(result, sizeof(result), "%s", "NG");
			goto err_fw_size;
		}
		input_info(true, &client->dev, "%s: ums fw is loaded!!\n", __func__);

#ifdef TCLM_CONCEPT
		sec_tclm_root_of_cal(info->tdata, CALPOSITION_TESTMODE);
		restore_cal = 1;
#endif
		ret = ts_upgrade_sequence(info, (u8*)buff, restore_cal);
		if (ret < 0) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			snprintf(result, sizeof(result), "%s", "NG");
			goto err_fw_size;
		}
		break;

	default:
		input_err(true, &client->dev, "%s: invalid fw file type!!\n", __func__);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(result, sizeof(result), "%s", "NG");
		goto err;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	snprintf(result, sizeof(result), "%s", "OK");

	if (fp != NULL) {
err_fw_size:
		kfree(buff);
err_alloc:
		filp_close(fp, NULL);
err_open:
		set_fs(old_fs);
	}
err:
#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(info->tdata, CALPOSITION_NONE);
#endif
	sec_cmd_set_cmd_result(sec, result, strnlen(result, sizeof(result)));
	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	char buff[16] = { 0 };
	u32 version;

	sec_cmd_set_default_result(sec);

	/* ZI123456: ZI[12][3][4][56] */
	version = ((info->fw_hw_id_bin & 0xff) << 16)
		| ((info->fw_ver_bin & 0xf) << 12) | ((info->fw_minor_ver_bin & 0xf) << 8)
		| ((info->fw_reg_ver_bin & 0xff));

	snprintf(buff, sizeof(buff), "ZI%06X", version);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[16] = { 0 };
	u16 fw_version, fw_minor_version, reg_version, hw_id, vendor_id;
	u32 version, length;
	int ret;

	sec_cmd_set_default_result(sec);

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif

	down(&info->work_lock);
	//wakeup cmd
	write_cmd(info->client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);
	write_cmd(info->client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);

	ret = ic_version_check(info);
	up(&info->work_lock);
	if (ret < 0) {
		input_err(true, &client->dev, "%s: version check error\n", __func__);
		return;
	}

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif

	fw_version = info->cap_info.fw_version;
	fw_minor_version = info->cap_info.fw_minor_version;
	reg_version = info->cap_info.reg_data_version;
	hw_id = info->cap_info.hw_id;
	vendor_id = ntohs(info->cap_info.vendor_id);
	version = (u32)((u32)(hw_id & 0xff) << 16) | ((fw_version & 0xf) << 12)
				| ((fw_minor_version & 0xf) << 8) | (reg_version & 0xff);

	length = sizeof(vendor_id);
	snprintf(buff, length + 1, "%s", (u8 *)&vendor_id);
	snprintf(buff + length, sizeof(buff) - length, "%06X", version);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void get_checksum_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[16] = { 0 };
	u16 checksum;

	sec_cmd_set_default_result(sec);

	read_data(client, ZT75XX_CHECKSUM, (u8 *)&checksum, 2);

	snprintf(buff, sizeof(buff), "0x%X", checksum);
	input_info(true, &client->dev, "%s: %d %x\n",__func__, checksum, checksum);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void get_threshold(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[20] = { 0 };

	sec_cmd_set_default_result(sec);

	read_data(client, ZT75XX_THRESHOLD, (u8 *)&info->cap_info.threshold, 2);

	snprintf(buff, sizeof(buff), "%d", info->cap_info.threshold);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void module_off_master(void *device_data)
{
	return;
}

static void module_on_master(void *device_data)
{
	return;
}

static void module_off_slave(void *device_data)
{
	return;
}

static void module_on_slave(void *device_data)
{
	return;
}

static void get_module_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[16] = {0};

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff),  "%s", tostring(NA));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}


#define ZT75XX_VENDOR_NAME "ZINITIX"

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s", ZT75XX_VENDOR_NAME);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_VENDOR");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

#define ZT75XX_CHIP_NAME "ZT75XX"

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	const char *name_buff;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (pdata->chip_name)
		name_buff = pdata->chip_name;
	else
		name_buff = ZT75XX_CHIP_NAME;

	snprintf(buff, sizeof(buff), "%s", name_buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_NAME");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	write_cmd(info->client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);

	read_data(client, ZT75XX_TOTAL_NUMBER_OF_X, (u8 *)&info->cap_info.x_node_num, 2);

	snprintf(buff, sizeof(buff), "%u", info->cap_info.x_node_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	write_cmd(info->client, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);

	read_data(client, ZT75XX_TOTAL_NUMBER_OF_Y, (u8 *)&info->cap_info.y_node_num, 2);

	snprintf(buff, sizeof(buff), "%u", info->cap_info.y_node_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[2] = { 0 };

	sec_cmd_set_default_result(sec);

	sprintf(sec->cmd_result, "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;

	sec_cmd_set_cmd_exit(sec);

	input_info(true, &client->dev, "%s: \"%s(%d)\"\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_dnd_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 min, max;
	s32 i, j;
	int ret;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(TOUCH_DND_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->dnd_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	min = 0xFFFF;
	max = 0x0000;

	input_info(true, &client->dev, "%s: start\n", __func__);
	for (i = 0; i < info->cap_info.x_node_num; i++) {
		pr_info("%s ", SECLOG);
		for (j = 0; j < info->cap_info.y_node_num; j++) {
			pr_cont("%d ", raw_data->dnd_data[i * info->cap_info.y_node_num + j]);

			if (raw_data->dnd_data[i * info->cap_info.y_node_num + j] < min &&
				raw_data->dnd_data[i * info->cap_info.y_node_num + j] != 0)
				min = raw_data->dnd_data[i * info->cap_info.y_node_num + j];

			if (raw_data->dnd_data[i * info->cap_info.y_node_num + j] > max)
				max = raw_data->dnd_data[i * info->cap_info.y_node_num + j];
		}
		pr_cont("\n");
	}
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "DND");
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "FAIL");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "DND");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
				(int)strlen(sec->cmd_result));

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void get_dnd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned int val;
	int x_node, y_node;
	int node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
			y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buff, sizeof(buff), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->dnd_data[node_num];
	snprintf(buff, sizeof(buff), "%u", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_dnd_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[16] = { 0 };
	char all_cmdbuff[info->cap_info.x_node_num*info->cap_info.y_node_num * 6];
	s32 i, j;
	int ret;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(info->client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(TOUCH_DND_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->dnd_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	memset(all_cmdbuff, 0, sizeof(char) * (info->cap_info.x_node_num * info->cap_info.y_node_num * 6));	//size 6  ex(12000,)

	for (i = 0; i < info->cap_info.x_node_num; i++) {
		for (j = 0; j < info->cap_info.y_node_num; j++) {
			sprintf(buff, "%u,", raw_data->dnd_data[i * info->cap_info.y_node_num + j]);
			strcat(all_cmdbuff, buff);
		}
	}

	sec_cmd_set_cmd_result(sec, all_cmdbuff,
			strnlen(all_cmdbuff, sizeof(all_cmdbuff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "FAIL");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void run_dnd_v_gap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char buff_onecmd_1[SEC_CMD_STR_LEN] = { 0 };
	char buff_onecmd_2[SEC_CMD_STR_LEN] = { 0 };
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val;
	u16 screen_max = 0x0000;
	u16 touchkey_max = 0x0000;

	sec_cmd_set_default_result(sec);

	memset(raw_data->vgap_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &client->dev, "%s: start\n", __func__);
	for (i = 0; i < x_num - 1; i++) {
		pr_info("%s ", SECLOG);
		for (j = 0; j < y_num; j++) {
			offset = (i * y_num) + j;

			cur_val = raw_data->dnd_data[offset];
			next_val = raw_data->dnd_data[offset + y_num];
			if (!next_val) {
				raw_data->vgap_data[offset] = next_val;
				continue;
			}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
			else
				val = 100 - ((next_val * 100) / cur_val);

			pr_cont("%2d ", val);

			raw_data->vgap_data[offset] = val;

			if (pdata->support_touchkey) {
				if (i < x_num - 2) {
					if (raw_data->vgap_data[i * y_num + j] > screen_max)
						screen_max = raw_data->vgap_data[i * y_num + j];
				} else {
					if (raw_data->vgap_data[i * y_num + j] > touchkey_max)
						touchkey_max = raw_data->vgap_data[i * y_num + j];
				}
			} else {
				if (raw_data->vgap_data[i * y_num + j] > screen_max)
					screen_max = raw_data->vgap_data[i * y_num + j];
			}
		}
		pr_cont("\n");
	}
	if (pdata->support_touchkey) {
		input_info(true, &client->dev, "%s: screen_max %d, touchkey_max %d\n", __func__, screen_max, touchkey_max);
		snprintf(buff, sizeof(buff), "%d,%d", screen_max, touchkey_max);
		snprintf(buff_onecmd_1, sizeof(buff_onecmd_1), "%d,%d", 0, screen_max);
		snprintf(buff_onecmd_2, sizeof(buff_onecmd_2), "%d,%d", 0, touchkey_max);
	} else {
		input_info(true, &client->dev, "%s: screen_max %d\n", __func__, screen_max);
		snprintf(buff, sizeof(buff), "%d", screen_max);
		snprintf(buff_onecmd_1, sizeof(buff_onecmd_1), "%d,%d", 0, screen_max);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		if (pdata->support_touchkey) {
			sec_cmd_set_cmd_result_all(sec, buff_onecmd_1, strnlen(buff_onecmd_1, sizeof(buff_onecmd_1)), "DND_V_GAP_SCREEN");
			sec_cmd_set_cmd_result_all(sec, buff_onecmd_2, strnlen(buff_onecmd_2, sizeof(buff_onecmd_2)), "DND_V_GAP_TOUCHKEY");
		} else {
			sec_cmd_set_cmd_result_all(sec, buff_onecmd_1, strnlen(buff_onecmd_1, sizeof(buff_onecmd_1)), "DND_V_GAP");
		}
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void run_dnd_h_gap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char buff_onecmd_1[SEC_CMD_STR_LEN] = { 0 };
	char buff_onecmd_2[SEC_CMD_STR_LEN] = { 0 };
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val;
	u16 screen_max = 0x0000;
	u16 touchkey_max = 0x0000;

	sec_cmd_set_default_result(sec);

	memset(raw_data->hgap_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &client->dev, "%s: start\n", __func__);
	for (i = 0; i < x_num ; i++) {
		pr_info("%s ", SECLOG);
		for (j = 0; j < y_num - 1; j++) {
			offset = (i * y_num) + j;

			cur_val = raw_data->dnd_data[offset];
			if (!cur_val) {
				raw_data->hgap_data[offset] = cur_val;
				continue;
			}

			next_val = raw_data->dnd_data[offset + 1];
			if (!next_val) {
				raw_data->hgap_data[offset] = next_val;
				for (++j; j < y_num - 1; j++) {
					offset = (i * y_num) + j;

					next_val = raw_data->dnd_data[offset];
					if (!next_val) {
						raw_data->hgap_data[offset] = next_val;
						continue;
					}
					break;
				}
			}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
			else
				val = 100 - ((next_val * 100) / cur_val);

			pr_cont("%2d ", val);
			raw_data->hgap_data[offset] = val;

			if (pdata->support_touchkey) {
				if (i < x_num - 1) {
					if (raw_data->hgap_data[i * y_num + j] > screen_max)
						screen_max = raw_data->hgap_data[i * y_num + j];
				} else {
					if (raw_data->hgap_data[i * y_num + j] > touchkey_max)
						touchkey_max = raw_data->hgap_data[i * y_num + j];
				}
			} else {
				if (raw_data->hgap_data[i * y_num + j] > screen_max)
					screen_max = raw_data->hgap_data[i * y_num + j];
			}
		}
		pr_cont("\n");
	}
	if (pdata->support_touchkey) {
		input_info(true, &client->dev, "%s: screen_max %d, touchkey_max %d\n", __func__, screen_max, touchkey_max);
		snprintf(buff, sizeof(buff), "%d,%d", screen_max, touchkey_max);
		snprintf(buff_onecmd_1, sizeof(buff_onecmd_1), "%d,%d", 0, screen_max);
		snprintf(buff_onecmd_2, sizeof(buff_onecmd_2), "%d,%d", 0, touchkey_max);
	} else {
		input_info(true, &client->dev, "%s: screen_max %d\n", __func__, screen_max);
		snprintf(buff, sizeof(buff), "%d", screen_max);
		snprintf(buff_onecmd_1, sizeof(buff_onecmd_1), "%d,%d", 0, screen_max);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		if (pdata->support_touchkey) {
			sec_cmd_set_cmd_result_all(sec, buff_onecmd_1, strnlen(buff_onecmd_1, sizeof(buff_onecmd_1)), "DND_H_GAP_SCREEN");
			sec_cmd_set_cmd_result_all(sec, buff_onecmd_2, strnlen(buff_onecmd_2, sizeof(buff_onecmd_2)), "DND_H_GAP_TOUCHKEY");
		} else {
			sec_cmd_set_cmd_result_all(sec, buff_onecmd_1, strnlen(buff_onecmd_1, sizeof(buff_onecmd_1)), "DND_H_GAP");
		}
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_dnd_h_gap(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int x_node, y_node;
	int node_num;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= x_num || y_node < 0 || y_node >= y_num - 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	node_num = (x_node * y_num) + y_node;

	snprintf(buff, sizeof(buff), "%d", raw_data->hgap_data[node_num]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

#define CMD_RESULT_WORD_LEN	10
static void run_dnd_h_gap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.x_node_num * info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val;

	sec_cmd_set_default_result(sec);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	input_info(true, &info->client->dev, "%s: start\n", __func__);
	for (i = 0; i < info->cap_info.x_node_num; i++) {
		pr_info("%s ", SECLOG);
		for (j = 0; j < info->cap_info.y_node_num - 1; j++) {
			offset = (i * info->cap_info.y_node_num) + j;

			cur_val = raw_data->dnd_data[offset];
			if (!cur_val) {
				raw_data->hgap_data[offset] = cur_val;
				continue;
			}

			next_val = raw_data->dnd_data[offset + 1];
			if (!next_val) {
				raw_data->hgap_data[offset] = next_val;
				for (++j; j < info->cap_info.y_node_num - 1; j++) {
					offset = (i * info->cap_info.y_node_num) + j;

					next_val = raw_data->dnd_data[offset];
					if (!next_val) {
						raw_data->hgap_data[offset] = next_val;
						continue;
					}
					break;
				}
			}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
			else
				val = 100 - ((next_val * 100) / cur_val);

			pr_cont("%2d ", val);
			raw_data->hgap_data[offset] = val;

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->hgap_data[offset]);
			strncat(buff, temp, CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
		pr_cont("\n");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void get_dnd_v_gap(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int x_node, y_node;
	int node_num;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= x_num - 1 || y_node < 0 || y_node >= y_num) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	node_num = (x_node * y_num) + y_node;

	sprintf(buff, "%d", raw_data->vgap_data[node_num]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_dnd_v_gap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.x_node_num * info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val;

	sec_cmd_set_default_result(sec);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	input_info(true, &info->client->dev, "%s: start\n", __func__);
	for (i = 0; i < info->cap_info.x_node_num - 1; i++) {
		pr_info("%s ", SECLOG);
		for (j = 0; j < info->cap_info.y_node_num; j++) {
			offset = (i * info->cap_info.y_node_num) + j;

			cur_val = raw_data->dnd_data[offset];
			next_val = raw_data->dnd_data[offset + info->cap_info.y_node_num];
			if (!next_val) {
				raw_data->vgap_data[offset] = next_val;
				continue;
			}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
			else
				val = 100 - ((next_val * 100) / cur_val);

			pr_cont("%2d ", val);
			raw_data->vgap_data[offset] = val;

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->vgap_data[offset]);
			strncat(buff, temp, CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
		pr_cont("\n");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void run_delta_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	s16 min, max;
	s32 i, j;
	int ret;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(TOUCH_DELTA_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->delta_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	min = (s16)0x7FFF;
	max = (s16)0x8000;

	input_info(true, &client->dev, "%s: start\n", __func__);
	for (i = 0; i < info->cap_info.x_node_num; i++) {
		pr_info("%s ", SECLOG);
		for (j = 0; j < info->cap_info.y_node_num; j++) {
			pr_cont("%4d ", raw_data->delta_data[j + i]);

			if (raw_data->delta_data[i * info->cap_info.y_node_num + j] < min &&
				raw_data->delta_data[i * info->cap_info.y_node_num + j] != 0)
				min = raw_data->delta_data[i * info->cap_info.y_node_num + j];

			if (raw_data->delta_data[i * info->cap_info.y_node_num + j] > max)
				max = raw_data->delta_data[i * info->cap_info.y_node_num + j];

		}
		pr_cont("\n");
	}

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "FAIL");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
				(int)strlen(sec->cmd_result));
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void get_delta(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned int val;
	int x_node, y_node;
	int node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
			y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buff, sizeof(buff), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->delta_data[node_num];
	snprintf(buff, sizeof(buff), "%u", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_hfdnd_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset;
	u16 min = 0xFFFF, max = 0x0000;
	int ret;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(TOUCH_HFDND_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->hfdnd_data, 2);
	ts_set_touchmode(TOUCH_POINT_MODE);

	input_info(true, &client->dev, "%s: start\n", __func__);
	for (i = 0; i < x_num; i++) {
		pr_info("%s ", SECLOG);
		for (j = 0; j < y_num; j++) {
			offset = (i * y_num) + j;
			pr_cont("%d ", raw_data->hfdnd_data[offset]);
			if (raw_data->hfdnd_data[offset] < min && raw_data->hfdnd_data[offset] != 0)
				min = raw_data->hfdnd_data[offset];
			if (raw_data->hfdnd_data[offset] > max)
				max = raw_data->hfdnd_data[offset];
		}
		pr_cont("\n");
	}

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "HF_DND");
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "FAIL");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "HF_DND");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void get_hfdnd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned int val;
	int x_node, y_node;
	int node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
			y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buff, sizeof(buff), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->hfdnd_data[node_num];
	snprintf(buff, sizeof(buff), "%u", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true,&client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
		(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}


static void run_hfdnd_v_gap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char buff_onecmd_1[SEC_CMD_STR_LEN] = { 0 };
	char buff_onecmd_2[SEC_CMD_STR_LEN] = { 0 };
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val;
	u16 screen_max = 0x0000;
	u16 touchkey_max = 0x0000;

	sec_cmd_set_default_result(sec);

	memset(raw_data->vgap_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &info->client->dev, "%s: start\n", __func__);
	for (i = 0; i < x_num - 1; i++) {
		pr_info("%s ", SECLOG);
		for (j = 0; j < y_num; j++) {
			offset = (i * y_num) + j;

			cur_val = raw_data->hfdnd_data[offset];
			next_val = raw_data->hfdnd_data[offset + y_num];
			if (!next_val) {
				raw_data->vgap_data[offset] = next_val;
				continue;
			}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
			else
				val = 100 - ((next_val * 100) / cur_val);

			pr_cont("%2d ", val);
			raw_data->vgap_data[offset] = val;

			if (pdata->support_touchkey) {
				if (i < x_num - 1) {
					if (raw_data->vgap_data[i * y_num + j] > screen_max)
						screen_max = raw_data->vgap_data[i * y_num + j];
				} else {
					if (raw_data->vgap_data[i * y_num + j] > touchkey_max)
						touchkey_max = raw_data->vgap_data[i * y_num + j];
				}
			} else{
				if (raw_data->vgap_data[i * y_num + j] > screen_max)
					screen_max = raw_data->vgap_data[i * y_num + j];
			}
		}
		pr_cont("\n");
	}

	if (pdata->support_touchkey) {
		input_info(true, &info->client->dev, "%s: screen_max %d, touchkey_max %d\n", __func__, screen_max, touchkey_max);
		snprintf(buff, sizeof(buff), "%d,%d", screen_max, touchkey_max);
		snprintf(buff_onecmd_1, sizeof(buff_onecmd_1), "%d,%d", 0, screen_max);
		snprintf(buff_onecmd_2, sizeof(buff_onecmd_2), "%d,%d", 0, touchkey_max);
	} else {
		input_info(true, &info->client->dev, "%s: screen_max %d\n", __func__, screen_max);
		snprintf(buff, sizeof(buff), "%d", screen_max);
		snprintf(buff_onecmd_1, sizeof(buff_onecmd_1), "%d,%d", 0, screen_max);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		if (pdata->support_touchkey) {
			sec_cmd_set_cmd_result_all(sec, buff_onecmd_1, strnlen(buff_onecmd_1, sizeof(buff_onecmd_1)), "HF_DND_V_GAP_SCREEN");
			sec_cmd_set_cmd_result_all(sec, buff_onecmd_2, strnlen(buff_onecmd_2, sizeof(buff_onecmd_2)), "HF_DND_V_GAP_TOUCHKEY");
		} else {
			sec_cmd_set_cmd_result_all(sec, buff_onecmd_1, strnlen(buff_onecmd_1, sizeof(buff_onecmd_1)), "HF_DND_V_GAP");
		}
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void run_hfdnd_h_gap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char buff_onecmd_1[SEC_CMD_STR_LEN] = { 0 };
	char buff_onecmd_2[SEC_CMD_STR_LEN] = { 0 };
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val;
	u16 screen_max = 0x0000;
	u16 touchkey_max = 0x0000;

	sec_cmd_set_default_result(sec);

	memset(raw_data->hgap_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &info->client->dev, "%s: start\n", __func__);
	for (i = 0; i < x_num ; i++) {
		pr_info("%s ", SECLOG);
		for (j = 0; j < y_num-1; j++) {
			offset = (i * y_num) + j;

			cur_val = raw_data->hfdnd_data[offset];
			if (!cur_val) {
				raw_data->hgap_data[offset] = cur_val;
				continue;
			}

			next_val = raw_data->hfdnd_data[offset + 1];
			if (!next_val) {
				raw_data->hgap_data[offset] = next_val;
				for (++j; j < y_num - 1; j++) {
					offset = (i * y_num) + j;

					next_val = raw_data->hfdnd_data[offset];
					if (!next_val) {
						raw_data->hgap_data[offset]
							= next_val;
						continue;
					}
					break;
				}
			}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
			else
				val = 100 - ((next_val * 100) / cur_val);

			pr_cont("%2d ", val);
			raw_data->hgap_data[offset] = val;

			if (pdata->support_touchkey) {
				if (i < x_num - 1) {
					if (raw_data->hgap_data[i * y_num + j] > screen_max)
						screen_max = raw_data->hgap_data[i * y_num + j];
				} else {
					if (raw_data->hgap_data[i * y_num + j] > touchkey_max)
						touchkey_max = raw_data->hgap_data[i * y_num + j];
				}
			} else {
				if (raw_data->hgap_data[i * y_num + j] > screen_max)
					screen_max = raw_data->hgap_data[i * y_num + j];
			}
		}
		pr_cont("\n");
	}

	if (pdata->support_touchkey) {
		input_info(true, &info->client->dev, "%s: screen_max %d, touchkey_max %d\n", __func__, screen_max, touchkey_max);
		snprintf(buff, sizeof(buff), "%d,%d", screen_max, touchkey_max);
		snprintf(buff_onecmd_1, sizeof(buff_onecmd_1), "%d,%d", 0, screen_max);
		snprintf(buff_onecmd_2, sizeof(buff_onecmd_2), "%d,%d", 0, touchkey_max);
	} else {
		input_info(true, &info->client->dev, "%s: screen_max %d\n", __func__, screen_max);
		snprintf(buff, sizeof(buff), "%d", screen_max);
		snprintf(buff_onecmd_1, sizeof(buff_onecmd_1), "%d,%d", 0, screen_max);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		if (pdata->support_touchkey) {
			sec_cmd_set_cmd_result_all(sec, buff_onecmd_1, strnlen(buff_onecmd_1, sizeof(buff_onecmd_1)), "HF_DND_H_GAP_SCREEN");
			sec_cmd_set_cmd_result_all(sec, buff_onecmd_2, strnlen(buff_onecmd_2, sizeof(buff_onecmd_2)), "HF_DND_H_GAP_TOUCHKEY");
		} else {
			sec_cmd_set_cmd_result_all(sec, buff_onecmd_1, strnlen(buff_onecmd_1, sizeof(buff_onecmd_1)), "HF_DND_H_GAP");
		}
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_hfdnd_h_gap(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int x_node, y_node;
	int node_num;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= x_num || y_node < 0 || y_node >= y_num - 1) {
		snprintf(buff, sizeof(buff), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	node_num = (x_node * y_num) + y_node;

	snprintf(buff, sizeof(buff), "%d", raw_data->hgap_data[node_num]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true,&info->client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
		(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void get_hfdnd_v_gap(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int x_node, y_node;
	int node_num;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= x_num - 1 || y_node < 0 || y_node >= y_num) {
		snprintf(buff, sizeof(buff), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	node_num = (x_node * y_num) + y_node;

	snprintf(buff, sizeof(buff), "%d", raw_data->vgap_data[node_num]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true,&info->client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
		(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_rxshort_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int y_num = info->cap_info.y_node_num;
	int i, touchkey_node = 2;
	u16 screen_max = 0x0000, touchkey_max = 0x0000;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	sec_cmd_set_default_result(sec);

	ts_set_touchmode3(TOUCH_RXSHORT_MODE);
	get_raw_data(info, (u8 *)raw_data->rxshort_data, 2);
	ts_set_touchmode3(TOUCH_POINT_MODE);

	input_info(true, &client->dev, "%s: start\n", __func__);
	for (i = 0; i < y_num; i++) {
		input_info(true, &client->dev, "Rx%d: %d\n", i, raw_data->rxshort_data[i]);

		if ((i == touchkey_node) || (i == (y_num - 1 - touchkey_node))) {
			if (raw_data->rxshort_data[i] > touchkey_max)
				touchkey_max = raw_data->rxshort_data[i];
		} else {
			if (raw_data->rxshort_data[i] > screen_max)
				screen_max = raw_data->rxshort_data[i];
		}
	}

	snprintf(buff, sizeof(buff), "%d,%d", screen_max, touchkey_max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void get_rxshort(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned int val;
	int x_node, y_node;
	int node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
			y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buff, sizeof(buff), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->rxshort_data[node_num];
	snprintf(buff, sizeof(buff), "%u", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true,&client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
		(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_txshort_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int x_num = info->cap_info.x_node_num;
	int i;
	u16 screen_max = 0x0000, touchkey_max = 0x0000;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	sec_cmd_set_default_result(sec);

	ts_set_touchmode3(TOUCH_TXSHORT_MODE);
	get_raw_data(info, (u8 *)raw_data->txshort_data, 2);
	ts_set_touchmode3(TOUCH_POINT_MODE);

	input_info(true, &client->dev, "%s: start\n", __func__);
	for (i = 0; i < x_num - 1; i++) {
		input_info(true,&client->dev, "Tx%d: %d\n", i, raw_data->txshort_data[i]);

		if (raw_data->txshort_data[i] > screen_max)
			screen_max = raw_data->txshort_data[i];
	}
	input_info(true,&client->dev, "Tx%d: %d\n", x_num - 1, raw_data->txshort_data[x_num - 1]);
	touchkey_max = raw_data->txshort_data[x_num - 1];

	snprintf(buff, sizeof(buff), "%d,%d", screen_max, touchkey_max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void get_txshort(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned int val;
	int x_node, y_node;
	int node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
			y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buff, sizeof(buff), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->txshort_data[node_num];
	snprintf(buff, sizeof(buff), "%u", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true,&client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
		(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_selfdnd_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 min, max;
	s32 j;
	int ret;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(TOUCH_SELF_DND_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->selfdnd_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	min = 0xFFFF;
	max = 0x0000;

	input_info(true, &client->dev, "%s: start\n", __func__);
	for (j = 0; j < info->cap_info.y_node_num; j++) {
		input_info(true, &client->dev, "Rx%d: %d\n", j, raw_data->selfdnd_data[j]);

		if (raw_data->selfdnd_data[j] < min && raw_data->selfdnd_data[j] != 0)
			min = raw_data->selfdnd_data[j];

		if (raw_data->selfdnd_data[j] > max)
			max = raw_data->selfdnd_data[j];
	}

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SELF_DND_RX");
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "FAIL");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SELF_DND_RX");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
				(int)strlen(sec->cmd_result));

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void get_selfdnd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned int val;
	int x_node, y_node;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
			y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buff, sizeof(buff), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	val = raw_data->selfdnd_data[y_node];
	snprintf(buff, sizeof(buff), "%u", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_selfdnd_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.y_node_num;
	s32 j;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(info->client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	sec_cmd_set_default_result(sec);

	ts_set_touchmode(TOUCH_SELF_DND_MODE);

	get_raw_data(info, (u8 *)raw_data->selfdnd_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto NG;

	for(j = 0; j < info->cap_info.y_node_num; j++) {
		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->selfdnd_data[j]);
		strncat(buff, temp, CMD_RESULT_WORD_LEN);
		memset(temp, 0x00, SEC_CMD_STR_LEN);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);

NG:
	if(sec->cmd_state != SEC_CMD_STATUS_OK) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void run_ssr_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 min, max;
	s32 j;
	int ret;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(DEF_RAW_SELF_SSR_DATA_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->ssr_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	min = 0xFFFF;
	max = 0x0000;

	input_info(true, &client->dev, "%s: SELF SATURATION RX start\n", __func__);
	for(j = 0; j < info->cap_info.y_node_num; j++) {
		input_info(true, &client->dev, "Rx%d: %d\n", j, raw_data->ssr_data[j]);

		if (raw_data->ssr_data[j] < min && raw_data->ssr_data[j] != 0)
			min = raw_data->ssr_data[j];

		if (raw_data->ssr_data[j] > max)
			max = raw_data->ssr_data[j];
	}

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SELF_SATURATION_RX");
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "FAIL");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SELF_SATURATION_RX");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
				(int)strlen(sec->cmd_result));

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void get_ssr(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 val;
	int x_node, y_node;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
			y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buff, sizeof(buff), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	val = raw_data->ssr_data[y_node];
	snprintf(buff, sizeof(buff), "%u", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_selfdnd_h_gap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char buff_onecmd[SEC_CMD_STR_LEN] = { 0 };
	int y_num = info->cap_info.y_node_num;
	int j, offset, val, cur_val, next_val;
	u16 screen_max = 0x0000;

	sec_cmd_set_default_result(sec);

	memset(raw_data->self_hgap_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &client->dev, "%s: start\n", __func__);
	for (j = 0; j < y_num - 1; j++) {
		offset = j;
		cur_val = raw_data->selfdnd_data[offset];

		if (!cur_val) {
			raw_data->self_hgap_data[offset] = cur_val;
			continue;
		}

		next_val = raw_data->selfdnd_data[offset + 1];

		if (next_val > cur_val)
			val = 100 - ((cur_val * 100) / next_val);
		else
			val = 100 - ((next_val * 100) / cur_val);

		input_info(true, &client->dev, "Rx%d: %d\n", offset, val);
		raw_data->self_hgap_data[offset] = val;

		if (raw_data->self_hgap_data[j] > screen_max)
			screen_max = raw_data->self_hgap_data[j];

	}

	snprintf(buff, sizeof(buff), "%d", screen_max);
	snprintf(buff_onecmd, sizeof(buff_onecmd), "%d,%d", 0, screen_max);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff_onecmd, strnlen(buff_onecmd, sizeof(buff_onecmd)), "SELF_DND_H_GAP");
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_selfdnd_h_gap(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int x_node, y_node;
	int x_num = info->cap_info.x_node_num;
	int y_num = info->cap_info.y_node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= x_num ||
			y_node < 0 || y_node >= y_num - 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	sprintf(buff, "%d", raw_data->self_hgap_data[y_node]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_selfdnd_h_gap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.y_node_num;
	int j, offset, val, cur_val, next_val;

	sec_cmd_set_default_result(sec);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	input_info(true, &info->client->dev, "%s: start\n", __func__);
	for (j = 0; j < info->cap_info.y_node_num - 1; j++) {
		offset = j;
		cur_val = raw_data->selfdnd_data[offset];

		if (!cur_val) {
			raw_data->self_hgap_data[offset] = cur_val;
			continue;
		}

		next_val = raw_data->selfdnd_data[offset + 1];

		if (next_val > cur_val)
			val = 100 - ((cur_val * 100) / next_val);
		else
			val = 100 - ((next_val * 100) / cur_val);

		input_info(true, &info->client->dev, "Rx%d: %d\n", offset, val);
		raw_data->self_hgap_data[offset] = val;

		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->self_hgap_data[offset]);
		strncat(buff, temp, CMD_RESULT_WORD_LEN);
		memset(temp, 0x00, SEC_CMD_STR_LEN);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void run_jitter_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 min, max;
	s32 i, j;
	int ret;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	sec_cmd_set_default_result(sec);

	if (write_reg(info->client, ZT75XX_JITTER_SAMPLING_CNT, 100) != I2C_SUCCESS)
		input_info(true, &client->dev, "%s: Fail to set JITTER_CNT.\n", __func__);

	ret = ts_set_touchmode(TOUCH_JITTER_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->jitter_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	min = 0xFFFF;
	max = 0x0000;

	input_info(true, &client->dev, "%s: start\n", __func__);
	for (i = 0; i < info->cap_info.x_node_num; i++) {
		pr_info("%s ", SECLOG);
		for (j = 0; j < info->cap_info.y_node_num; j++) {
			pr_cont("%4d ", raw_data->jitter_data[i * info->cap_info.y_node_num + j]);

			if (raw_data->jitter_data[i * info->cap_info.y_node_num + j] < min &&
				raw_data->jitter_data[i * info->cap_info.y_node_num + j] != 0)
				min = raw_data->jitter_data[i * info->cap_info.y_node_num + j];

			if (raw_data->jitter_data[i * info->cap_info.y_node_num + j] > max)
				max = raw_data->jitter_data[i * info->cap_info.y_node_num + j];
		}
		pr_cont("\n");
	}
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "FAIL");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
				(int)strlen(sec->cmd_result));

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void get_jitter(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned int val;
	int x_node, y_node;
	int node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
			y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buff, sizeof(buff), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->jitter_data[node_num];
	snprintf(buff, sizeof(buff), "%u", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_jitter_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.x_node_num * info->cap_info.y_node_num;
	s32 i,j;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(info->client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	sec_cmd_set_default_result(sec);

	if (write_reg(info->client, ZT75XX_JITTER_SAMPLING_CNT, 100) != I2C_SUCCESS)
		input_info(true, &info->client->dev, "%s: Fail to set JITTER_CNT.\n", __func__);

	ts_set_touchmode(TOUCH_JITTER_MODE);
	get_raw_data(info, (u8 *)raw_data->jitter_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto NG;

	for (i = 0; i < info->cap_info.x_node_num; i++) {
		pr_info("%s ", SECLOG);
		for (j = 0; j < info->cap_info.y_node_num; j++) {
			pr_cont("%4d ", raw_data->jitter_data[i * info->cap_info.y_node_num + j]);
			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->jitter_data[i * info->cap_info.y_node_num + j]);
			strncat(buff, temp, CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
		pr_cont("\n");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);

NG:
	if (sec->cmd_state != SEC_CMD_STATUS_OK) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

#define I2C_BUFFER_SIZE 64
static bool get_raw_data_size(struct zt75xx_ts_info *info, u8 *buff, int skip_cnt, int sz)
{
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	int i;
	u32 temp_sz;

	disable_irq(info->irq);

	down(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_err(true, &client->dev, "%s: other process occupied. (%d)\n",
			__func__, info->work_state);
		enable_irq(info->irq);
		up(&info->work_lock);
		return false;
	}

	info->work_state = RAW_DATA;

	for (i = 0; i < skip_cnt; i++) {
		while (gpio_get_value(pdata->gpio_int))
			usleep_range(1 * 1000, 1 * 1000);

		write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
		usleep_range(1 * 1000, 1 * 1000);
	}

	while (gpio_get_value(pdata->gpio_int))
		usleep_range(1 * 1000, 1 * 1000);

	for (i = 0; sz > 0; i++) {
		temp_sz = I2C_BUFFER_SIZE;

		if (sz < I2C_BUFFER_SIZE)
			temp_sz = sz;
		if (read_raw_data(client, ZT75XX_RAWDATA_REG + i,
			(char *)(buff + (i * I2C_BUFFER_SIZE)), temp_sz) < 0) {

			input_err(true, &info->client->dev, "%s: error read zinitix tc raw data\n", __func__);
			info->work_state = NOTHING;
			enable_irq(info->irq);
			up(&info->work_lock);
			return false;
		}
		sz -= I2C_BUFFER_SIZE;
	}

	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);

	return true;
}

static void run_reference_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int min = 0xFFFF, max = 0x0000;
	s32 i, j, touchkey_node = 2;
	int buffer_offset;
	int ret;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(TOUCH_REFERENCE_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data_size(info, (u8 *)raw_data->reference_data, 2,
		info->cap_info.total_node_num * 2 + info->cap_info.y_node_num + info->cap_info.x_node_num);
	ts_set_touchmode(TOUCH_POINT_MODE);

	input_info(true,&client->dev, "%s: start\n",__func__);
	for (i = 0; i < info->cap_info.x_node_num; i++) {
		pr_info("%s ", SECLOG);
		for (j = 0; j < info->cap_info.y_node_num; j++) {
			pr_cont("%4d ", raw_data->reference_data[(i * info->cap_info.y_node_num) + j]);

			if (i == (info->cap_info.x_node_num - 1) && info->pdata->support_touchkey) {
				if ((j == touchkey_node) || (j == ((info->cap_info.y_node_num - 1) - touchkey_node))) {
					if (raw_data->reference_data[(i * info->cap_info.y_node_num) + j] < min &&
							raw_data->reference_data[(i * info->cap_info.y_node_num) + j] >= 0)
						min = raw_data->reference_data[(i * info->cap_info.y_node_num) + j];

					if (raw_data->reference_data[(i * info->cap_info.y_node_num) + j] > max)
						max = raw_data->reference_data[(i * info->cap_info.y_node_num) + j];
				}
			} else {
				if (raw_data->reference_data[(i * info->cap_info.y_node_num) + j] < min &&
						raw_data->reference_data[(i * info->cap_info.y_node_num) + j] >= 0)
					min = raw_data->reference_data[(i * info->cap_info.y_node_num) + j];

				if (raw_data->reference_data[(i * info->cap_info.y_node_num) + j] > max)
					max = raw_data->reference_data[(i * info->cap_info.y_node_num) + j];
			}
		}
		pr_cont("\n");
	}

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	buffer_offset = info->cap_info.total_node_num;
	input_info(true, &info->client->dev, "%s: ref_data\n", __func__);
	for (i = 0; i < info->cap_info.x_node_num; i++) {
		pr_info("%s ", SECLOG);
		for (j = 0; j < info->cap_info.y_node_num; j++) {
			pr_cont("%5d ", raw_data->reference_data[buffer_offset + i * info->cap_info.y_node_num + j]);
		}
		pr_cont("\n");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "FAIL");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strlen(sec->cmd_result));
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void get_reference(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned int val;
	int x_node, y_node;
	int node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
			y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buff, sizeof(buff), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->reference_data[node_num];
	snprintf(buff, sizeof(buff), "%u", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_self_sat_dnd_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	s16 min, max;
	s32 j;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	sec_cmd_set_default_result(sec);

	ts_set_self_sat_touchmode(TOUCH_SELF_DND_MODE);
	get_raw_data_size(info, (u8 *)raw_data->self_sat_dnd_data, 1, 32);
	info->touch_mode = TOUCH_POINT_MODE;

	zt75xx_power_control(info, POWER_OFF);
	zt75xx_power_control(info, POWER_ON_SEQUENCE);
	clear_report_data(info);
	mini_init_touch(info);

	min = 0x7FFF;
	max = -0x7FFF;

	input_info(true, &client->dev, "%s: start\n", __func__);
	for(j = 0; j < info->cap_info.y_node_num; j++) {
		input_info(true, &client->dev, "Rx%d: %d\n", j, raw_data->self_sat_dnd_data[j]);

		if (raw_data->self_sat_dnd_data[j] < min && raw_data->self_sat_dnd_data[j] != 0)
			min = raw_data->self_sat_dnd_data[j];

		if (raw_data->self_sat_dnd_data[j] > max)
			max = raw_data->self_sat_dnd_data[j];
	}

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
				(int)strlen(sec->cmd_result));

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void get_self_sat_dnd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned int val;
	int x_node, y_node;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
			y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buff, sizeof(buff), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	val = raw_data->self_sat_dnd_data[y_node];
	snprintf(buff, sizeof(buff), "%u", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

#ifdef TCLM_CONCEPT
static void run_tsp_rawdata_read(void *device_data, u16 rawdata_mode, s16* buff)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, ret;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(info->client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif

	ret = ts_set_touchmode(rawdata_mode);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)buff, 2);
	ts_set_touchmode(TOUCH_POINT_MODE);

	input_info(true, &info->client->dev, "%s: mode %d\n", __func__, rawdata_mode);
	for (i = 0; i < x_num; i++) {
		pr_info("%s [%2d] ", SECLOG, i);
		for (j = 0; j < y_num; j++) {
			pr_cont("%6d ", buff[(i * y_num) + j]);
		}
		pr_cont("\n");
	}
out:
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

/*
## Mis Cal result ##
FD : spec out
F3,F4 : i2c faile
F2 : power off state
F1 : not support mis cal concept
F0 : initial value in function
00 : pass
*/

static void run_mis_cal_read(void * device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char buff_all[SEC_CMD_STR_LEN] = { 0 };

	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset;
	char mis_cal_data = 0xF0;
	int ret = 0;
	s16 raw_data_buff[TSP_CMD_NODE_NUM];
	u16 chip_eeprom_info;
	int min = 0xFFFF, max = -0xFF;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	disable_irq(info->irq);
	sec_cmd_set_default_result(sec);

	if (pdata->mis_cal_check == 0) {
		input_info(true, &info->client->dev, "%s: not support\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	if (info->work_state == SUSPEND) {
		input_info(true, &info->client->dev, "%s: Touch is stopped\n",__func__);
		mis_cal_data = 0xF2;
		goto NG;
	}

	if (read_data(info->client, ZT75XX_EEPROM_INFO, (u8 *)&chip_eeprom_info, 2) < 0) {
		input_info(true, &info->client->dev, "%s: read eeprom_info i2c fail!\n", __func__);
		mis_cal_data = 0xF3;
		goto NG;
	}

	if (zinitix_bit_test(chip_eeprom_info, 0)) {
		input_info(true, &info->client->dev, "%s: eeprom cal 0, skip !\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	ret = ts_set_touchmode(TOUCH_REF_ABNORMAL_TEST_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		mis_cal_data = 0xF4;
		goto NG;
	}
	ret = get_raw_data(info, (u8 *)raw_data->reference_data_abnormal, 2);
	if (!ret) {
		input_info(true, &info->client->dev, "%s: i2c fail!\n", __func__);
		ts_set_touchmode(TOUCH_POINT_MODE);
		mis_cal_data = 0xF4;
		goto NG;
	}
	ts_set_touchmode(TOUCH_POINT_MODE);

	input_info(true, &info->client->dev, "%s: start\n", __func__);
	ret = 1;
	for (i = 0; i < x_num; i++) {
		pr_info("%s ", SECLOG);
		for (j = 0; j < y_num; j++) {
			offset = (i * y_num) + j;
			pr_cont("%5d ", raw_data->reference_data_abnormal[offset]);
			if (pdata->item_version >= 4) {
				if (raw_data->reference_data_abnormal[offset] < min)
					min = raw_data->reference_data_abnormal[offset];
				if (raw_data->reference_data_abnormal[offset] > max)
					max = raw_data->reference_data_abnormal[offset];
			} else {
				if (ret && (raw_data->reference_data_abnormal[offset] > DEF_MIS_CAL_SPEC_MAX
						|| raw_data->reference_data_abnormal[offset] < DEF_MIS_CAL_SPEC_MIN)) {
					mis_cal_data = 0xFD;
					ret = 0;
				}
			}
		}
		pr_cont("\n");
	}
	if(!ret)
		goto NG;

	mis_cal_data = 0x00;
	if (pdata->item_version >= 4) {
		snprintf(buff, sizeof(buff), "%d,%d", min, max);
		snprintf(buff_all, sizeof(buff_all), "%d,%d", min, max);
	} else {
		snprintf(buff, sizeof(buff), "%d", mis_cal_data);
		snprintf(buff_all, sizeof(buff_all), "%d,%d,%d,%d", mis_cal_data, raw_data->reference_data_abnormal[0],
				raw_data->reference_data_abnormal[1], raw_data->reference_data_abnormal[2]);
	}

	sec_cmd_set_cmd_result(sec, buff_all, strnlen(buff_all, sizeof(buff_all)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "MIS_CAL");
	sec->cmd_state = SEC_CMD_STATUS_OK;
 	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
				(int)strlen(sec->cmd_result));
	enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
	return;
NG:
	if (pdata->item_version >= 4) {
		snprintf(buff, sizeof(buff), "%s_%d", "FAIL", mis_cal_data);
		snprintf(buff_all, sizeof(buff_all), "%s_%d", "FAIL", mis_cal_data);
	} else {
		snprintf(buff, sizeof(buff), "%d", mis_cal_data);
		snprintf(buff_all, sizeof(buff_all), "%d,%d,%d,%d", mis_cal_data, 0, 0, 0);
	}

	if (mis_cal_data == 0xFD) {
		run_tsp_rawdata_read(device_data, 7, raw_data_buff);
		run_tsp_rawdata_read(device_data, TOUCH_REFERENCE_MODE, raw_data_buff);
	}
	sec_cmd_set_cmd_result(sec, buff_all, strnlen(buff_all, sizeof(buff_all)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "MIS_CAL");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
 	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
				(int)strlen(sec->cmd_result));
	enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void get_mis_cal(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned int val;
	int x_node, y_node;
	int node_num;

	disable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
			y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buff, sizeof(buff), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
		esd_timer_start(CHECK_ESD_TIMER, info);
		write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
			SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->reference_data_abnormal[node_num];
	snprintf(buff, sizeof(buff), "%u", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

	enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void run_mis_cal_read_all(void * device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.x_node_num * info->cap_info.y_node_num;
	int i, j, offset;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(info->client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	disable_irq(info->irq);

	sec_cmd_set_default_result(sec);

	ts_set_touchmode(TOUCH_POINT_MODE);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto NG;

	for (i = 0; i < info->cap_info.x_node_num; i++) {
		pr_info("%s ", SECLOG);
		for (j = 0; j < info->cap_info.y_node_num; j++) {
			offset = (i * info->cap_info.y_node_num) + j;
			pr_cont("%5d ", raw_data->reference_data_abnormal[offset]);
			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->reference_data_abnormal[offset]);
			strncat(buff, temp, CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
		pr_cont("\n");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);

NG:
	if (sec->cmd_state != SEC_CMD_STATUS_OK) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void get_pat_information(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	char buff[50] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "C%02XT%04X.%4s%s%c%d%c%d%c%d",
		info->tdata->nvdata.cal_count, info->tdata->nvdata.tune_fix_ver,
		info->tdata->tclm_string[info->tdata->nvdata.cal_position].f_name,
		(info->tdata->tclm_level == TCLM_LEVEL_LOCKDOWN) ? ".L " : " ",
		info->tdata->cal_pos_hist_last3[0], info->tdata->cal_pos_hist_last3[1],
		info->tdata->cal_pos_hist_last3[2], info->tdata->cal_pos_hist_last3[3],
		info->tdata->cal_pos_hist_last3[4], info->tdata->cal_pos_hist_last3[5]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

/* FACTORY TEST RESULT SAVING FUNCTION
 * bit 3 ~ 0 : OCTA Assy
 * bit 7 ~ 4 : OCTA module
 * param[0] : OCTA module(1) / OCTA Assy(2)
 * param[1] : TEST NONE(0) / TEST FAIL(1) / TEST PASS(2) : 2 bit
 */
static void get_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	char cbuff[SEC_CMD_STR_LEN] = { 0 };
	u8 buff[2] = {0};

	sec_cmd_set_default_result(sec);

	get_zt_tsp_nvm_data(info, ZT75XX_TS_NVM_OFFSET_FAC_RESULT, (u8 *)buff, 2);
	info->test_result.data[0] = buff[0];

	input_info(true, &info->client->dev, "%s : %X", __func__, info->test_result.data[0]);

	if (info->test_result.data[0] == 0xFF) {
		input_info(true, &info->client->dev, "%s: clear factory_result as zero\n", __func__);
		info->test_result.data[0] = 0;
	}

	snprintf(cbuff, sizeof(cbuff), "M:%s, M:%d, A:%s, A:%d",
			info->test_result.module_result == 0 ? "NONE" :
				info->test_result.module_result == 1 ? "FAIL" : "PASS",
			info->test_result.module_count,
			info->test_result.assy_result == 0 ? "NONE" :
				info->test_result.assy_result == 1 ? "FAIL" : "PASS",
			info->test_result.assy_count);

	sec_cmd_set_cmd_result(sec, cbuff, strnlen(cbuff, sizeof(cbuff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void set_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	char cbuff[SEC_CMD_STR_LEN] = { 0 };
	u8 buff[2] = {0};

	sec_cmd_set_default_result(sec);

	get_zt_tsp_nvm_data(info, ZT75XX_TS_NVM_OFFSET_FAC_RESULT, (u8 *)buff, 2);
	info->test_result.data[0] = buff[0];

	input_info(true, &info->client->dev, "%s: %X", __func__, info->test_result.data[0]);

	if (info->test_result.data[0] == 0xFF) {
		input_info(true, &info->client->dev, "%s: clear factory_result as zero\n", __func__);
		info->test_result.data[0] = 0;
	}

	if (sec->cmd_param[0] == TEST_OCTA_ASSAY) {
		info->test_result.assy_result = sec->cmd_param[1];
		if (info->test_result.assy_count < 3)
			info->test_result.assy_count++;

	} else if (sec->cmd_param[0] == TEST_OCTA_MODULE) {
		info->test_result.module_result = sec->cmd_param[1];
		if (info->test_result.module_count < 3)
			info->test_result.module_count++;
	}

	input_info(true, &info->client->dev, "%s: [0x%X] M:%s, M:%d, A:%s, A:%d\n",
					__func__, info->test_result.data[0],
					info->test_result.module_result == 0 ? "NONE" :
						info->test_result.module_result == 1 ? "FAIL" : "PASS",
					info->test_result.module_count,
					info->test_result.assy_result == 0 ? "NONE" :
						info->test_result.assy_result == 1 ? "FAIL" : "PASS",
					info->test_result.assy_count);

	set_zt_tsp_nvm_data(info, ZT75XX_TS_NVM_OFFSET_FAC_RESULT, &info->test_result.data[0], 1);

	snprintf(cbuff, sizeof(cbuff), "OK");
	sec_cmd_set_cmd_result(sec, cbuff, strnlen(cbuff, sizeof(cbuff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void increase_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 count[2] = { 0 };

	sec_cmd_set_default_result(sec);

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	get_zt_tsp_nvm_data(info, ZT75XX_TS_NVM_OFFSET_DISASSEMBLE_COUNT, count, 2);
	input_info(true, &info->client->dev, "%s: current disassemble count: %d\n", __func__, count[0]);

	if (count[0] == 0xFF)
		count[0] = 0;
	if (count[0] < 0xFE)
		count[0]++;

	set_zt_tsp_nvm_data(info, ZT75XX_TS_NVM_OFFSET_DISASSEMBLE_COUNT, count , 2);

	msleep(5);

	memset(count, 0x00, 2);
	get_zt_tsp_nvm_data(info, ZT75XX_TS_NVM_OFFSET_DISASSEMBLE_COUNT, count, 2);
	input_info(true, &info->client->dev, "%s: check disassemble count: %d\n", __func__, count[0]);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 count[2] = { 0 };

	sec_cmd_set_default_result(sec);

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	get_zt_tsp_nvm_data(info, ZT75XX_TS_NVM_OFFSET_DISASSEMBLE_COUNT, count, 2);
	if (count[0] == 0xFF) {
		count[0] = 0;
		count[1] = 0;
		set_zt_tsp_nvm_data(info, ZT75XX_TS_NVM_OFFSET_DISASSEMBLE_COUNT, count , 2);
	}

	input_info(true, &info->client->dev, "%s: read disassemble count: %d\n", __func__, count[0]);
	snprintf(buff, sizeof(buff), "%d", count[0]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

#define DEF_IUM_ADDR_OFFSET		0xF0A0
#define DEF_IUM_LOCK			0xF0F6
#define DEF_IUM_UNLOCK			0xF0FA

int get_zt_tsp_nvm_data(struct zt75xx_ts_info *info, u8 addr, u8 *values, u16 length)
{
	struct i2c_client *client = info->client;
	u16 buff_start;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	disable_irq(info->irq);

	if (write_cmd(client, DEF_IUM_LOCK) != I2C_SUCCESS) {
		input_err(true, &client->dev, "failed ium lock\n");
		goto fail_ium_random_read;
	}
	msleep(40);

	buff_start = addr;	// custom setting address(0~62, 0,2,4,6)
	//length = 2;		// custom setting(max 64)
	if (length > TC_NVM_SECTOR_SZ)
		length = TC_NVM_SECTOR_SZ;
	if (length < 2) {
		length = 2;	// minimum read 2byte
	}

	if (read_raw_data(client, buff_start + DEF_IUM_ADDR_OFFSET,
			values, length) < 0) {
		input_err(true, &client->dev, "%s: Failed to read raw data %d\n", __func__, length);
		goto fail_ium_random_read;
	}

	if (write_cmd(client, DEF_IUM_UNLOCK) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed ium unlock\n", __func__);
		goto fail_ium_random_read;
	}

	enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
	return 0;

fail_ium_random_read:
	zt75xx_power_control(info, POWER_OFF);
	zt75xx_power_control(info, POWER_ON_SEQUENCE);

	mini_init_touch(info);

	enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
	return -1;
}

int set_zt_tsp_nvm_data(struct zt75xx_ts_info *info, u8 addr, u8 *values, u16 length)
{
	struct i2c_client *client = info->client;
	u8 buff[64];
	u16 buff_start;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	disable_irq(info->irq);

	if (write_cmd(client, DEF_IUM_LOCK) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed ium lock\n", __func__);
		goto fail_ium_random_write;
	}

	buff_start = addr;	//custom setting address(0~62, 0,2,4,6)

	memcpy((u8 *)&buff[buff_start], values, length);

	/* data write start */
	if (length > TC_NVM_SECTOR_SZ)
		length = TC_NVM_SECTOR_SZ;
	if (length < 2) {
		length = 2;	// minimum write 2byte
		buff[buff_start + 1] = 0;
	}

	if (write_data(client, buff_start + DEF_IUM_ADDR_OFFSET,
			(u8 *)&buff[buff_start], length) < 0) {
		input_err(true, &client->dev, "%s: error write zinitix tc firmare\n", __func__);
		goto fail_ium_random_write;
	}
	/* data write end */

	/* for save rom start */
	if (write_reg(client, 0xc104, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to write nvm wp disable\n", __func__);
		goto fail_ium_random_write;
	}
	mdelay(10);

	if (write_cmd(client, 0xF0F8) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed save ium\n", __func__);
		goto fail_ium_random_write;
	}
	mdelay(30);

	if (write_reg(client, 0xc104, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: nvm wp enable\n", __func__);
		goto fail_ium_random_write;
	}
	mdelay(10);
	/* for save rom end */

	if (write_cmd(client, DEF_IUM_UNLOCK) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed ium unlock\n", __func__);
		goto fail_ium_random_write;
	}

	enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
	return 0;

fail_ium_random_write:
	if (write_reg(client, 0xc104, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: nvm wp enable\n", __func__);
	}
	mdelay(10);

	zt75xx_power_control(info, POWER_OFF);
	zt75xx_power_control(info, POWER_ON_SEQUENCE);

	mini_init_touch(info);

	enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
	return -1;
}

int zt75xx_tclm_data_read(struct i2c_client *client, int address)
{
	struct zt75xx_ts_info *info = i2c_get_clientdata(client);
	int i, ret = 0;
	u8 buff[10];
	u8 nbuff[ZT75XX_TS_NVM_OFFSET_LENGTH];

	switch (address) {
	case SEC_TCLM_NVM_OFFSET_IC_FIRMWARE_VER:
		ret = read_data(client, ZT75XX_MINOR_FW_VERSION, buff, 2);
		if (ret < 0) {
			input_err(true, &info->client->dev,"%s: fail fw_minor_version\n", __func__);
			return ret;
		}

		ret = read_data(client, ZT75XX_CHIP_REVISION, (u8*)&buff[2], 8);
		if (ret < 0) {
			input_err(true, &info->client->dev,"%s: fail chip_revision\n", __func__);
			return ret;
		}
		/* ((fw_version & 0xf) << 12) | ((fw_minor_version & 0xf) << 8) | (reg_version & 0xff); */
		ret = ((buff[4] & 0xf) << 12) | ((buff[0] & 0xf) << 8) | (buff[6] & 0xff);
		return ret;

	case SEC_TCLM_NVM_ALL_DATA:
		/* Zinitx driver support index read/write so do not need read FAC_RESULT and DISASSEMBLE_COUNT here
		  * length minus the first 4 bytes
		  */
		ret = get_zt_tsp_nvm_data(info, 4, &nbuff[4], ZT75XX_TS_NVM_OFFSET_LENGTH - 4);
		if (ret < 0)
			return ret;

		info->tdata->nvdata.cal_count = nbuff[ZT75XX_TS_NVM_OFFSET_CAL_COUNT];
		info->tdata->nvdata.tune_fix_ver = (nbuff[ZT75XX_TS_NVM_OFFSET_TUNE_VERSION] << 8) | nbuff[ZT75XX_TS_NVM_OFFSET_TUNE_VERSION + 1];
		info->tdata->nvdata.cal_position = nbuff[ZT75XX_TS_NVM_OFFSET_CAL_POSITION];
		info->tdata->nvdata.cal_pos_hist_cnt = nbuff[ZT75XX_TS_NVM_OFFSET_HISTORY_QUEUE_COUNT];
		info->tdata->nvdata.cal_pos_hist_lastp = nbuff[ZT75XX_TS_NVM_OFFSET_HISTORY_QUEUE_LASTP];
		for (i = ZT75XX_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO; i < ZT75XX_TS_NVM_OFFSET_LENGTH; i++)
			info->tdata->nvdata.cal_pos_hist_queue[i - ZT75XX_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO] = nbuff[i];

		input_err(true, &info->client->dev, "%s:	%d %X %d %d\n", __func__,
			info->tdata->nvdata.cal_count, info->tdata->nvdata.tune_fix_ver,
			info->tdata->nvdata.cal_pos_hist_cnt, info->tdata->nvdata.cal_pos_hist_lastp);

		return ret;
	case SEC_TCLM_NVM_TEST:
		input_info(true, &info->client->dev, "%s: dt: tclm_level [%d] afe_base [%04X]\n",
			__func__, info->tdata->tclm_level, info->tdata->afe_base);

		input_err(true, &info->client->dev, "%s: before %02X, %02X%02X\n", __func__, info->tdata->tclm[0], info->tdata->tclm[1], info->tdata->tclm[2]);
		ret = get_zt_tsp_nvm_data(info, ZT75XX_TS_NVM_OFFSET_LENGTH + SEC_TCLM_NVM_OFFSET,
			info->tdata->tclm, SEC_TCLM_NVM_OFFSET_LENGTH);
		input_err(true, &info->client->dev, "%s: after %02X, %02X%02X\n", __func__, info->tdata->tclm[0], info->tdata->tclm[1], info->tdata->tclm[2]);

		if (info->tdata->tclm[0] != 0xFF) {
			info->tdata->tclm_level = info->tdata->tclm[0];
			info->tdata->afe_base = (info->tdata->tclm[1] << 8) | info->tdata->tclm[2];
			input_info(true, &info->client->dev, "%s: nv: tclm_level [%d] afe_base [%04X]\n",
				__func__, info->tdata->tclm_level, info->tdata->afe_base);
		}
		return ret;
	default:
		return ret;
	}
}

int zt75xx_tclm_data_write(struct i2c_client *client, int address)
{
	struct zt75xx_ts_info *info = i2c_get_clientdata(client);
	int i, ret = 1;
	u8 nbuff[ZT75XX_TS_NVM_OFFSET_LENGTH];

	memset(&nbuff[4], 0x00, ZT75XX_TS_NVM_OFFSET_LENGTH - 4);

	switch (address) {
	case SEC_TCLM_NVM_ALL_DATA:
		nbuff[ZT75XX_TS_NVM_OFFSET_CAL_COUNT] = info->tdata->nvdata.cal_count;
		nbuff[ZT75XX_TS_NVM_OFFSET_TUNE_VERSION] = (u8)(info->tdata->nvdata.tune_fix_ver >> 8);
		nbuff[ZT75XX_TS_NVM_OFFSET_TUNE_VERSION + 1] = (u8)(0xff & info->tdata->nvdata.tune_fix_ver);
		nbuff[ZT75XX_TS_NVM_OFFSET_CAL_POSITION] = info->tdata->nvdata.cal_position;
		nbuff[ZT75XX_TS_NVM_OFFSET_HISTORY_QUEUE_COUNT] = info->tdata->nvdata.cal_pos_hist_cnt;
		nbuff[ZT75XX_TS_NVM_OFFSET_HISTORY_QUEUE_LASTP] = info->tdata->nvdata.cal_pos_hist_lastp;
		for (i = ZT75XX_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO; i < ZT75XX_TS_NVM_OFFSET_LENGTH; i++)
			nbuff[i] = info->tdata->nvdata.cal_pos_hist_queue[i - ZT75XX_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO];

		ret = set_zt_tsp_nvm_data(info, 4, &nbuff[4], ZT75XX_TS_NVM_OFFSET_LENGTH - 4);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: [ERROR] set_tsp_nvm_data ret:%d\n", __func__, ret);
		}
		return ret;
	case SEC_TCLM_NVM_TEST:
		input_err(true, &info->client->dev, "%s: %02X, %02X%02X\n", __func__, info->tdata->tclm[0], info->tdata->tclm[1], info->tdata->tclm[2]);
		ret = set_zt_tsp_nvm_data(info, ZT75XX_TS_NVM_OFFSET_LENGTH + SEC_TCLM_NVM_OFFSET,
			info->tdata->tclm, SEC_TCLM_NVM_OFFSET_LENGTH);
		return ret;
	default:
		return ret;
	}
}

static void tclm_test_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct sec_tclm_data *data = info->tdata;
	int ret = 0;

	sec_cmd_set_default_result(sec);

	if (!data->support_tclm_test)
		goto not_support;

	ret = tclm_test_command(data, sec->cmd_param[0], sec->cmd_param[1], sec->cmd_param[2], buff);
	if (ret < 0)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	else
		sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	return;

not_support:
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void get_calibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (!info->tdata->support_tclm_test)
		goto not_support;

	snprintf(buff, sizeof(buff), "%d", info->is_cal_done);
	info->is_cal_done = false;

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	return;

not_support:
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
static void ium_random_write(struct zt75xx_ts_info *info, u8 data)
{
	struct i2c_client *client = info->client;
	u8 buff[64]; // custom data buffer
	u16 length, buff_start;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	disable_irq(info->irq);

	input_info(true, &client->dev, "%s: %x %d\n", __func__, data, data);

	if (write_cmd(client, DEF_IUM_LOCK) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed ium lock\n", __func__);
		goto fail_ium_random_write;
	}

	//for( i=0 ; i<64 ; i++)
		buff[data] = data;
		buff[data + 1] = data;

	buff_start = data;	// custom setting address(0~62)
	length = 2;		// custom odd number setting(max 64)
	if (length > TC_SECTOR_SZ)
		length = TC_SECTOR_SZ;
	if (write_data(client, buff_start + DEF_IUM_ADDR_OFFSET, (u8 *)&buff[buff_start], length) < 0) {
		input_err(true, &client->dev, "%s: error write zinitix tc firmare\n", __func__);
		goto fail_ium_random_write;
	}

	if (write_reg(client, 0xc104, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to write nvm wp disable\n", __func__);
		goto fail_ium_random_write;
	}
	mdelay(10);

	if (write_cmd(client, 0xF0F8) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed save ium\n", __func__);
		goto fail_ium_random_write;
	}
	mdelay(30);

	if (write_reg(client, 0xc104, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed nvm wp enable\n", __func__);
	}
	mdelay(10);

	if (write_cmd(client, DEF_IUM_UNLOCK) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed ium unlock\n", __func__);
		goto fail_ium_random_write;
	}

	enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
	input_info(true, &client->dev, "%s: %d\n", __func__, __LINE__);
	return;

fail_ium_random_write:
	if (write_reg(client, 0xc104, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed nvm wp enable\n", __func__);
	}
	mdelay(10);

	zt75xx_power_control(info, POWER_OFF);
	zt75xx_power_control(info, POWER_ON_SEQUENCE);

	enable_irq(info->irq);
	return;
}


static void ium_random_read(struct zt75xx_ts_info *info, u8 data)
{
	struct i2c_client *client = info->client;
	u8 buff[64]; // custom data buffer
	u16 length, buff_start;

	disable_irq(info->irq);

	buff_start = 8;		// custom setting address(0~62)
	length = 2;		// custom setting(max 64)
	if (length > TC_SECTOR_SZ)
		length = TC_SECTOR_SZ;

	if (read_raw_data(client, data + DEF_IUM_ADDR_OFFSET, (u8 *)&buff[data], length) < 0) {
		input_err(true, &client->dev, "%s: Failed to read raw data %d\n", __func__, length);
		goto fail_ium_random_read;
	}

	enable_irq(info->irq);

	input_info(true, &info->client->dev, "%s: %x %d", __func__, buff[data], buff[data]);

	return;

fail_ium_random_read:

	zt75xx_power_control(info, POWER_OFF);
	zt75xx_power_control(info, POWER_ON_SEQUENCE);

	enable_irq(info->irq);
	return;
}

static void ium_r_write(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec->cmd_param[1] = sec->cmd_param[0];

	input_info(true, &info->client->dev, "%s: %x %x", __func__, sec->cmd_param[0], sec->cmd_param[1]);

	set_zt_tsp_nvm_data(info, sec->cmd_param[0], (u8 *)&sec->cmd_param[0], 2);

	sec_cmd_set_default_result(sec);

	ium_random_write(info, sec->cmd_param[0]);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void ium_r_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 val = sec->cmd_param[0];

	ium_random_read(info, val);

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void ium_write(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char cbuff[SEC_CMD_STR_LEN] = { 0 };
	int i;
	u8 temp[10];
	u8 buff[64]; // custom data buffer
	u8 val = sec->cmd_param[0];

	for (i = 0; i < 64; i++)
		buff[i] = val;

	zt75xx_power_control(info, POWER_OFF);
	zt75xx_power_control(info, POWER_ON);

	write_reg(client, 0xC000, 0x0001);
	write_reg(client, 0xC004, 0x0001);

	for (i = 0; i < 16; i++) {
		temp[0] = i;
		temp[1] = 0x00;
		temp[2] = buff[i * 4];
		temp[3] = buff[i * 4 + 1];
		temp[4] = buff[i * 4 + 2];
		temp[5] = buff[i * 4 + 3];
		write_data(client, 0xC020, temp, 6);
	}

	write_reg(client, 0xC003, 0x0001);
	temp[0] = 0x00;
	temp[1] = 0x00;
	temp[2] = 0x01;
	temp[3] = 0x00;
	write_data(client, 0xC10D, temp, 4);
	mdelay(5);

	temp[0] = 0x10;
	temp[1] = 0x00;
	temp[2] = 0x00;
	temp[3] = 0x00;
	temp[4] = 0x00;
	temp[5] = 0x20;
	temp[6] = 0xe0;
	temp[7] = 0x00;
	temp[8] = 0x00;
	temp[9] = 0x40;
	write_data(client, 0xC10B, temp, 10);
	msleep(5);

	write_reg(client, 0xC003, 0x0000);
	msleep(5);

	zt75xx_power_control(info, POWER_OFF);
	zt75xx_power_control(info, POWER_ON_SEQUENCE);
	mini_init_touch(info);

	sec_cmd_set_default_result(sec);

	snprintf(cbuff, sizeof(cbuff), "OK");
	sec_cmd_set_cmd_result(sec, cbuff, strnlen(cbuff, sizeof(cbuff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void ium_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char cbuff[SEC_CMD_STR_LEN] = { 0 };
	int i;
	u8 temp[8];
	u8 buff[64]; // custom data buffer

	zt75xx_power_control(info, POWER_OFF);
	zt75xx_power_control(info, POWER_ON);

	write_reg(client, 0xC000, 0x0001);

	temp[0] = 0x0C;
	temp[1] = 0x00;
	temp[2] = 0x02;
	temp[3] = 0x20;
	temp[4] = 0x03;
	temp[5] = 0x00;
	temp[6] = 0x00;
	temp[7] = 0x00;
	write_data(client, 0xCC02, temp, 8);

	for (i = 0; i < 16; i++) {
		temp[0] = i * 4;
		temp[1] = 0x00;
		temp[2] = 0x00;
		temp[3] = 0x20;
		write_data(client, 0xCC01, temp, 4);
		read_data_only(client, buff + i * 4, 4);
	}
	temp[0] = 0x0C;
	temp[1] = 0x00;
	temp[2] = 0x02;
	temp[3] = 0x20;
	temp[4] = 0x02;
	temp[5] = 0x00;
	temp[6] = 0x00;
	temp[7] = 0x00;
	write_data(client, 0xCC02, temp, 8);
	msleep(5);

	zt75xx_power_control(info, POWER_OFF);
	zt75xx_power_control(info, POWER_ON_SEQUENCE);
	mini_init_touch(info);

	for (i = 63; i > 0; i -= 4) {
		input_info(true, &client->dev, "%s: [%2d]:%02X %02X %02X %02X\n",
				__func__, i, buff[i], buff[i - 1], buff[i - 2], buff[i - 3]);
	}

	sec_cmd_set_default_result(sec);

	snprintf(cbuff, sizeof(cbuff), "OK");
	sec_cmd_set_cmd_result(sec, cbuff, strnlen(cbuff, sizeof(cbuff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}
#endif
#endif

static void clear_cover_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int arg = sec->cmd_param[0];

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%u", (unsigned int) arg);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] > 1) {
			info->flip_enable = true;
			info->cover_type = sec->cmd_param[1];
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
			if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
				//msleep(100);
				tui_force_close(1);
				msleep(300);
				if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
					trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);
					trustedui_set_mode(TRUSTEDUI_MODE_OFF);
				}
			}
#endif // CONFIG_TRUSTONIC_TRUSTED_UI
		} else {
			info->flip_enable = false;
		}

		set_cover_type(info, info->flip_enable);

		snprintf(buff, sizeof(buff), "%s", "OK");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	return;
}

static void clear_reference_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif

	write_reg(client, ZT75XX_EEPROM_INFO, 0xffff);

	write_reg(client, 0xc003, 0x0001);
	write_reg(client, 0xc104, 0x0001);
	usleep_range(100, 100);
	if (write_cmd(client, ZT75XX_SAVE_STATUS_CMD) != I2C_SUCCESS)
		return;

	msleep(500);
	write_reg(client, 0xc003, 0x0000);
	write_reg(client, 0xc104, 0x0000);
	usleep_range(100, 100);

#if ESD_TIMER_INTERVAL
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
	esd_timer_start(CHECK_ESD_TIMER, info);
#endif
	input_info(true, &client->dev, "%s: TSP clear calibration bit\n", __func__);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

int zt_tclm_execute_force_calibration(struct i2c_client *client, int cal_mode)
{
	struct zt75xx_ts_info *info = i2c_get_clientdata(client);

	if (ts_hw_calibration(info) == false)
		return -1;

	return 0;
}

static void run_ref_calibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int i;
#ifdef TCLM_CONCEPT
	int ret;
#endif
	sec_cmd_set_default_result(sec);

	if (info->finger_cnt1 != 0) {
		input_info(true, &client->dev, "%s: return (finger cnt %d)\n", __func__, info->finger_cnt1);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	disable_irq(info->irq);

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif

	zt75xx_power_control(info, POWER_OFF);
	zt75xx_power_control(info, POWER_ON_SEQUENCE);

	if (ts_hw_calibration(info) == true) {
#ifdef TCLM_CONCEPT
		/* devide tclm case */
		sec_tclm_case(info->tdata, sec->cmd_param[0]);

		input_info(true, &info->client->dev, "%s: param, %d, %c, %d\n", __func__,
				sec->cmd_param[0], sec->cmd_param[0], info->tdata->root_of_calibration);

		ret = sec_execute_tclm_package(info->tdata, 1);
		if (ret < 0) {
			input_err(true, &info->client->dev,
					"%s: sec_execute_tclm_package\n", __func__);
		}
		sec_tclm_root_of_cal(info->tdata, CALPOSITION_NONE);
#endif
		input_info(true, &client->dev, "%s: TSP calibration Pass\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec_cmd_set_cmd_result(sec, buff, (int)strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		input_info(true, &client->dev, "%s: TSP calibration Fail\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, (int)strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	zt75xx_power_control(info, POWER_OFF);
	zt75xx_power_control(info, POWER_ON_SEQUENCE);
	mini_init_touch(info);

	for (i = 0; i < 5; i++) {
		write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
		usleep_range(10, 10);
	}

#if ESD_TIMER_INTERVAL
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
	esd_timer_start(CHECK_ESD_TIMER, info);
#endif

	enable_irq(info->irq);
	input_info(true, &client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int val = sec->cmd_param[0];

	sec_cmd_set_default_result(sec);

	if(val) //normal
		zinitix_bit_clr(m_optional_mode.select_mode.flag, DEF_OPTIONAL_MODE_EDGE_SELECT);
	else //factory
		zinitix_bit_set(m_optional_mode.select_mode.flag, DEF_OPTIONAL_MODE_EDGE_SELECT);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s\n", __func__, sec->cmd_result);
}

static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int val = sec->cmd_param[0];

	sec_cmd_set_default_result(sec);

	if (val) {
		info->spay_enable = 1;
		zinitix_bit_set(lpm_mode_reg.flag, BIT_EVENT_SPAY);
	} else {
		info->spay_enable = 0;
		zinitix_bit_clr(lpm_mode_reg.flag, BIT_EVENT_SPAY);
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s\n", __func__, sec->cmd_result);
}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int val = sec->cmd_param[0];

	sec_cmd_set_default_result(sec);

	if (val) {
		info->aot_enable = 1;
		zinitix_bit_set(lpm_mode_reg.flag, BIT_EVENT_AOT);
	} else {
		info->aot_enable = 0;
		zinitix_bit_clr(lpm_mode_reg.flag, BIT_EVENT_AOT);
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s\n", __func__, sec->cmd_result);
}

static void aod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int val = sec->cmd_param[0];

	sec_cmd_set_default_result(sec);

	if (val) {
		info->aod_enable = 1;
		zinitix_bit_set(lpm_mode_reg.flag, BIT_EVENT_AOD);
	} else {
		info->aod_enable = 0;
		zinitix_bit_clr(lpm_mode_reg.flag, BIT_EVENT_AOD);
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s\n", __func__, sec->cmd_result);
}

static void set_aod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	input_info(true, &info->client->dev, "%s: w:%d, h:%d, x:%d, y:%d\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3]);

	write_cmd(info->client, 0x0A);
	write_reg(info->client, ZT75XX_SET_AOD_W_REG, (u16)sec->cmd_param[0]);
	write_reg(info->client, ZT75XX_SET_AOD_H_REG, (u16)sec->cmd_param[1]);
	write_reg(info->client, ZT75XX_SET_AOD_X_REG, (u16)sec->cmd_param[2]);
	write_reg(info->client, ZT75XX_SET_AOD_Y_REG, (u16)sec->cmd_param[3]);
	write_cmd(info->client, 0x0B);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s\n", __func__, sec->cmd_result);
}

static void get_wet_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 temp;

	sec_cmd_set_default_result(sec);

	down(&info->work_lock);
	read_data(client, ZT75XX_DEBUG_REG, (u8 *)&temp, 2);
	up(&info->work_lock);

	input_info(true, &client->dev, "%s: %x\n", __func__, temp);

	if (zinitix_bit_test(temp, DEF_DEVICE_STATUS_WATER_MODE))
		temp = true;
	else
		temp = false;

	snprintf(buff, sizeof(buff), "%u", temp);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "WET_MODE");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

#ifdef GLOVE_MODE
static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0])
			zinitix_bit_set(m_optional_mode.select_mode.flag, DEF_OPTIONAL_MODE_SENSITIVE_BIT);
		else
			zinitix_bit_clr(m_optional_mode.select_mode.flag, DEF_OPTIONAL_MODE_SENSITIVE_BIT);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
				(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}
#endif

static void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	snprintf(buff, sizeof(buff), "%d", pdata->item_version);
	sec_cmd_set_cmd_result_all(sec, buff, sizeof(buff), "ITEM_VERSION");

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	run_dnd_read(sec);

	switch (pdata->item_version) {
	case 1:
		run_dnd_v_gap_read(sec);
		run_dnd_h_gap_read(sec);
		run_selfdnd_read(sec);
		run_selfdnd_h_gap_read(sec);
		get_wet_mode(sec);
#ifdef TCLM_CONCEPT
		run_mis_cal_read(sec);
#endif
		break;
	case 2:
		run_dnd_v_gap_read(sec);
		run_dnd_h_gap_read(sec);
		run_selfdnd_read(sec);
		run_selfdnd_h_gap_read(sec);
		run_ssr_read(sec);
		get_wet_mode(sec);
#ifdef TCLM_CONCEPT
		run_mis_cal_read(sec);
#endif
		break;
	case 3:
		run_hfdnd_read(sec);
		run_hfdnd_v_gap_read(sec);
		run_hfdnd_h_gap_read(sec);
		run_selfdnd_read(sec);
		run_selfdnd_h_gap_read(sec);
		run_ssr_read(sec);
		break;
	case 4:
		run_dnd_v_gap_read(sec);
		run_dnd_h_gap_read(sec);
		run_selfdnd_read(sec);
		run_selfdnd_h_gap_read(sec);
		run_ssr_read(sec);
#ifdef TCLM_CONCEPT
		run_mis_cal_read(sec);
#endif
		get_wet_mode(sec);
		break;
	}

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

out:
	input_info(true, &client->dev, "%s: %d%s\n", __func__, sec->item_count,
				sec->cmd_result_all);
}

static void check_connection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[3] = { 0 };
	u8 conn_check_val;
	int ret;

	sec_cmd_set_default_result(sec);

	ret = read_data(client, ZT75XX_CONNECTION_CHECK_REG, (u8 *)&conn_check_val, 1);
	if (ret < 0) {
		input_err(true, &info->client->dev,"%s: fail read TSP connection value\n", __func__);
		goto err_conn_check;
	}

	if (conn_check_val != 1)
		goto err_conn_check;

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;

err_conn_check:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;
}

static ssize_t scrub_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	char buff[256] = { 0 };

	input_info(true, &info->client->dev, "%s: scrub_id: %d, X:%d, Y:%d \n", __func__,
				info->scrub_id, info->scrub_x, info->scrub_y);

	snprintf(buff, sizeof(buff), "%d %d %d", info->scrub_id, info->scrub_x, info->scrub_y);

	info->scrub_id = 0;
	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	s16 value[5];

	get_raw_data_size(info, (u8*) value, 2, 10);

	input_info(true, &info->client->dev, "%s: sensitivity mode,%d,%d,%d,%d,%d\n", __func__,
		value[0], value[1], value[2], value[3], value[4]);

	return snprintf(buf, SEC_CMD_BUF_SIZE,"%d,%d,%d,%d,%d",
			value[0], value[1], value[2], value[3], value[4]);
}

static ssize_t sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	int ret;
	unsigned long value = 0;

	if (count > 2)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret != 0)
		return ret;

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &info->client->dev, "%s: power off in IC\n", __func__);
		return 0;
	}

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif

	input_err(true, &info->client->dev, "%s: enable:%d\n", __func__, value);

	if (value == 1) {
		ts_set_touchmode(TOUCH_SENTIVITY_MEASUREMENT_MODE);
		input_info(true, &info->client->dev, "%s: enable end\n", __func__);
	} else {
		ts_set_touchmode(TOUCH_POINT_MODE);
		input_info(true, &info->client->dev, "%s: disable end\n", __func__);
	}

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
			SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif

	input_info(true, &info->client->dev, "%s: done\n", __func__);
	return count;
}

static ssize_t read_wet_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__, info->wet_count);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", info->wet_count);
}

static ssize_t clear_wet_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);

	info->wet_count = 0;

	input_info(true, &info->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_multi_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__,
			info->multi_count);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", info->multi_count);
}

static ssize_t clear_multi_count_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);

	info->multi_count = 0;

	input_info(true, &info->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_comm_err_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__,
			info->comm_err_count);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", info->comm_err_count);
}

static ssize_t clear_comm_err_count_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);

	info->comm_err_count = 0;

	input_info(true, &info->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_module_id_show(struct device *dev,
					struct device_attribute *devattr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	ssize_t count = snprintf(buf, SEC_CMD_BUF_SIZE, "ZI%02X%02x%c%01X%04X\n",
			info->cap_info.reg_data_version, info->test_result.data[0],
			info->tdata->tclm_string[info->tdata->nvdata.cal_position].s_name,
			info->tdata->nvdata.cal_count & 0xF, info->tdata->nvdata.tune_fix_ver);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buf);

	return count;
}

static ssize_t set_ta_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	int ret;
	unsigned long value = 0;

	if (count > 2)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret != 0)
		return ret;

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &info->client->dev, "%s: power off in IC\n", __func__);
		return 0;
	}

	input_info(true, &info->client->dev, "%s: enable:%d\n", __func__, value);

	if (value == 1) {
		g_ta_connected = true;
	} else {
		g_ta_connected = false;
	}

	zt75xx_set_ta_status(info);
	return count;
}

static ssize_t prox_power_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__,
			info->prox_power_off);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%ld", info->prox_power_off);
}

static ssize_t prox_power_off_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	int ret;
	unsigned long value = 0;

	ret = kstrtoul(buf, 10, &value);
	if (ret != 0)
		return ret;

	input_info(true, &info->client->dev, "%s: enable:%d\n", __func__, value);
	info->prox_power_off = value;

	return count;
}

/*
 * read_support_feature function
 * returns the bit combination of specific feature that is supported.
 */
static ssize_t read_support_feature(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt75xx_ts_info *info = container_of(sec, struct zt75xx_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u32 feature = 0;

	if (info->pdata->support_aot)
		feature |= INPUT_FEATURE_SUPPORT_AOT;

	snprintf(buff, sizeof(buff), "%d", feature);
	input_info(true, &client->dev, "%s: %s\n", __func__, buff);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", buff);
}

static DEVICE_ATTR(scrub_pos, S_IRUGO, scrub_position_show, NULL);
static DEVICE_ATTR(sensitivity_mode, S_IRUGO | S_IWUSR | S_IWGRP, sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(wet_mode, S_IRUGO | S_IWUSR | S_IWGRP, read_wet_mode_show, clear_wet_mode_store);
static DEVICE_ATTR(comm_err_count, S_IRUGO | S_IWUSR | S_IWGRP, read_comm_err_count_show, clear_comm_err_count_store);
static DEVICE_ATTR(multi_count, S_IRUGO | S_IWUSR | S_IWGRP, read_multi_count_show, clear_multi_count_store);
static DEVICE_ATTR(module_id, S_IRUGO, read_module_id_show, NULL);
static DEVICE_ATTR(ta_mode, S_IWUSR | S_IWGRP, NULL, set_ta_mode_store);
static DEVICE_ATTR(prox_power_off, S_IRUGO | S_IWUSR | S_IWGRP, prox_power_off_show, prox_power_off_store);
static DEVICE_ATTR(support_feature, S_IRUGO, read_support_feature, NULL);

static struct attribute *touchscreen_attributes[] = {
	&dev_attr_scrub_pos.attr,
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_wet_mode.attr,
	&dev_attr_multi_count.attr,
	&dev_attr_comm_err_count.attr,
	&dev_attr_module_id.attr,
	&dev_attr_ta_mode.attr,
	&dev_attr_prox_power_off.attr,
	&dev_attr_support_feature.attr,
	NULL,
};

static struct attribute_group touchscreen_attr_group = {
	.attrs = touchscreen_attributes,
};

static ssize_t show_touchkey_threshold(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zt75xx_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	struct capa_info *cap = &(info->cap_info);

	read_data(client, ZT75XX_BUTTON_SENSITIVITY, (u8 *)&cap->key_threshold, 2);

#ifdef NOT_SUPPORTED_TOUCH_DUMMY_KEY
	input_info(true, &client->dev, "%s: key threshold = %d\n", __func__, cap->key_threshold);

	return snprintf(buf, 41, "%d", cap->key_threshold);
#else
	read_data(client, ZT75XX_DUMMY_BUTTON_SENSITIVITY, (u8 *)&cap->dummy_threshold, 2);
	input_info(true, &client->dev, "%s: key threshold = %d %d %d %d\n", __func__,
			cap->dummy_threshold, cap->key_threshold, cap->key_threshold, cap->dummy_threshold);

	return snprintf(buf, 41, "%d %d %d %d", cap->dummy_threshold,
					cap->key_threshold,  cap->key_threshold,
					cap->dummy_threshold);
#endif
}

static ssize_t show_touchkey_sensitivity(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct zt75xx_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u16 val = 0;
	int ret;
	int i;

#ifdef NOT_SUPPORTED_TOUCH_DUMMY_KEY
	if (!strcmp(attr->attr.name, "touchkey_recent")) {
		i = 0;
	} else if (!strcmp(attr->attr.name, "touchkey_back")) {
		i = 1;
	} else {
		input_err(true, &client->dev, "%s: Invalid attribute\n",__func__);

		goto err_out;
	}

#else
	if (!strcmp(attr->attr.name, "touchkey_dummy_btn1")) {
		i = 0;
	} else if (!strcmp(attr->attr.name, "touchkey_recent")) {
		i = 1;
	} else if (!strcmp(attr->attr.name, "touchkey_back")) {
		i = 2;
	} else if (!strcmp(attr->attr.name, "touchkey_dummy_btn4")) {
		i = 3;
	} else if (!strcmp(attr->attr.name, "touchkey_dummy_btn5")) {
		i = 4;
	} else if (!strcmp(attr->attr.name, "touchkey_dummy_btn6")) {
		i = 5;
	} else {
		input_err(true, &client->dev, "%s: Invalid attribute\n",__func__);

		goto err_out;
	}
#endif
	down(&info->work_lock);
	ret = read_data(client, ZT75XX_BTN_WIDTH + i, (u8*)&val, 2);
	up(&info->work_lock);
	if (ret < 0) {
		input_err(true, &client->dev, "%s: Failed to read %d's key sensitivity\n",
					 __func__, i);

		goto err_out;
	}

	input_info(true, &client->dev, "%s: %d's key sensitivity = %d\n",
				__func__, i, val);

	return snprintf(buf, 6, "%d", val);

err_out:
	return sprintf(buf, "NG");
}

static ssize_t show_back_key_raw_data(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t show_menu_key_raw_data(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t touch_led_control(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct zt75xx_ts_info *info = dev_get_drvdata(dev);
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct regulator *regulator_led = NULL;
	int retval = 0;
	u8 data;

	sscanf(buf, "%hhu", &data);

	if (pdata->regulator_tkled) {
		regulator_led = regulator_get(NULL, pdata->regulator_tkled);
		if (IS_ERR(regulator_led)) {
			input_err(true, dev, "%s: Failed to get regulator_led.\n", __func__);
			goto out_led_control;
		}

		input_info(true, &info->client->dev, "%s : %d _ %d\n", __func__, data, __LINE__);

		if (data) {
			retval = regulator_enable(regulator_led);
			if (retval)
				input_err(true, dev, "%s: Failed to enable regulator_led: %d\n", __func__, retval);
		} else {
			if (regulator_is_enabled(regulator_led)) {
				retval = regulator_disable(regulator_led);
				if (retval)
					input_err(true, dev, "%s: Failed to disable regulator_led: %d\n", __func__, retval);
			}
		}

out_led_control:
		regulator_put(regulator_led);
	}

	return size;
}

static DEVICE_ATTR(touchkey_threshold, S_IRUGO, show_touchkey_threshold, NULL);
static DEVICE_ATTR(touchkey_recent, S_IRUGO, show_touchkey_sensitivity, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, show_touchkey_sensitivity, NULL);
#ifndef NOT_SUPPORTED_TOUCH_DUMMY_KEY
static DEVICE_ATTR(touchkey_dummy_btn1, S_IRUGO,
					show_touchkey_sensitivity, NULL);
static DEVICE_ATTR(touchkey_dummy_btn3, S_IRUGO,
					show_touchkey_sensitivity, NULL);
static DEVICE_ATTR(touchkey_dummy_btn4, S_IRUGO,
					show_touchkey_sensitivity, NULL);
static DEVICE_ATTR(touchkey_dummy_btn6, S_IRUGO,
					show_touchkey_sensitivity, NULL);
#endif
static DEVICE_ATTR(touchkey_raw_back, S_IRUGO, show_back_key_raw_data, NULL);
static DEVICE_ATTR(touchkey_raw_menu, S_IRUGO, show_menu_key_raw_data, NULL);
static DEVICE_ATTR(brightness, 0664, NULL, touch_led_control);

static struct attribute *touchkey_attributes[] = {
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_recent.attr,
	&dev_attr_touchkey_raw_menu.attr,
	&dev_attr_touchkey_raw_back.attr,
#ifndef NOT_SUPPORTED_TOUCH_DUMMY_KEY
	&dev_attr_touchkey_dummy_btn1.attr,
	&dev_attr_touchkey_dummy_btn3.attr,
	&dev_attr_touchkey_dummy_btn4.attr,
	&dev_attr_touchkey_dummy_btn6.attr,
#endif
	&dev_attr_brightness.attr,
	NULL,
};
static struct attribute_group touchkey_attr_group = {
	.attrs = touchkey_attributes,
};

static struct sec_cmd sec_cmds[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_checksum_data", get_checksum_data),},
	{SEC_CMD("get_threshold", get_threshold),},
	{SEC_CMD("module_off_master", module_off_master),},
	{SEC_CMD("module_on_master", module_on_master),},
	{SEC_CMD("module_off_slave", module_off_slave),},
	{SEC_CMD("module_on_slave", module_on_slave),},
	{SEC_CMD("get_module_vendor", get_module_vendor),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},

	/* vendor dependant command */
	{SEC_CMD("run_delta_read", run_delta_read),},
	{SEC_CMD("get_delta_all_data", get_delta),},
	{SEC_CMD("run_dnd_read", run_dnd_read),},
	{SEC_CMD("get_dnd", get_dnd),},
	{SEC_CMD("run_dnd_read_all", run_dnd_read_all),},
	{SEC_CMD("run_dnd_v_gap_read", run_dnd_v_gap_read),},
	{SEC_CMD("get_dnd_v_gap", get_dnd_v_gap),},
	{SEC_CMD("run_dnd_v_gap_read_all", run_dnd_v_gap_read_all),},
	{SEC_CMD("run_dnd_h_gap_read", run_dnd_h_gap_read),},
	{SEC_CMD("get_dnd_h_gap", get_dnd_h_gap),},
	{SEC_CMD("run_dnd_h_gap_read_all", run_dnd_h_gap_read_all),},
	{SEC_CMD("run_hfdnd_read", run_hfdnd_read),},
	{SEC_CMD("get_hfdnd", get_hfdnd),},
	{SEC_CMD("run_hfdnd_v_gap_read", run_hfdnd_v_gap_read),},
	{SEC_CMD("get_hfdnd_v_gap", get_hfdnd_v_gap),},
	{SEC_CMD("run_hfdnd_h_gap_read", run_hfdnd_h_gap_read),},
	{SEC_CMD("get_hfdnd_h_gap", get_hfdnd_h_gap),},
	{SEC_CMD("run_rxshort_read", run_rxshort_read),},
	{SEC_CMD("get_rxshort", get_rxshort),},
	{SEC_CMD("run_txshort_read", run_txshort_read),},
	{SEC_CMD("get_txshort", get_txshort),},
	{SEC_CMD("run_selfdnd_read", run_selfdnd_read),},
	{SEC_CMD("get_selfdnd", get_selfdnd),},
	{SEC_CMD("run_selfdnd_read_all", run_selfdnd_read_all),},
	{SEC_CMD("run_ssr_read", run_ssr_read),},
	{SEC_CMD("get_ssr", get_ssr),},
	{SEC_CMD("run_selfdnd_h_gap_read", run_selfdnd_h_gap_read),},
	{SEC_CMD("get_selfdnd_h_gap", get_selfdnd_h_gap),},
	{SEC_CMD("run_selfdnd_h_gap_read_all", run_selfdnd_h_gap_read_all),},
	{SEC_CMD("run_self_sat_dnd_read", run_self_sat_dnd_read),},
	{SEC_CMD("get_self_sat_dnd", get_self_sat_dnd),},
	{SEC_CMD("run_jitter_read", run_jitter_read),},
	{SEC_CMD("get_jitter", get_jitter),},
	{SEC_CMD("run_jitter_read_all", run_jitter_read_all),},
	{SEC_CMD("run_reference_read", run_reference_read),},
	{SEC_CMD("get_reference", get_reference),},
#ifdef TCLM_CONCEPT
	{SEC_CMD("run_mis_cal_read", run_mis_cal_read),},
	{SEC_CMD("get_mis_cal", get_mis_cal),},
	{SEC_CMD("run_mis_cal_read_all", run_mis_cal_read_all),},
	{SEC_CMD("get_pat_information", get_pat_information),},
	{SEC_CMD("get_tsp_test_result", get_tsp_test_result),},
	{SEC_CMD("set_tsp_test_result", set_tsp_test_result),},
	{SEC_CMD("increase_disassemble_count", increase_disassemble_count),},
	{SEC_CMD("get_disassemble_count", get_disassemble_count),},
	{SEC_CMD("tclm_test_cmd", tclm_test_cmd),},
	{SEC_CMD("get_calibration", get_calibration),},
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	{SEC_CMD("ium_write", ium_write),},
	{SEC_CMD("ium_read", ium_read),},
	{SEC_CMD("ium_r_write", ium_r_write),},
	{SEC_CMD("ium_r_read", ium_r_read),},
#endif
#endif
	{SEC_CMD("run_force_calibration", run_ref_calibration),},
	{SEC_CMD("clear_reference_data", clear_reference_data),},
	{SEC_CMD("run_ref_calibration", run_ref_calibration),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD_H("clear_cover_mode", clear_cover_mode),},
	{SEC_CMD_H("spay_enable", spay_enable),},
	{SEC_CMD_H("aot_enable", aot_enable),},
	{SEC_CMD_H("aod_enable", aod_enable),},
	{SEC_CMD("set_aod_rect", set_aod_rect),},
	{SEC_CMD("get_wet_mode", get_wet_mode),},
#ifdef GLOVE_MODE
	{SEC_CMD_H("glove_mode", glove_mode),},
#endif
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("check_connection", check_connection),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

static int init_sec_factory(struct zt75xx_ts_info *info)
{
	struct device *factory_tk_dev = NULL;
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct tsp_raw_data *raw_data;
	int ret;

	raw_data = kzalloc(sizeof(struct tsp_raw_data), GFP_KERNEL);
	if (unlikely(!raw_data)) {
		input_err(true, &info->client->dev, "%s: Failed to allocate memory\n",
				__func__);
		ret = -ENOMEM;

		goto err_alloc;
	}

	if (pdata->support_touchkey) {
#if defined(CONFIG_DRV_SAMSUNG)
		factory_tk_dev = sec_device_create( SEC_CLASS_DEVT_TKEY,
					NULL, SEC_CLASS_DEV_NAME_TKEY);
#else
		factory_tk_dev = device_create(sec_class, NULL,
					(dev_t) (size_t) (&info->client->dev),
					NULL, SEC_CLASS_DEV_NAME_TKEY);
#endif
		if (IS_ERR(factory_tk_dev)) {
			input_err(true, &info->client->dev, "Failed to create factory dev\n");
			ret = -ENODEV;
			goto err_create_device;
		}
	}

#ifdef CONFIG_TOUCHSCREEN_DUAL_FOLDABLE
	ret = sec_cmd_init(&info->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP2);
#else
	ret = sec_cmd_init(&info->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
#endif
	if (ret < 0) {
		input_err(true, &info->client->dev,
				"%s: Failed to sec_cmd_init\n", __func__);
		goto err_init_cmd;
	}

	ret = sysfs_create_group(&info->sec.fac_dev->kobj,
			&touchscreen_attr_group);
	if (ret < 0) {
		input_err(true, &info->client->dev,
				"%s: Failed to create sysfs attributes\n", __func__);
		goto err_create_sysfs;
	}

	ret = sysfs_create_link(&info->sec.fac_dev->kobj,
			&info->input_dev->dev.kobj, "input");
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: Failed to create link\n", __func__);
		goto err_create_sysfs;
	}

	if (pdata->support_touchkey) {
		ret = sysfs_create_group(&factory_tk_dev->kobj, &touchkey_attr_group);
		if (unlikely(ret)) {
			input_err(true, &info->client->dev, "%s: Failed to create touchkey sysfs group\n", __func__);
			goto err_create_sysfs;
		}
	}

	info->raw_data = raw_data;

	return ret;

err_create_sysfs:
err_init_cmd:
err_create_device:
	kfree(raw_data);
err_alloc:

	return ret;
}
#endif

#ifdef USE_MISC_DEVICE
static int ts_misc_fops_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int ts_misc_fops_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static long ts_misc_fops_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	struct zt75xx_ts_info *info = misc_info;
	struct raw_ioctl raw_ioctl;
	u8 *u8Data;
	int ret = 0;
	size_t sz = 0;
	//u16 version;
	u16 mode;
	struct reg_ioctl reg_ioctl;
	u16 val;
	int nval = 0;
#ifdef CONFIG_COMPAT
	void __user *argp = compat_ptr(arg);
#else
	void __user *argp = (void __user *)arg;
#endif

	if (info == NULL) {
		input_err(true, NULL, "[zinitix_touch] misc device NULL\n");
		return -1;
	}

	switch (cmd) {
	case TOUCH_IOCTL_GET_DEBUGMSG_STATE:
		ret = m_ts_debug_mode;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_SET_DEBUGMSG_STATE:
		if (copy_from_user(&nval, argp, 4)) {
			input_err(true, &info->client->dev, "[zinitix_touch] error : copy_from_user\n");
			return -1;
		}
		if (nval)
			input_info(true, &info->client->dev, "[zinitix_touch] on debug mode (%d)\n", nval);
		else
			input_info(true, &info->client->dev, "[zinitix_touch] off debug mode (%d)\n", nval);
		m_ts_debug_mode = nval;
		break;

	case TOUCH_IOCTL_GET_CHIP_REVISION:
		ret = info->cap_info.ic_revision;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_FW_VERSION:
		ret = info->cap_info.fw_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_REG_DATA_VERSION:
		ret = info->cap_info.reg_data_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_VARIFY_UPGRADE_SIZE:
		if (copy_from_user(&sz, argp, sizeof(size_t)))
			return -1;

		//input_info(true, &info->client->dev, "[zinitix_touch]: firmware size = %d\n", sz);
		if (info->cap_info.ic_fw_size != sz) {
			input_err(true, &info->client->dev, "[zinitix_touch]: firmware size error\n");
			return -1;
		}
		break;
/*
	case TOUCH_IOCTL_VARIFY_UPGRADE_DATA:
		if (copy_from_user(m_firmware_data,
			argp, info->cap_info.ic_fw_size))
			return -1;

		version = (u16) (m_firmware_data[52] | (m_firmware_data[53]<<8));

		input_info(true, &info->client->dev, "[zinitix_touch]: firmware version = %x\n", version);

		if (copy_to_user(argp, &version, sizeof(version)))
			return -1;
		break;

	case TOUCH_IOCTL_START_UPGRADE:
		return ts_upgrade_sequence((u8*)m_firmware_data);
*/
	case TOUCH_IOCTL_GET_X_RESOLUTION:
		ret = info->pdata->x_resolution;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_RESOLUTION:
		ret = info->pdata->y_resolution;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_X_NODE_NUM:
		ret = info->cap_info.x_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_NODE_NUM:
		ret = info->cap_info.y_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_TOTAL_NODE_NUM:
		ret = info->cap_info.total_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_HW_CALIBRAION:
		ret = -1;
		disable_irq(info->irq);
		down(&info->work_lock);
		if (info->work_state != NOTHING) {
			input_info(true, &info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\n",
				info->work_state);
			up(&info->work_lock);
			return -1;
		}
		info->work_state = HW_CALIBRAION;
		msleep(100);

		/* h/w calibration */
		if (ts_hw_calibration(info) == true) {
			ret = 0;
#ifdef TCLM_CONCEPT
			sec_tclm_root_of_cal(info->tdata, CALPOSITION_TESTMODE);
			sec_execute_tclm_package(info->tdata, 1);
			sec_tclm_root_of_cal(info->tdata, CALPOSITION_NONE);
#endif
		}

		mode = info->touch_mode;
		if (write_reg(info->client, ZT75XX_TOUCH_MODE, mode) != I2C_SUCCESS) {
			input_err(true, &info->client->dev, "[zinitix_touch]: failed to set touch mode %d\n",
				mode);
			goto fail_hw_cal;
		}

		if (write_cmd(info->client, ZT75XX_SWRESET_CMD) != I2C_SUCCESS)
			goto fail_hw_cal;

		enable_irq(info->irq);
		info->work_state = NOTHING;
		up(&info->work_lock);
		return ret;

fail_hw_cal:
		enable_irq(info->irq);
		info->work_state = NOTHING;
		up(&info->work_lock);
		return -1;

	case TOUCH_IOCTL_SET_RAW_DATA_MODE:
		if (copy_from_user(&nval, argp, 4)) {
			input_info(true, &info->client->dev, "[zinitix_touch] error : copy_from_user\n");
			info->work_state = NOTHING;
			return -1;
		}
		ts_set_touchmode((u16)nval);

		return 0;

	case TOUCH_IOCTL_GET_REG:
		down(&info->work_lock);
		if (info->work_state != NOTHING) {
			input_info(true, &info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\n",
				info->work_state);
			up(&info->work_lock);
			return -1;
		}

		info->work_state = SET_MODE;

		if (copy_from_user(&reg_ioctl, argp, sizeof(struct reg_ioctl))) {
			info->work_state = NOTHING;
			up(&info->work_lock);
			input_info(true, &info->client->dev, "[zinitix_touch] error : copy_from_user\n");
			return -1;
		}

		if (read_data(info->client, (u16)reg_ioctl.addr, (u8 *)&val, 2) < 0)
			ret = -1;

		nval = (int)val;

#ifdef CONFIG_COMPAT
		if (copy_to_user(compat_ptr(reg_ioctl.val), (u8 *)&nval, 4)) {
#else
		if (copy_to_user((void __user *)(reg_ioctl.val), (u8 *)&nval, 4)) {
#endif
			info->work_state = NOTHING;
			up(&info->work_lock);
			input_info(true, &info->client->dev, "[zinitix_touch] error : copy_to_user\n");
			return -1;
		}

		input_info(true, &info->client->dev, "[zinitix_touch] read : reg addr = 0x%x, val = 0x%x\n",
			reg_ioctl.addr, nval);

		info->work_state = NOTHING;
		up(&info->work_lock);
		return ret;

	case TOUCH_IOCTL_SET_REG:
		down(&info->work_lock);
		if (info->work_state != NOTHING) {
			input_info(true, &info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\n",
				info->work_state);
			up(&info->work_lock);
			return -1;
		}

		info->work_state = SET_MODE;
		if (copy_from_user(&reg_ioctl, argp, sizeof(struct reg_ioctl))) {
			info->work_state = NOTHING;
			up(&info->work_lock);
			input_info(true, &info->client->dev, "[zinitix_touch] error : copy_from_user(1)\n");
			return -1;
		}

#ifdef CONFIG_COMPAT
		if (copy_from_user(&val, compat_ptr(reg_ioctl.val), 4)) {
#else
		if (copy_from_user(&val,(void __user *)(reg_ioctl.val), 4)) {
#endif
			info->work_state = NOTHING;
			up(&info->work_lock);
			input_info(true, &info->client->dev, "[zinitix_touch] error : copy_from_user(2)\n");
			return -1;
		}

		if (write_reg(info->client, (u16)reg_ioctl.addr, val) != I2C_SUCCESS)
			ret = -1;

		input_info(true, &info->client->dev, "[zinitix_touch] write : reg addr = 0x%x, val = 0x%x\r\n",
			reg_ioctl.addr, val);
		info->work_state = NOTHING;
		up(&info->work_lock);
		return ret;

	case TOUCH_IOCTL_DONOT_TOUCH_EVENT:
		down(&info->work_lock);
		if (info->work_state != NOTHING) {
			input_info(true, &info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\r\n",
				info->work_state);
			up(&info->work_lock);
			return -1;
		}

		info->work_state = SET_MODE;
		if (write_reg(info->client, ZT75XX_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
			ret = -1;
		input_info(true, &info->client->dev, "[zinitix_touch] write : reg addr = 0x%x, val = 0x0\r\n",
			ZT75XX_INT_ENABLE_FLAG);

		info->work_state = NOTHING;
		up(&info->work_lock);
		return ret;

	case TOUCH_IOCTL_SEND_SAVE_STATUS:
		down(&info->work_lock);
		if (info->work_state != NOTHING) {
			input_info(true, &info->client->dev, "[zinitix_touch]: other process occupied.." \
				"(%d)\r\n", info->work_state);
			up(&info->work_lock);
			return -1;
		}
		info->work_state = SET_MODE;
		ret = 0;
		write_reg(info->client, 0xc003, 0x0001);
		write_reg(info->client, 0xc104, 0x0001);
		if (write_cmd(info->client, ZT75XX_SAVE_STATUS_CMD) != I2C_SUCCESS)
			ret =  -1;

		msleep(1000);	/* for fusing eeprom */
		write_reg(info->client, 0xc003, 0x0000);
		write_reg(info->client, 0xc104, 0x0000);

		info->work_state = NOTHING;
		up(&info->work_lock);
		return ret;

	case TOUCH_IOCTL_GET_RAW_DATA:
		if (info->touch_mode == TOUCH_POINT_MODE)
			return -1;

		down(&info->raw_data_lock);
		if (info->update == 0) {
			up(&info->raw_data_lock);
			return -2;
		}

		if (copy_from_user(&raw_ioctl,
			argp, sizeof(struct raw_ioctl))) {
			up(&info->raw_data_lock);
			input_info(true, &info->client->dev, "[zinitix_touch] error : copy_from_user\r\n");
			return -1;
		}

		info->update = 0;

		u8Data = (u8 *)&info->cur_data[0];
		if(raw_ioctl.sz > MAX_TRAW_DATA_SZ*2)
			raw_ioctl.sz = MAX_TRAW_DATA_SZ*2;
#ifdef CONFIG_COMPAT
		if (copy_to_user(compat_ptr(raw_ioctl.buf), (u8 *)u8Data,
			raw_ioctl.sz)) {
#else
		if (copy_to_user((void __user *)(raw_ioctl.buf), (u8 *)u8Data,
			raw_ioctl.sz)) {
#endif
			up(&info->raw_data_lock);
			return -1;
		}

		up(&info->raw_data_lock);
		return 0;

	default:
		break;
	}
	return 0;
}
#endif

#ifdef CONFIG_OF
static int zt75xx_pinctrl_configure(struct zt75xx_ts_info *info, bool active)
{
	struct device *dev = &info->client->dev;
	struct pinctrl_state *pinctrl_state;
	int retval = 0;

	input_dbg(true, dev, "%s: pinctrl %d\n", __func__, active);

	if (active)
		pinctrl_state = pinctrl_lookup_state(info->pinctrl, "on_state");
	else
		pinctrl_state = pinctrl_lookup_state(info->pinctrl, "off_state");

	if (IS_ERR(pinctrl_state)) {
		input_err(true, dev, "%s: Failed to lookup pinctrl.\n", __func__);
	} else {
		retval = pinctrl_select_state(info->pinctrl, pinctrl_state);
		if (retval)
			input_err(true, dev, "%s: Failed to configure pinctrl.\n", __func__);
	}
	return 0;
}

static int zt75xx_power_ctrl(void *data, bool on)
{
	struct zt75xx_ts_info* info = (struct zt75xx_ts_info*)data;
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct device *dev = &info->client->dev;
	struct regulator *regulator_dvdd = NULL;
	struct regulator *regulator_avdd;
	int retval = 0;

	if (info->tsp_pwr_enabled == on)
		return retval;

	if (!pdata->gpio_ldo_en) {
		regulator_dvdd = regulator_get(NULL, pdata->regulator_dvdd);
		if (IS_ERR(regulator_dvdd)) {
			input_err(true, dev, "%s: Failed to get %s regulator.\n",
				 __func__, pdata->regulator_dvdd);
			return PTR_ERR(regulator_dvdd);
		}
	}
	regulator_avdd = regulator_get(NULL, pdata->regulator_avdd);
	if (IS_ERR(regulator_avdd)) {
		input_err(true, dev, "%s: Failed to get %s regulator.\n",
			 __func__, pdata->regulator_avdd);
		return PTR_ERR(regulator_avdd);
	}

	input_info(true, dev, "%s: %s\n", __func__, on ? "on" : "off");

	if (on) {
		retval = regulator_enable(regulator_avdd);
		if (retval) {
			input_err(true, dev, "%s: Failed to enable avdd: %d\n", __func__, retval);
			return retval;
		}
		if (!pdata->gpio_ldo_en) {
			retval = regulator_enable(regulator_dvdd);
			if (retval) {
				input_err(true, dev, "%s: Failed to enable vdd: %d\n", __func__, retval);
				return retval;
			}
		}
	} else {
		if (!pdata->gpio_ldo_en) {
			if (regulator_is_enabled(regulator_dvdd))
				regulator_disable(regulator_dvdd);
		}
		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);
	}

	info->tsp_pwr_enabled = on;
	if (!pdata->gpio_ldo_en)
		regulator_put(regulator_dvdd);
	regulator_put(regulator_avdd);

	return retval;
}


static int zinitix_init_gpio(struct zt75xx_ts_platform_data *pdata)
{
	int ret = 0;

	ret = gpio_request(pdata->gpio_int, "zinitix_tsp_irq");
	if (ret) {
		pr_err("%s %s: unable to request zinitix_tsp_irq [%d]\n",
			SECLOG, __func__, pdata->gpio_int);
		return ret;
	}

	return ret;
}

static int zt75xx_ts_parse_dt(struct device_node *np,
			 struct device *dev,
			 struct zt75xx_ts_platform_data *pdata)
{
	int ret = 0;
	u32 temp;
	u32 px_zone[3] = { 0 };

	ret = of_property_read_u32(np, "zinitix,x_resolution", &temp);
	if (ret) {
		input_info(true, dev, "Unable to read controller version\n");
		return ret;
	}
	pdata->x_resolution = (u16) temp;

	ret = of_property_read_u32(np, "zinitix,y_resolution", &temp);
	if (ret) {
		input_info(true, dev, "Unable to read controller version\n");
		return ret;
	}
	pdata->y_resolution = (u16) temp;

	if (of_property_read_u32_array(np, "zinitix,area-size", px_zone, 3)) {
		dev_info(dev, "Failed to get zone's size\n");
		pdata->area_indicator = 48;
		pdata->area_navigation = 96;
		pdata->area_edge = 60;
	} else {
		pdata->area_indicator = (u8) px_zone[0];
		pdata->area_navigation = (u8) px_zone[1];
		pdata->area_edge = (u8) px_zone[2];
	}

	ret = of_property_read_u32(np, "zinitix,page_size", &temp);
	if (ret) {
		input_info(true, dev, "Unable to read controller version\n");
		return ret;
	}
	pdata->page_size = (u16) temp;

	pdata->gpio_int = of_get_named_gpio(np, "zinitix,irq_gpio", 0);
	if (pdata->gpio_int < 0) {
		pr_err("%s: of_get_named_gpio failed: tsp_gpio %d\n", __func__,
			pdata->gpio_int);
		return -EINVAL;
	}

	if (of_get_property(np, "zinitix,gpio_ldo_en", NULL)) {
		pdata->gpio_ldo_en = true;
	} else {
		if (of_property_read_string(np, "zinitix,regulator_dvdd", &pdata->regulator_dvdd)) {
			input_err(true, dev, "Failed to get regulator_dvdd name property\n");
			return -EINVAL;
		}
	}
	if (of_property_read_string(np, "zinitix,regulator_avdd", &pdata->regulator_avdd)) {
		input_err(true, dev, "Failed to get regulator_avdd name property\n");
		return -EINVAL;
	}

	pdata->tsp_power = zt75xx_power_ctrl;

	/* Optional parmeters(those values are not mandatory)
	 * do not return error value even if fail to get the value
	 */
	of_property_read_string(np, "zinitix,firmware_name", &pdata->firmware_name);
	of_property_read_string(np, "zinitix,chip_name", &pdata->chip_name);
	of_property_read_string(np, "zinitix,project_name", &pdata->project_name);
	of_property_read_string(np, "zinitix,regulator_tkled", &pdata->regulator_tkled);

	pdata->support_touchkey = of_property_read_bool(np, "zinitix,touchkey");
	pdata->support_spay = of_property_read_bool(np, "zinitix,spay");
	pdata->support_aod = of_property_read_bool(np, "zinitix,aod");
	pdata->support_aot = of_property_read_bool(np, "zinitix,aot");
	pdata->support_lpm_mode = (pdata->support_spay | pdata->support_aod | pdata->support_aot);
	pdata->bringup = of_property_read_bool(np, "zinitix,bringup");
	pdata->mis_cal_check = of_property_read_bool(np, "zinitix,mis_cal_check");

	if (of_property_read_u32(np, "zinitix,factory_item_version", &pdata->item_version) < 0)
		pdata->item_version = 0;
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	of_property_read_u32(np, "zinitix,ss_touch_num", &pdata->ss_touch_num);
	input_info(true, dev, "%s: ss_touch_num:%d\n", __func__, pdata->ss_touch_num);
#endif
	return 0;
}

static void sec_tclm_parse_dt(struct i2c_client *client, struct sec_tclm_data *tdata)
{
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;

	if (of_property_read_u32(np, "zinitix,tclm_level", &tdata->tclm_level) < 0) {
		tdata->tclm_level = 0;
		input_err(true, dev, "%s: Failed to get tclm_level property\n", __func__);
	}

	if (of_property_read_u32(np, "zinitix,afe_base", &tdata->afe_base) < 0) {
		tdata->afe_base = 0;
		input_err(true, dev, "%s: Failed to get afe_base property\n", __func__);
	}

	tdata->support_tclm_test = of_property_read_bool(np, "support_tclm_test");

	input_err(true, &client->dev, "%s: tclm_level %d, afe_base %04X%s\n",
		__func__, tdata->tclm_level, tdata->afe_base,
		tdata->support_tclm_test ? ", support_tclm_test" : "");
}
#endif

/* print raw data at booting time */
static void zt75xx_display_rawdata(struct zt75xx_ts_info *info, struct tsp_raw_data *raw_data, int *min, int *max, bool is_mis_cal)
{
	int x_num = info->cap_info.x_node_num;
	int y_num = info->cap_info.y_node_num;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int tmp_rawdata;
	int i, j;

	input_raw_info(true, &info->client->dev, "%s: %s\n", __func__, is_mis_cal ? "mis_cal" : "dnd");

	pStr = kzalloc(6 * (y_num + 1), GFP_KERNEL);
	if (pStr == NULL)
		return;

	memset(pStr, 0x0, 6 * (y_num + 1));
	snprintf(pTmp, sizeof(pTmp), "      Rx");
	strlcat(pStr, pTmp, 6 * (y_num + 1));

	for (i = 0; i < y_num; i++) {
		snprintf(pTmp, sizeof(pTmp), " %02d  ", i);
		strlcat(pStr, pTmp, 6 * (y_num + 1));
	}

	input_raw_info(true, &info->client->dev, "%s\n", pStr);

	memset(pStr, 0x0, 6 * (y_num + 1));
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, 6 * (y_num + 1));

	for (i = 0; i < y_num; i++) {
		snprintf(pTmp, sizeof(pTmp), "-----");
		strlcat(pStr, pTmp, 6 * (y_num + 1));
	}

	input_raw_info(true, &info->client->dev, "%s\n", pStr);

	for (i = 0; i < x_num; i++) {
		memset(pStr, 0x0, 6 * (y_num + 1));
		snprintf(pTmp, sizeof(pTmp), "Tx%02d | ", i);
		strlcat(pStr, pTmp, 6 * (y_num + 1));

		for (j = 0; j < y_num; j++) {
			if (is_mis_cal) {
				/* print mis_cal data (value - DEF_MIS_CAL_SPEC_MID) */
				tmp_rawdata = raw_data->reference_data_abnormal[(i * y_num) + j] - DEF_MIS_CAL_SPEC_MID;
				snprintf(pTmp, sizeof(pTmp), " %4d", tmp_rawdata);

				if (tmp_rawdata < *min)
					*min = tmp_rawdata;

				if (tmp_rawdata > *max)
					*max = tmp_rawdata;

			} else {
				/* print dnd data */
				tmp_rawdata = raw_data->dnd_data[(i * y_num) + j];
				snprintf(pTmp, sizeof(pTmp), " %4d", tmp_rawdata);

				if (tmp_rawdata < *min && tmp_rawdata != 0)
					*min = tmp_rawdata;

				if (tmp_rawdata > *max)
					*max = tmp_rawdata;
			}
			strlcat(pStr, pTmp, 6 * (y_num + 1));
		}
		input_raw_info(true, &info->client->dev, "%s\n", pStr);
	}

	input_raw_info(true, &info->client->dev, "Max/Min %d,%d ##\n", *max, *min);

	kfree(pStr);
}

static void zt75xx_run_dnd(struct zt75xx_ts_info *info)
{
	struct tsp_raw_data *raw_data = info->raw_data;
	int min = 0xFFFF, max = -0xFF;
	int ret;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(info->client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif

	ret = ts_set_touchmode(TOUCH_DND_MODE);
	if (ret < 0) {
		input_raw_info(true, &info->client->dev, "%s: failed to set testmode\n", __func__);
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->dnd_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	zt75xx_display_rawdata(info, raw_data, &min, &max, false);

out:
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void zt75xx_run_mis_cal(struct zt75xx_ts_info *info)
{
	struct zt75xx_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;

	char mis_cal_data = 0xF0;
	int ret = 0;
	u16 chip_eeprom_info;
	int min = 0xFFFF, max = -0xFF;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	write_cmd(client, ZT75XX_CLEAR_INT_STATUS_CMD);
#endif
	disable_irq(info->irq);

	if (pdata->mis_cal_check == 0) {
		input_raw_info(true, &info->client->dev, "%s: not support\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	if (info->work_state == SUSPEND) {
		input_raw_info(true, &info->client->dev, "%s: Touch is stopped\n",__func__);
		mis_cal_data = 0xF2;
		goto NG;
	}

	if (read_data(info->client, ZT75XX_EEPROM_INFO, (u8 *)&chip_eeprom_info, 2) < 0) {
		input_raw_info(true, &info->client->dev, "%s: read eeprom_info i2c fail\n", __func__);
		mis_cal_data = 0xF3;
		goto NG;
	}

	if (zinitix_bit_test(chip_eeprom_info, 0)) {
		input_raw_info(true, &info->client->dev, "%s: eeprom cal 0, skip\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	ret = ts_set_touchmode(TOUCH_REF_ABNORMAL_TEST_MODE);
	if (ret < 0) {
		input_raw_info(true, &info->client->dev, "%s: failed to set testmode\n", __func__);
		ts_set_touchmode(TOUCH_POINT_MODE);
		mis_cal_data = 0xF4;
		goto NG;
	}
	ret = get_raw_data(info, (u8 *)raw_data->reference_data_abnormal, 2);
	if (!ret) {
		input_raw_info(true, &info->client->dev, "%s: i2c fail\n", __func__);
		ts_set_touchmode(TOUCH_POINT_MODE);
		mis_cal_data = 0xF4;
		goto NG;
	}
	ts_set_touchmode(TOUCH_POINT_MODE);

	zt75xx_display_rawdata(info, raw_data, &min, &max, true);
	if ((min + DEF_MIS_CAL_SPEC_MID) < DEF_MIS_CAL_SPEC_MIN ||
			(max + DEF_MIS_CAL_SPEC_MID) > DEF_MIS_CAL_SPEC_MAX) {
		mis_cal_data = 0xFD;
		goto NG;
	}

	mis_cal_data = 0x00;
NG:
	input_raw_info(true, &info->client->dev, "%s : mis_cal_data: %X\n", __func__, mis_cal_data);
	enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
	write_reg(client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL,
		SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
#endif
}

static void zt75xx_run_rawdata(struct zt75xx_ts_info *info)
{
	info->tsp_dump_lock = 1;
#ifdef CONFIG_TOUCHSCREEN_DUAL_FOLDABLE
	input_raw_data_clear(0);
#else
	input_raw_data_clear();
#endif

	input_raw_info(true, &info->client->dev, "%s: start ##\n", __func__);
	zt75xx_run_dnd(info);
	zt75xx_run_mis_cal(info);
	input_raw_info(true, &info->client->dev, "%s: done ##\n", __func__);

	info->tsp_dump_lock = 0;
}

#if defined(CONFIG_TOUCHSCREEN_DUMP_MODE)
#include <linux/sec_debug.h>
extern struct tsp_dump_callbacks dump_callbacks;
static struct delayed_work *p_ghost_check;

static void zt75xx_check_rawdata(struct work_struct *work)
{
	struct zt75xx_ts_info *info = container_of(work, struct zt75xx_ts_info,
			ghost_check.work);

	if (info->tsp_dump_lock == 1) {
		input_info(true, &info->client->dev, "%s: ignored ## already checking..\n", __func__);
		return;
	}

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_info(true, &info->client->dev, "%s: ignored ## IC is power off\n", __func__);
		return;
	}

	zt75xx_run_rawdata(info);
}

static void dump_tsp_log(void)
{
	pr_info("%s: %s %s: start\n", ZT75XX_TS_DEVICE, SECLOG, __func__);

#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
		pr_err("%s: %s %s: ignored ## lpm charging Mode!!\n", ZT75XX_TS_DEVICE, SECLOG, __func__);
		return;
	}
#endif

	if (p_ghost_check == NULL) {
		pr_err("%s: %s %s: ignored ## tsp probe fail!!\n", ZT75XX_TS_DEVICE, SECLOG, __func__);
		return;
	}
	schedule_delayed_work(p_ghost_check, msecs_to_jiffies(100));
}
#endif

static void zt_read_info_work(struct work_struct *work)
{
	struct zt75xx_ts_info *info = container_of(work, struct zt75xx_ts_info,
			work_read_info.work);
#ifdef TCLM_CONCEPT
	u8 data[2] = {0};
	int ret;
#endif

	mutex_lock(&info->modechange);

#ifdef TCLM_CONCEPT
	get_zt_tsp_nvm_data(info, ZT75XX_TS_NVM_OFFSET_FAC_RESULT, (u8 *)data, 2);
	info->test_result.data[0] = data[0];
	ret = sec_tclm_check_cal_case(info->tdata);
	input_info(true, &info->client->dev, "%s: sec_tclm_check_cal_case result %d; test result %X\n",
				__func__, ret, info->test_result.data[0]);
#endif
	input_log_fix();
	zt75xx_run_rawdata(info);
	info->info_work_done = true;

	mutex_unlock(&info->modechange);

	schedule_work(&info->work_print_info.work);

#ifdef CONFIG_TOUCHSCREEN_DUAL_FOLDABLE
	if (info->change_flip_status) {
		input_info(true, &info->client->dev, "%s: re-try switching after reading info\n", __func__);
		schedule_work(&info->switching_work.work);
	}
#endif
}

static void zt_print_info_work(struct work_struct *work)
{
	struct zt75xx_ts_info *info = container_of(work, struct zt75xx_ts_info,
			work_print_info.work);

	zt75xx_print_info(info);
	schedule_delayed_work(&info->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

#ifdef CONFIG_TOUCHSCREEN_DUAL_FOLDABLE
static void zt75xx_switching_work(struct work_struct *work)
{
	struct zt75xx_ts_info *info = container_of(work, struct zt75xx_ts_info,
				switching_work.work);

	if (info == NULL) {
		input_err(true, NULL, "%s: tsp info is null\n", __func__);
		return;
	}

	if (info->flip_status != info->flip_status_current) {
		if (!info->info_work_done) {
			input_err(true, &info->client->dev, "%s: info_work is not done yet\n", __func__);
			info->change_flip_status = 1;
			return;
		}
		info->change_flip_status = 0;

		mutex_lock(&info->switching_mutex);
		info->flip_status = info->flip_status_current;

		if (info->flip_status == 0) {
			/* open : sub_tsp off */
		} else {
			/* close : sub_tsp on */
		}

		mutex_unlock(&info->switching_mutex);
	}
}

static int zt75xx_hall_ic_notify(struct notifier_block *nb,
			unsigned long flip_cover, void *v)
{
	struct zt75xx_ts_info *info = container_of(nb, struct zt75xx_ts_info,
				hall_ic_nb);

	if (info == NULL) {
		input_err(true, NULL, "%s: tsp info is null\n", __func__);
		return 0;
	}

	input_info(true, &info->client->dev, "%s: %s\n", __func__,
			 flip_cover ? "close" : "open");

	cancel_delayed_work(&info->switching_work);

	info->flip_status_current = flip_cover;

	schedule_work(&info->switching_work.work);

	return 0;
}
#endif

static int zt75xx_ts_probe(struct i2c_client *client,
		const struct i2c_device_id *i2c_id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct zt75xx_ts_platform_data *pdata = client->dev.platform_data;
	struct sec_tclm_data *tdata = NULL;
	struct zt75xx_ts_info *info;
	struct device_node *np = client->dev.of_node;
	int ret = 0;
	int i;
	bool force_update = false;
#if !defined(CONFIG_EXYNOS_DECON_MDNIE_LITE) && defined(CONFIG_DISPLAY_SAMSUNG)
	int lcdtype = 0;
#endif

#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
		input_err(true, &client->dev, "%s : Do not load driver due to : lpm %d\n",
				__func__, lpcharge);
		return -ENODEV;
	}
#endif

#ifdef CONFIG_DISPLAY_SAMSUNG
	lcdtype = get_lcd_attached("GET");
	if (lcdtype == 0xFFFFFF) {
		input_err(true, &client->dev, "%s: lcd is not attached\n", __func__);
		return -ENODEV;
	}
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	if (lcdtype == 0) {
		input_err(true, &client->dev, "%s: lcd is not attached\n", __func__);
		return -ENODEV;
	}
#endif

	if (client->dev.of_node) {
		if (!pdata) {
			pdata = devm_kzalloc(&client->dev,
					sizeof(*pdata), GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
		}
		ret = zt75xx_ts_parse_dt(np, &client->dev, pdata);
		if (ret){
			input_err(true, &client->dev, "%s: Error parsing dt %d\n", __func__, ret);
			goto err_no_platform_data;
		}
		tdata = devm_kzalloc(&client->dev,
				sizeof(struct sec_tclm_data), GFP_KERNEL);
		if (!tdata)
			goto error_allocate_tdata;

		sec_tclm_parse_dt(client, tdata);

		ret = zinitix_init_gpio(pdata);
		if (ret < 0)
			goto err_gpio_request;

	} else if (!pdata) {
		input_err(true, &client->dev, "%s: Not exist platform data\n", __func__);
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev, "%s: Not compatible i2c function\n", __func__);
		return -EIO;
	}

	info = kzalloc(sizeof(struct zt75xx_ts_info), GFP_KERNEL);
	if (!info) {
		input_err(true, &client->dev, "%s: Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	i2c_set_clientdata(client, info);
	info->client = client;
	info->pdata = pdata;

	info->tdata = tdata;
	if (!info->tdata)
		goto error_null_data;

#ifdef TCLM_CONCEPT
	sec_tclm_initialize(info->tdata);
	info->tdata->client = info->client;
	info->tdata->tclm_read = zt75xx_tclm_data_read;
	info->tdata->tclm_write = zt75xx_tclm_data_write;
	info->tdata->tclm_execute_force_calibration = zt_tclm_execute_force_calibration;
	info->tdata->tclm_parse_dt = sec_tclm_parse_dt;
#endif
	INIT_DELAYED_WORK(&info->work_read_info, zt_read_info_work);
	INIT_DELAYED_WORK(&info->work_print_info, zt_print_info_work);
	mutex_init(&info->modechange);

	info->input_dev = input_allocate_device();
	if (!info->input_dev) {
		input_err(true, &client->dev, "%s: Failed to allocate input device\n", __func__);
		ret = -ENOMEM;
		goto err_alloc;
	}

	info->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(info->pinctrl)) {
		input_err(true, &client->dev, "%s: Failed to get pinctrl data\n", __func__);
		ret = PTR_ERR(info->pinctrl);
		goto err_get_pinctrl;
	}

	info->work_state = PROBE;

	// power on
	if (!zt75xx_power_control(info, POWER_ON_SEQUENCE)) {
		ret = -EPERM;
		goto err_power_sequence;
	}

	/* To Do */
	/* FW version read from tsp */

	memset(&info->reported_touch_info, 0x0, sizeof(struct point_info));

	/* init touch mode */
	info->touch_mode = TOUCH_POINT_MODE;
	misc_info = info;
	mutex_init(&info->set_reg_lock);

#if ESD_TIMER_INTERVAL
	spin_lock_init(&info->lock);
	INIT_WORK(&info->tmr_work, ts_tmr_work);

	esd_tmr_workqueue = create_singlethread_workqueue("esd_tmr_workqueue");
	if (!esd_tmr_workqueue) {
		input_err(true, &client->dev, "Failed to create esd tmr work queue\n");
		ret = -EPERM;

		goto err_esd_sequence;
	}

	esd_timer_init(info);
#endif
	sema_init(&info->work_lock, 1);

	ret = ic_version_check(info);
	if (ret < 0) {
		input_err(true, &info->client->dev,
			"%s: fail version check", __func__);
		force_update = true;
	}

	ret = fw_update_work(info, force_update);
	if (ret < 0) {
		ret = -EPERM;
		input_err(true, &info->client->dev,
			"%s: fail update_work", __func__);
		goto err_fw_update;
	}

	if (pdata->support_touchkey) {
		for (i = 0; i < MAX_SUPPORTED_BUTTON_NUM; i++)
			info->button[i] = ICON_BUTTON_UNCHANGE;
	}
	snprintf(info->phys, sizeof(info->phys),
		"%s/input0", dev_name(&client->dev));
	info->input_dev->name = "sec_touchscreen2";
	info->input_dev->id.bustype = BUS_I2C;
/*	info->input_dev->id.vendor = 0x0001; */
	info->input_dev->phys = info->phys;
/*	info->input_dev->id.product = 0x0002; */
/*	info->input_dev->id.version = 0x0100; */
	info->input_dev->dev.parent = &client->dev;

#ifdef GLOVE_MODE
	input_set_capability(info->input_dev, EV_SW, SW_GLOVE);
#endif

	set_bit(EV_SYN, info->input_dev->evbit);
	set_bit(EV_KEY, info->input_dev->evbit);
	set_bit(EV_ABS, info->input_dev->evbit);
	set_bit(BTN_TOUCH, info->input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, info->input_dev->propbit);
	set_bit(EV_LED, info->input_dev->evbit);
	set_bit(LED_MISC, info->input_dev->ledbit);
	if (pdata->support_touchkey) {
		for (i = 0; i < MAX_SUPPORTED_BUTTON_NUM; i++)
			set_bit(BUTTON_MAPPING_KEY[i], info->input_dev->keybit);
	}

	if (pdata->support_lpm_mode) {
		set_bit(KEY_BLACK_UI_GESTURE, info->input_dev->keybit);
		set_bit(KEY_HOMEPAGE, info->input_dev->keybit);
	}

	input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
		0, pdata->x_resolution + ABS_PT_OFFSET, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
		0, pdata->y_resolution + ABS_PT_OFFSET, 0, 0);
#ifdef CONFIG_SEC_FACTORY
	input_set_abs_params(info->input_dev, ABS_MT_PRESSURE,
		0, 3000, 0, 0);
#endif
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR,
		0, 255, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_WIDTH_MAJOR,
		0, 255, 0, 0);

#ifdef SUPPORTED_PALM_TOUCH
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MINOR,
		0, 255, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_CUSTOM,
		0, 1, 0, 0);
#endif

	set_bit(MT_TOOL_FINGER, info->input_dev->keybit);
	input_mt_init_slots(info->input_dev, MAX_SUPPORTED_FINGER_NUM,
			INPUT_MT_DIRECT);

	input_set_drvdata(info->input_dev, info);
	ret = input_register_device(info->input_dev);
	if (ret) {
		input_info(true, &client->dev, "%s: unable to register %s input device\n",
			__func__, info->input_dev->name);
		goto err_input_register_device;
	}

	if (init_touch(info) == false) {
		ret = -EPERM;
		goto err_init_touch;
	}

	info->work_state = NOTHING;

	wake_lock_init(&info->wakelock, WAKE_LOCK_SUSPEND, "tsp_wakelock");
	init_completion(&info->resume_done);
	complete_all(&info->resume_done);

	/* configure irq */
	info->irq = gpio_to_irq(pdata->gpio_int);
	if (info->irq < 0) {
		input_err(true, &client->dev, "%s: error gpio_to_irq\n", __func__);
		ret = -EINVAL;
		goto error_gpio_irq;
	}

	/* ret = request_threaded_irq(info->irq, ts_int_handler, zt75xx_touch_work */
	ret = request_threaded_irq(info->irq, NULL, zt75xx_touch_work,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT , ZT75XX_TS_DEVICE, info);
	if (ret) {
		input_info(true, &client->dev, "%s: unable to register irq.(%s)\n",
			__func__, info->input_dev->name);
		goto err_request_irq;
	}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	trustedui_set_tsp_irq(info->irq);
	input_info(true, &client->dev, "%s: [%d] called!\n", __func__, info->irq);
#endif

#ifdef CONFIG_INPUT_ENABLED
	info->input_dev->open = zt75xx_ts_open;
	info->input_dev->close = zt75xx_ts_close;
#endif

	sema_init(&info->raw_data_lock, 1);
#ifdef USE_MISC_DEVICE
	ret = misc_register(&touch_misc_device);
	if (ret) {
		input_err(true, &client->dev, "%s: Failed to register touch misc device\n", __func__);
		goto err_misc_register;
	}
#endif
#ifdef SEC_FACTORY_TEST
	ret = init_sec_factory(info);
	if (ret) {
		input_err(true, &client->dev, "%s: Failed to init sec factory device\n", __func__);

		goto err_kthread_create_failed;
	}
#endif

	info->register_cb = info->pdata->register_cb;

	info->callbacks.inform_charger = zt75xx_charger_status_cb;
	if (info->register_cb)
		info->register_cb(&info->callbacks);
#ifdef CONFIG_VBUS_NOTIFIER
	vbus_notifier_register(&info->vbus_nb, tsp_vbus_notification,
				VBUS_NOTIFY_DEV_CHARGER);
#endif

	if (pdata->support_lpm_mode) {
		device_init_wakeup(&client->dev, true);
	}

#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (sysfs_create_group(&info->input_dev->dev.kobj, &secure_attr_group) < 0)
		input_err(true, &info->client->dev, "%s: do not make secure group\n", __func__);
	else
		secure_touch_init(info);
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	tui_tsp_info = info;
#endif

#ifdef CONFIG_TOUCHSCREEN_DUAL_FOLDABLE
	mutex_init(&info->switching_mutex);
	INIT_DELAYED_WORK(&info->switching_work, zt75xx_switching_work);

	/* Hall IC notify priority -> ftn -> register */
	info->flip_status = -1;
	info->flip_status_current = -1;
	info->hall_ic_nb.priority = 1;
	info->hall_ic_nb.notifier_call = zt75xx_hall_ic_notify;
	hall_ic_register_notify(&info->hall_ic_nb);
	input_info(true, &info->client->dev, "%s: hall ic register\n", __func__);
#endif

	schedule_delayed_work(&info->work_read_info, msecs_to_jiffies(50));

#if defined(CONFIG_TOUCHSCREEN_DUMP_MODE)
	dump_callbacks.inform_dump = dump_tsp_log;
	INIT_DELAYED_WORK(&info->ghost_check, zt75xx_check_rawdata);
	p_ghost_check = &info->ghost_check;
#endif
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	sec_secure_touch_register(info, info->pdata->ss_touch_num, &info->input_dev->dev.kobj);
#endif
	input_info(true, &client->dev, "%s: done\n", __func__);
	input_log_fix();

	return 0;

#ifdef SEC_FACTORY_TEST
err_kthread_create_failed:
#ifdef CONFIG_TOUCHSCREEN_DUAL_FOLDABLE
	sec_cmd_exit(&info->sec, SEC_CLASS_DEVT_TSP2);
#else
	sec_cmd_exit(&info->sec, SEC_CLASS_DEVT_TSP);
#endif
	kfree(info->raw_data);
#endif
#ifdef USE_MISC_DEVICE
err_misc_register:
#endif
	free_irq(info->irq, info);
err_request_irq:
error_gpio_irq:
	wake_lock_destroy(&info->wakelock);
err_init_touch:
	input_unregister_device(info->input_dev);
err_input_register_device:
err_fw_update:
#if ESD_TIMER_INTERVAL
	del_timer(&(info->esd_timeout_tmr));
err_esd_sequence:
#endif
err_power_sequence:
	zt75xx_power_control(info, POWER_OFF);
err_get_pinctrl:
	input_free_device(info->input_dev);
	info->input_dev = NULL;
error_null_data:
err_alloc:
	kfree(info);
err_gpio_request:
error_allocate_tdata:
	if (IS_ENABLED(CONFIG_OF))
		devm_kfree(&client->dev, (void *)tdata);
err_no_platform_data:
	if (IS_ENABLED(CONFIG_OF))
		devm_kfree(&client->dev, (void *)pdata);

#ifdef CONFIG_TOUCHSCREEN_DUMP_MODE
		p_ghost_check = NULL;
#endif
	input_info(true, &client->dev, "%s: Failed to probe\n", __func__);
	input_log_fix();

	return ret;
}

static int zt75xx_ts_remove(struct i2c_client *client)
{
	struct zt75xx_ts_info *info = i2c_get_clientdata(client);
	struct zt75xx_ts_platform_data *pdata = info->pdata;

	disable_irq(info->irq);
	down(&info->work_lock);

	info->work_state = REMOVE;

	cancel_delayed_work_sync(&info->work_read_info);
	cancel_delayed_work_sync(&info->work_print_info);

#ifdef SEC_FACTORY_TEST
#ifdef CONFIG_TOUCHSCREEN_DUAL_FOLDABLE
	sec_cmd_exit(&info->sec, SEC_CLASS_DEVT_TSP2);
#else
	sec_cmd_exit(&info->sec, SEC_CLASS_DEVT_TSP);
#endif
	kfree(info->raw_data);
#endif

#ifdef CONFIG_TOUCHSCREEN_DUMP_MODE
	p_ghost_check = NULL;
#endif

#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
	write_reg(info->client, ZT75XX_PERIODICAL_INTERRUPT_INTERVAL, 0);
	esd_timer_stop(info);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "%s: Stopped esd timer\n", __func__);
#endif
	destroy_workqueue(esd_tmr_workqueue);
#endif

	if (info->irq)
		free_irq(info->irq, info);
	wake_lock_destroy(&info->wakelock);
#ifdef USE_MISC_DEVICE
	misc_deregister(&touch_misc_device);
#endif

	if (gpio_is_valid(pdata->gpio_int) != 0)
		gpio_free(pdata->gpio_int);
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	secure_touch_remove(info);
#endif
	input_unregister_device(info->input_dev);
	input_free_device(info->input_dev);
	up(&info->work_lock);
	kfree(info);

	return 0;
}

void zt75xx_ts_shutdown(struct i2c_client *client)
{
	struct zt75xx_ts_info *info = i2c_get_clientdata(client);

	input_info(true, &client->dev, "%s++\n",__func__);
	disable_irq(info->irq);
	down(&info->work_lock);
#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
	esd_timer_stop(info);
#endif
	up(&info->work_lock);
	zt75xx_power_control(info, POWER_OFF);
	input_info(true, &client->dev, "%s--\n",__func__);
}

#ifdef CONFIG_PM
static int zt75xx_ts_pm_suspend(struct device *dev)
{
	struct zt75xx_ts_info *info = dev_get_drvdata(dev);

	reinit_completion(&info->resume_done);

	return 0;
}

static int zt75xx_ts_pm_resume(struct device *dev)
{
	struct zt75xx_ts_info *info = dev_get_drvdata(dev);

	complete_all(&info->resume_done);

	return 0;
}

static const struct dev_pm_ops zt75xx_ts_dev_pm_ops = {
	.suspend = zt75xx_ts_pm_suspend,
	.resume = zt75xx_ts_pm_resume,
};
#endif

static struct i2c_device_id zt75xx_idtable[] = {
	{ZT75XX_TS_DEVICE, 0},
	{ }
};

#ifdef CONFIG_OF
static const struct of_device_id zinitix_match_table[] = {
	{ .compatible = "zinitix,zt75xx_ts_device",},
	{},
};
#endif

static struct i2c_driver zt75xx_ts_driver = {
	.probe	= zt75xx_ts_probe,
	.remove	= zt75xx_ts_remove,
	.shutdown = zt75xx_ts_shutdown,
	.id_table	= zt75xx_idtable,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= ZT75XX_TS_DEVICE,
#ifdef CONFIG_OF
		.of_match_table = zinitix_match_table,
#endif
#ifdef CONFIG_PM
		.pm = &zt75xx_ts_dev_pm_ops,
#endif
	},
};

static int __init zt75xx_ts_init(void)
{
	pr_info("%s\n", __func__);
	return i2c_add_driver(&zt75xx_ts_driver);
}

static void __exit zt75xx_ts_exit(void)
{
	i2c_del_driver(&zt75xx_ts_driver);
}

module_init(zt75xx_ts_init);
module_exit(zt75xx_ts_exit);

MODULE_DESCRIPTION("touch-screen device driver using i2c interface");
MODULE_AUTHOR("<mika.kim@samsung.com>");
MODULE_LICENSE("GPL");
