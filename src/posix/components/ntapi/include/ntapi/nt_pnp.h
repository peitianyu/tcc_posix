#ifndef _NT_PNP_H_
#define _NT_PNP_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"

typedef enum _nt_latency_time {
	NT_LT_DONT_CARE,
	NT_LT_LOWEST_LATENCY
} nt_latency_time;


typedef enum _nt_device_power_state {
	NT_POWER_DEVICE_UNSPECIFIED,
	NT_POWER_DEVICE_D0,
	NT_POWER_DEVICE_D1,
	NT_POWER_DEVICE_D2,
	NT_POWER_DEVICE_D3
} nt_device_power_state;


typedef enum _nt_power_action {
	NT_POWER_ACTION_NONE,
	NT_POWER_ACTION_RESERVED,
	NT_POWER_ACTION_SLEEP,
	NT_POWER_ACTION_HIBERNATE,
	NT_POWER_ACTION_SHUTDOWN,
	NT_POWER_ACTION_SHUTDOWN_RESET,
	NT_POWER_ACTION_SHUTDOWN_OFF,
} nt_power_action;


typedef enum _nt_system_power_state {
	NT_POWER_SYSTEM_UNSPECIFIED,
	NT_POWER_SYSTEM_WORKING,
	NT_POWER_SYSTEM_SLEEPING1,
	NT_POWER_SYSTEM_SLEEPING2,
	NT_POWER_SYSTEM_SLEEPING3,
	NT_POWER_SYSTEM_HIBERNATE,
	NT_POWER_SYSTEM_SHUTDOWN
} nt_system_power_state;


typedef enum _nt_power_info_level {
	NT_SYSTEM_POWER_POLICY_AC,
	NT_SYSTEM_POWER_POLICY_DC,
	NT_VERIFY_SYSTEM_POLICY_AC,
	NT_VERIFY_SYSTEM_POLICY_DC,
	NT_SYSTEM_POWER_CAPABILITIES,
	NT_SYSTEM_BATTERY_STATE,
	NT_SYSTEM_POWER_STATE_HANDLER,
	NT_PROCESSOR_STATE_HANDLER,
	NT_SYSTEM_POWER_POLICY_CURRENT,
	NT_ADMINISTRATOR_POWER_POLICY,
	NT_SYSTEM_RESERVE_HIBER_FILE,
	NT_PROCESSOR_INFORMATION,
	NT_SYSTEM_POWER_INFORMATION
} nt_power_info_level;


/* execution state bits */
#define NT_ES_SYSTEM_REQUIRED		(0x00000001)
#define NT_ES_DISPLAY_REQUIRED		(0x00000002)
#define NT_ES_USER_PRESENT		(0x00000004)
#define NT_ES_AWAYMODE_REQUIRED		(0x00000040)
#define NT_ES_CONTINUOUS		(0x80000000)


/* power action flag bits */
#define NT_POWER_ACTION_QUERY_ALLOWED	(0x00000001)
#define NT_POWER_ACTION_UI_ALLOWED	(0x00000002)
#define NT_POWER_ACTION_OVERRIDE_APPS	(0x00000004)
#define NT_POWER_ACTION_LIGHTEST_FIRST	(0x10000000)
#define NT_POWER_ACTION_LOCK_CONSOLE	(0x20000000)
#define NT_POWER_ACTION_DISABLE_WAKES	(0x40000000)
#define NT_POWER_ACTION_CRITICAL	(0x80000000)


/* power state event codes */
#define NT_POWER_LEVEL_USER_NOTIFY_TEXT		(0x00000001)
#define NT_POWER_LEVEL_USER_NOTIFY_SOUND	(0x00000002)
#define NT_POWER_LEVEL_USER_NOTIFY_EXEC		(0x00000004)
#define NT_POWER_USER_NOTIFY_BUTTON		(0x00000008)
#define NT_POWER_USER_NOTIFY_SHUTDOWN		(0x00000010)
#define NT_POWER_FORCE_TRIGGER_RESET		(0x80000000)

/* system power policy */
#define NT_NUM_DISCHARGE_POLICIES		(0x00000004)



