/******************************************************************************
 *  FILE
 *      main.c
 *
 *  DESCRIPTION
 *      This file implements a minimal CSR uEnergy beacon application.
 *
 ******************************************************************************/

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <main.h>           /* Functions relating to powering up the device */
#include <pio.h>            /* in/out pins functions */
#include <ls_app_if.h>      /* Link Supervisor application interface */
#include <timer.h>          /* Chip timer functions */
#include <panic.h>          /* Support for applications to panic */
#include <gap_app_if.h>     /* GAP function */
#include <thermometer.h>    /* Internal themperature sensor */
#include <battery.h>        /* Measure battery level in mV */
#include <sleep.h>

/*============================================================================*
 *  Private Definitions
 *============================================================================*/

/* Number of timers used in this application */
#define MAX_TIMERS 1

#define LED 10
#define SENSOR 11

/*============================================================================*
 *  Private Data
 *============================================================================*/

/* Declare timer buffer to be managed by firmware library */
static uint16 app_timers[SIZEOF_APP_TIMER * MAX_TIMERS];

uint8 data_b[] = {0xff, 0x4c, 0x00, 0x02, 0x15, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x00, 0x01, 0x00, 0x01, 0x30};
/*============================================================================*
 *  Private Function Prototypes
 *============================================================================*/
static void startTimer(timer_id const);
void advUpdate(void);

/*============================================================================*
 *  Private Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *  startTimer    
 *
 *  DESCRIPTION
 *      change led state
 *      send temperature via Bluetooth
 *  PARAMETERS
 *      timer ID
 *  RETURNS
 *      none
 *----------------------------------------------------------------------------*/
static void startTimer(timer_id const id)
{
    /* Now starting a timer */
    const timer_id tId = TimerCreate(1* SECOND, TRUE, startTimer);
    if (tId == TIMER_INVALID){
        Panic(0xfe);
    }
    PioSet(LED, !PioGet(LED));
    
    /* Create new message */
    int16 temp = ThermometerReadTemperature();
    data_b[24] = temp;
    data_b[23] =  temp>>8;
    int16 bat = BatteryReadVoltage()/10-200; /* in 10mV and from 2V */
    data_b[22] = bat;
    
    advUpdate();
}

/*----------------------------------------------------------------------------*
 *  NAME
 *  advUpdate    
 *
 *  DESCRIPTION
 *      update advertisig data
 *  PARAMETERS
 *      none
 *  RETURNS
 *      none
 *----------------------------------------------------------------------------*/
void advUpdate(void){
    LsStoreAdvScanData(0, data_b, ad_src_advertise); /*Remove old adv data*/
    LsStoreAdvScanData(26, data_b, ad_src_advertise); /*set new*/
    LsStartStopAdvertise(TRUE, whitelist_disabled, ls_addr_type_public); /* update advertising data... */
}

/*============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      AppPowerOnReset
 *
 *  DESCRIPTION
 *      This user application function is called just after a power-on reset
 *      (including after a firmware panic), or after a wakeup from Hibernate or
 *      Dormant sleep states.
 *
 *      At the time this function is called, the last sleep state is not yet
 *      known.
 *
 *      NOTE: this function should only contain code to be executed after a
 *      power-on reset or panic. Code that should also be executed after an
 *      HCI_RESET should instead be placed in the AppInit() function.
 *
 *  PARAMETERS
 *      None
 *
 *  RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
void AppPowerOnReset(void)
{
    /* Code that is only executed after a power-on reset or firmware panic
     * should be implemented here - e.g. configuring application constants
     */
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      AppInit
 *
 *  DESCRIPTION
 *      This user application function is called after a power-on reset
 *      (including after a firmware panic), after a wakeup from Hibernate or
 *      Dormant sleep states, or after an HCI Reset has been requested.
 *
 *      NOTE: In the case of a power-on reset, this function is called
 *      after AppPowerOnReset().
 *
 *  PARAMETERS
 *      last_sleep_state [in]   Last sleep state
 *
 *  RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
void AppInit(sleep_state last_sleep_state)
{
    /* init pins */
    PioSetMode(LED, pio_mode_user);
    PioSetDir(LED, TRUE);
    
    PioSetMode(SENSOR, pio_mode_user);
    PioSetDir(SENSOR, FALSE);
    PioSetPullModes((1UL << SENSOR), pio_mode_weak_pull_up);
    
    /* Set the sensor to generate sys_event_pio_changed when pressed as well
     * as released */
    PioSetEventMask((1UL << SENSOR), pio_event_mode_both);
    
    /*init timer*/
    TimerInit(MAX_TIMERS, (void *)app_timers);
    startTimer(0);
    
    /*B Smart adv */
    GapSetMode(gap_role_broadcaster, gap_mode_discover_no, gap_mode_connect_no, gap_mode_bond_no, gap_mode_security_unauthenticate);    
    GapSetStaticAddress();
    GapSetAdvInterval(500*MILLISECOND, 500*MILLISECOND);
    
    
    LsStoreAdvScanData(26, data_b, ad_src_advertise);	
    LsStartStopScan(FALSE, whitelist_disabled, ls_addr_type_public);		
    LsStartStopAdvertise(TRUE, whitelist_disabled, ls_addr_type_public);
    
    /*Sleep mode*/
    SleepModeChange(sleep_mode_deep); /* default */
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      AppProcesSystemEvent
 *
 *  DESCRIPTION
 *      This user application function is called whenever a system event, such
 *      as a battery low notification, is received by the system.
 *
 *  PARAMETERS
 *      id   [in]               System event ID
 *      data [in]               Event data
 *
 *  RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
void AppProcessSystemEvent(sys_event_id id, void *data)
{

    if (id == sys_event_pio_changed)
    {
        data_b[21] = PioGet(SENSOR);
        advUpdate();
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      AppProcessLmEvent
 *
 *  DESCRIPTION
 *      This user application function is called whenever a LM-specific event
 *      is received by the system.
 *
 *  PARAMETERS
 *      event_code [in]         LM event ID
 *      p_event_data [in]       LM event data
 *
 *  RETURNS
 *      Always returns TRUE. See the Application module in the Firmware Library
 *      documentation for more information.
 *----------------------------------------------------------------------------*/
bool AppProcessLmEvent(lm_event_code event_code, LM_EVENT_T *event_data)
{
    /* This application does not process any LM-specific events */
    return TRUE;
}