typedef struct _nt_power_action_policy {
	nt_power_action		action;
	uint32_t		action_flags;
	uint32_t		event_code;
} nt_power_action_policy;


typedef struct _nt_system_power_level {
	unsigned char			enable;
	unsigned char			padding[3];
	uint32_t			battery_level;
	nt_power_action_policy		power_policy;
	nt_system_power_state		min_system_state;
} nt_system_power_level;


typedef struct _nt_system_power_policy {
	uint32_t		revision;
	nt_power_action_policy	power_button;
	nt_power_action_policy	sleep_button;
	nt_power_action_policy	lid_close;
	nt_system_power_state 	lid_open_wake;
	uint32_t		reserved_1st;
	nt_power_action_policy	idle;
	uint32_t		idle_timeout;
 	unsigned char		idle_sensitivity;
 	unsigned char		dynamic_throttle;
 	unsigned char		padding[2];
	nt_system_power_state	min_sleep;
	nt_system_power_state	max_sleep;
	nt_system_power_state	reduced_latency_sleep;
	uint32_t		win_logon_flags;
	uint32_t		reserved_2nd;
	uint32_t		doze_s4_timeout;
	uint32_t		broadcast_capacity_resolution;
 	nt_system_power_level	discharge_policy[NT_NUM_DISCHARGE_POLICIES];
	uint32_t		video_timeout;
 	unsigned char		video_dim_display;
	uint32_t		video_reserved[3];
	uint32_t		spindown_timeout;
 	unsigned char		optimize_for_power;
 	unsigned char		fan_throttle_tolerance;
 	unsigned char		forced_throttle;
 	unsigned char		min_throttle;
	nt_power_action_policy	over_throttled;
} nt_system_power_policy;


typedef struct _nt_system_battery_state {
	unsigned char	ac_on_line;
	unsigned char	battery_present;
	unsigned char	charging;
	unsigned char	discharging;
	unsigned char	padding[4];
	uint32_t	max_capacity;
	uint32_t	remaining_capacity;
	uint32_t	rate;
	uint32_t	estimated_time;
	uint32_t	default_alert1;
	uint32_t	default_alert2;
} nt_system_battery_state;


typedef struct _nt_battery_reporting_scale {
	uint32_t	granularity;
	uint32_t	capacity;
} nt_battery_reporting_scale;


typedef struct _nt_system_power_capabilities {
	unsigned char			power_button_present;
	unsigned char			sleep_button_present;
	unsigned char			lid_present;
	unsigned char			system_s1;
	unsigned char			system_s2;
	unsigned char			system_s3;
	unsigned char			system_s4;
	unsigned char			system_s5;
	unsigned char			hiber_file_present;
	unsigned char			full_wake;
	unsigned char			video_dim_present;
	unsigned char			apm_present;
	unsigned char			ups_present;
	unsigned char			thermal_control;
	unsigned char			processor_throttle;
	unsigned char			processor_min_throttle;
	unsigned char			processor_max_throttle;
	unsigned char			fast_system_s4;
	unsigned char			hiber_boot;
	unsigned char			wake_alarm_present;
	unsigned char			ao_ac;
	unsigned char			disk_spin_down;
	unsigned char			padding[8];
	unsigned char			system_batteries_present;
	unsigned char			batteries_are_short_term;
	nt_battery_reporting_scale	battery_scale[3];
	nt_system_power_state		ac_on_line_wake;
	nt_system_power_state		soft_lid_wake;
	nt_system_power_state		rtc_wake;
	nt_system_power_state		min_device_wake_state;
	nt_system_power_state		default_low_latency_wake;
} nt_system_power_capabilities;


typedef struct _nt_system_power_information {
	uint32_t	max_idleness_allowed;
	uint32_t	idleness;
	uint32_t	time_remaining;
	unsigned char	cooling_mode;
} nt_system_power_information;


typedef struct _nt_system_power_status {
	unsigned char	ac_line_status;
	unsigned char	battery_flag;
	unsigned char	battery_life_percent;
	unsigned char	reserved;
	uint32_t	battery_life_time;
	uint32_t	battery_full_life_time;
} nt_system_power_status;


typedef struct _nt_administrator_power_policy {
	nt_system_power_state	min_sleep;
	nt_system_power_state	max_sleep;
	uint32_t		min_video_timeout;
	uint32_t		max_video_timeout;
	uint32_t		min_spindown_timeout;
	uint32_t		max_spindown_timeout;
} nt_administrator_power_policy;


typedef struct _nt_user_power_policy {
	uint32_t		revision;
	nt_power_action_policy	idle_ac;
	nt_power_action_policy	idle_dc;
	uint32_t		idle_timeout_ac;
	uint32_t		idle_timeout_dc;
	unsigned char		idle_sensitivity_ac;
	unsigned char		idle_sensitivity_dc;
	unsigned char		throttle_policy_ac;
	unsigned char		throttle_policy_dc;
	nt_system_power_state	max_sleep_ac;
	nt_system_power_state	max_sleep_dc;
	uint32_t		reserved[2];
	uint32_t		video_timeout_ac;
	uint32_t		video_timeout_dc;
	uint32_t		spindown_timeout_ac;
	uint32_t		spindown_timeout_dc;
	unsigned char		optimize_for_power_ac;
	unsigned char		optimize_for_power_dc;
	unsigned char		fan_throttle_tolerance_ac;
	unsigned char		fan_throttle_tolerance_dc;
	unsigned char		forced_throttle_ac;
	unsigned char		forced_throttle_dc;
} nt_user_power_policy;


typedef struct _nt_processor_power_information {
	uint32_t	number;
	uint32_t	max_mhz;
	uint32_t	current_mhz;
	uint32_t	mhz_limit;
	uint32_t	max_idle_state;
	uint32_t	current_idle_state;
} nt_processor_power_information;


typedef struct _nt_plug_play_notification_header {
	uint16_t	version;
	uint16_t	size;
	nt_guid		event;
} nt_plug_play_notification_header;


/* ZwRequestWakeupLatency: no longer present */
typedef int32_t __stdcall ntapi_zw_request_wakeup_latency(
	__in	nt_latency_time		latency);


/* ZwRequestDeviceWakeup: no longer present */
typedef int32_t __stdcall ntapi_zw_request_device_wakeup(
	__in	void *	hdevice);


/* ZwCancelDeviceWakeupRequest: no longer present */
typedef int32_t __stdcall ntapi_zw_cancel_device_wakeup_request(
	__in	void *	hdevice);


typedef int32_t __stdcall ntapi_zw_is_system_resume_automatic(void);


typedef int32_t __stdcall ntapi_zw_set_thread_execution_state(
	__in	uint32_t	execution_state,
	__out	uint32_t *	prev_execution_state);


typedef int32_t __stdcall ntapi_zw_get_device_power_state(
	__in	void *				hdevice,
	__out	nt_device_power_state *		device_power_state);


typedef int32_t __stdcall ntapi_zw_set_system_power_state(
	__in	nt_power_action		system_action,
	__in	nt_system_power_state	min_system_state,
	__in	uint32_t		system_power_state_flags);


typedef int32_t __stdcall ntapi_zw_initiate_power_action(
	__in	nt_power_action		system_action,
	__in	nt_system_power_state	min_system_statem,
	__in	uint32_t		system_power_state_flags,
	__in	unsigned char		async);

typedef int32_t __stdcall ntapi_zw_power_information(
	__in	nt_power_info_level	power_info_level,
	__in	void *			input_buffer		__optional,
	__in	uint32_t		input_buffer_length,
	__out	void *			output_buffer		__optional,
	__in	uint32_t		output_buffer_length);


typedef int32_t __stdcall ntapi_zw_plug_play_control(
	__in		uint32_t	pnp_control_code,
	__in_out	void *		buffer,
	__in		uint32_t	buffer_length);

typedef int32_t __stdcall ntapi_zw_get_plug_play_event(
	__in	uint32_t	reserved_1st,
	__in	uint32_t	reserved_2nd,
	__out	void *		buffer,
	__in	uint32_t	buffer_length);

#endif
