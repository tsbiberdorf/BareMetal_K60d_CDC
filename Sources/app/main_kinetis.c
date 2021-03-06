/******************************************************************************
 *
 * Freescale Semiconductor Inc.
 * (c) Copyright 2004-2010 Freescale Semiconductor, Inc.
 * ALL RIGHTS RESERVED.
 *
 ******************************************************************************
 *
 * THIS SOFTWARE IS PROVIDED BY FREESCALE "AS IS" AND ANY EXPRESSED OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  
 * IN NO EVENT SHALL FREESCALE OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 **************************************************************************//*!
 *
 * @file main_kinetis.c
 *
 * @author
 *
 * @version
 *
 * @date
 *
 * @brief   This software is the USB driver stack for S08 family
 *****************************************************************************/
#include "types.h"
#include "derivative.h" /* include peripheral declarations */
#include "user_config.h"
#include "RealTimerCounter.h"
#include "wdt_kinetis.h"
#include "hidef.h"
#ifndef _SERIAL_AGENT_   
#include "usb_dci_kinetis.h"
#include "usb_dciapi.h"
#endif
/*****************************************************************************
 * Global Functions Prototypes
 *****************************************************************************/
extern uint_8 TimerQInitialize(uint_8 ControllerId);
extern void TestApp_Init(void);
extern void TestApp_Task(void);
#ifdef MCU_MK70F12
extern void sci2_init();
#endif /* MCU_MK70F12 */
#if (defined MCU_MK20D7) || (defined MCU_MK40D7)
    #define MCGOUTCLK_72_MHZ
#endif

#if (defined MCU_MK60N512VMD100) || (defined MCU_MK53N512CMD100)
	#define BSP_CLOCK_SRC   (50000000ul)    /* crystal, oscillator freq. */
#elif (defined MCU_MK40N512VMD100)
   #if (defined KWIKSTIK)
    #define BSP_CLOCK_SRC   (4000000ul)     /* crystal, oscillator freq. */
   #else
	#define BSP_CLOCK_SRC   (8000000ul)     /* crystal, oscillator freq. */
   #endif
#else
    #define BSP_CLOCK_SRC   (8000000ul)     /* crystal, oscillator freq. */
#endif
#define BSP_REF_CLOCK_SRC   (2000000ul)     /* must be 2-4MHz */


#ifndef MCGOUTCLK_72_MHZ
#define PLL_48              (1)
#define PLL_96              (0)
#else
#define PLL_48              (0)
#define PLL_96              (0)
#endif

#ifdef MCGOUTCLK_72_MHZ
	#define BSP_CORE_DIV    (1)
	#define BSP_BUS_DIV     (2)
	#define BSP_FLEXBUS_DIV (2)
	#define BSP_FLASH_DIV   (3)

	// BSP_CLOCK_MUL from interval 24 - 55
	#define BSP_CLOCK_MUL   (36)    // 72MHz
#elif PLL_48
	#define BSP_CORE_DIV    (1)
	#define BSP_BUS_DIV     (1)
	#define BSP_FLEXBUS_DIV (1)
	#define BSP_FLASH_DIV   (2)

	// BSP_CLOCK_MUL from interval 24 - 55
	#define BSP_CLOCK_MUL   (24)    // 48MHz
#elif PLL_96
    #define BSP_CORE_DIV    (1)
    #define BSP_BUS_DIV     (2)
    #define BSP_FLEXBUS_DIV (2)
    #define BSP_FLASH_DIV   (4)
    // BSP_CLOCK_MUL from interval 24 - 55
    #define BSP_CLOCK_MUL   (48)    // 96MHz
#endif

#define BSP_REF_CLOCK_DIV   (BSP_CLOCK_SRC / BSP_REF_CLOCK_SRC)

#define BSP_CLOCK           (BSP_REF_CLOCK_SRC * BSP_CLOCK_MUL)
#define BSP_CORE_CLOCK      (BSP_CLOCK / BSP_CORE_DIV)      /* CORE CLK, max 100MHz */
#define BSP_SYSTEM_CLOCK    (BSP_CORE_CLOCK)                /* SYSTEM CLK, max 100MHz */
#define BSP_BUS_CLOCK       (BSP_CLOCK / BSP_BUS_DIV)       /* max 50MHz */
#define BSP_FLEXBUS_CLOCK   (BSP_CLOCK / BSP_FLEXBUS_DIV)
#define BSP_FLASH_CLOCK     (BSP_CLOCK / BSP_FLASH_DIV)     /* max 25MHz */

#ifdef MCU_MK70F12
enum usbhs_clock
{
  MCGPLL0,
  MCGPLL1,
  MCGFLL,
  PLL1,
  CLKIN
};

// Constants for use in pll_init
#define NO_OSCINIT          0
#define OSCINIT             1

#define OSC_0               0
#define OSC_1               1

#define LOW_POWER           0
#define HIGH_GAIN           1

#define CANNED_OSC          0
#define CRYSTAL             1

#define PLL_0               0
#define PLL_1               1

#define PLL_ONLY            0
#define MCGOUT              1

#define BLPI                1
#define FBI                 2
#define FEI                 3
#define FEE                 4
#define FBE                 5
#define BLPE                6
#define PBE                 7
#define PEE                 8

// IRC defines
#define SLOW_IRC            0
#define FAST_IRC            1

/*
 * Input Clock Info
 */
#define CLK0_FREQ_HZ        50000000
#define CLK0_TYPE           CANNED_OSC

#define CLK1_FREQ_HZ        12000000
#define CLK1_TYPE           CRYSTAL

/* Select Clock source */
/* USBHS Fractional Divider value for 120MHz input */
/* USBHS Clock = PLL0 x (USBHSFRAC+1) / (USBHSDIV+1)       */
#define USBHS_FRAC          0
#define USBHS_DIV           SIM_CLKDIV2_USBHSDIV(1)
#define USBHS_CLOCK         MCGPLL0


/* USB Fractional Divider value for 120MHz input */
/** USB Clock = PLL0 x (FRAC +1) / (DIV+1)       */
/** USB Clock = 120MHz x (1+1) / (4+1) = 48 MHz    */
#define USB_FRAC            SIM_CLKDIV2_USBFSFRAC_MASK
#define USB_DIV             SIM_CLKDIV2_USBFSDIV(4)


/* Select Clock source */
#define USB_CLOCK           MCGPLL0
//#define USB_CLOCK         MCGPLL1
//#define USB_CLOCK         MCGFLL
//#define USB_CLOCK         PLL1
//#define USB_CLOCK         CLKIN

/* The expected PLL output frequency is:
 * PLL out = (((CLKIN/PRDIV) x VDIV) / 2)
 * where the CLKIN can be either CLK0_FREQ_HZ or CLK1_FREQ_HZ.
 * 
 * For more info on PLL initialization refer to the mcg driver files.
 */

#define PLL0_PRDIV          5
#define PLL0_VDIV           24

#define PLL1_PRDIV          5
#define PLL1_VDIV           30
#endif

/*****************************************************************************
 * Local Variables
 *****************************************************************************/
#ifdef MCU_MK70F12
	int mcg_clk_hz;
	int mcg_clk_khz;
	int core_clk_khz;
	int periph_clk_khz;
	int pll_0_clk_khz;
	int pll_1_clk_khz;
#endif

/*****************************************************************************
 * Local Functions Prototypes
 *****************************************************************************/
static void SYS_Init(void);
static void GPIO_Init();
static void USB_Init(uint_8 controller_ID);
static int pll_init(
	#ifdef MCU_MK70F12
		unsigned char init_osc, 
		unsigned char osc_select, 
		int crystal_val, 
		unsigned char hgo_val, 
		unsigned char erefs_val, 
		unsigned char pll_select, 
		signed char prdiv_val, 
		signed char vdiv_val, 
		unsigned char mcgout_select
	#endif
		);
#ifdef MCU_MK70F12
	void trace_clk_init(void);
	void fb_clk_init(void);
#endif
    
#if WATCHDOG_DISABLE
static void wdog_disable(void);
#endif /* WATCHDOG_DISABLE */

/****************************************************************************
 * Global Variables
 ****************************************************************************/
#if ((defined __CWCC__) || (defined __IAR_SYSTEMS_ICC__) || (defined __CC_ARM)|| (defined __arm__))
	extern uint_32 ___VECTOR_RAM[];            // Get vector table that was copied to RAM
#elif defined(__GNUC__)
	extern uint_32 __cs3_interrupt_vector[];
#endif
volatile uint_8 kbi_stat;	   /* Status of the Key Pressed */
#ifdef USE_FEEDBACK_ENDPOINT
	extern uint_32 feedback_data;
#endif

/*****************************************************************************
 * Global Functions
 *****************************************************************************/
/******************************************************************************
 * @name        main
 *
 * @brief       This routine is the starting point of the application
 *
 * @param       None
 *
 * @return      None
 *
 *****************************************************************************
 * This function initializes the system, enables the interrupts and calls the
 * application
 *****************************************************************************/
void main(void)
{
	/* Initialize the system */
    SYS_Init();

    USB_Init(MAX3353);

    /* Initialize GPIO pins */
    GPIO_Init();

    (void)TimerQInitialize(0);
    
    /* Initialize the USB Test Application */
    TestApp_Init();

    while(TRUE)
    {
    	Watchdog_Reset();
    	/* Call the application task */
    	TestApp_Task();
    }
}

/*****************************************************************************
 * Local Functions
 *****************************************************************************/
/*****************************************************************************
 *
 *    @name     GPIO_Init
 *
 *    @brief    This function Initializes LED GPIO
 *
 *    @param    None
 *
 *    @return   None
 *
 ****************************************************************************
 * Intializes GPIO pins
 ***************************************************************************/
static void GPIO_Init()
{   
#if(defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7) 
	#if(!(defined MCU_MK20D5)) || (!(defined _USB_BATT_CHG_APP_H_))
		/* Enable clock gating to PORTC */
		SIM_SCGC5 |= SIM_SCGC5_PORTC_MASK;
		
		/* LEDs settings */
		PORTC_PCR9 = PORT_PCR_MUX(1);
		PORTC_PCR9|= PORT_PCR_SRE_MASK    /* Slow slew rate */
				  |  PORT_PCR_ODE_MASK    /* Open Drain Enable */
				  |  PORT_PCR_DSE_MASK    /* High drive strength */
				  ;
		
		PORTC_PCR10 = PORT_PCR_MUX(1);
		PORTC_PCR10|= PORT_PCR_SRE_MASK    /* Slow slew rate */
				  |  PORT_PCR_ODE_MASK    /* Open Drain Enable */
				  |  PORT_PCR_DSE_MASK    /* High drive strength */
				  ;
		
		GPIOC_PSOR = 1 << 9 | 1 << 10;
		GPIOC_PDDR |= 1 << 9 | 1 << 10;
		
		/* Switch buttons settings */    	
		/* Set input PORTC1 */
		PORTC_PCR1 = PORT_PCR_MUX(1);
		GPIOC_PDDR &= ~((uint_32)1 << 1);
		/* Pull up */
		PORTC_PCR1 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
		/* GPIO_INT_EDGE_FALLING */
		PORTC_PCR1 |= PORT_PCR_IRQC(0xA);	
		/* Set input PORTC2 */
		PORTC_PCR2 =  PORT_PCR_MUX(1);
		GPIOC_PDDR &= ~((uint_32)1 << 2);
		/* Pull up */
		PORTC_PCR2 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
		/* GPIO_INT_EDGE_FALLING */
		PORTC_PCR2 |= PORT_PCR_IRQC(0xA);
		/* Enable interrupt */
		PORTC_ISFR = (1 << 1);
		PORTC_ISFR = (1 << 2);
		#ifdef MCU_MK20D5
			NVICICPR1 = 1 << ((42)%32);
			NVICISER1 = 1 << ((42)%32);	
		#else
			NVICICPR2 = (uint32_t)(1 << ((89)%32));		/* Clear any pending interrupts on PORTC */
			NVICISER2 = (uint32_t)(1 << ((89)%32));		/* Enable interrupts from PORTC module */ 
		#endif
	#endif
#endif
#if (defined MCU_MK40N512VMD100) ||  (defined MCU_MK53N512CMD100)
    /* Enable clock gating to PORTC */
    SIM_SCGC5 |= SIM_SCGC5_PORTC_MASK;	
	
    /* LEDs settings */
    PORTC_PCR7 = PORT_PCR_MUX(1);
    PORTC_PCR7|= PORT_PCR_SRE_MASK    /* Slow slew rate */
              |  PORT_PCR_ODE_MASK    /* Open Drain Enable */
              |  PORT_PCR_DSE_MASK    /* High drive strength */
              ;
    
    PORTC_PCR8 = PORT_PCR_MUX(1);
    PORTC_PCR8|= PORT_PCR_SRE_MASK    /* Slow slew rate */
              |  PORT_PCR_ODE_MASK    /* Open Drain Enable */
              |  PORT_PCR_DSE_MASK    /* High drive strength */
              ;
    
    GPIOC_PSOR = 1 << 7 | 1 << 8;
    GPIOC_PDDR |= 1 << 7 | 1 << 8;
    
	/* Switch buttons settings */
	/* Set in put PORTC5 */
	PORTC_PCR5 =  PORT_PCR_MUX(1);
	GPIOC_PDDR &= ~((uint_32)1 << 5);
	/* Pull up */
	PORTC_PCR5 |= PORT_PCR_PE_MASK|PORT_PCR_PS_MASK;
	/* GPIO_INT_EDGE_HIGH */
	PORTC_PCR5 |= PORT_PCR_IRQC(9);	
	/* Set in put PORTC13*/
	PORTC_PCR13 =  PORT_PCR_MUX(1);
	GPIOC_PDDR &= ~((uint_32)1 << 13);
	/* Pull up */
	PORTC_PCR13 |= PORT_PCR_PE_MASK|PORT_PCR_PS_MASK;
	/* GPIO_INT_EDGE_HIGH */
	PORTC_PCR13 |= PORT_PCR_IRQC(9);
	/* Enable interrupt */
	PORTC_ISFR = (1 << 5);
	PORTC_ISFR = (1 << 13);
	NVICICPR2 = 1 << ((89)%32);
	NVICISER2 = 1 << ((89)%32);
	
	PORTC_PCR8 =  PORT_PCR_MUX(1);
	GPIOC_PDDR |= 1<<8;
#endif	
#if defined(MCU_MK60N512VMD100)  
	/* Enable clock gating to PORTA and PORTE */
	SIM_SCGC5 |= (SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTE_MASK);
	
	/* LEDs settings */
	PORTA_PCR10 =  PORT_PCR_MUX(1);
	PORTA_PCR11 =  PORT_PCR_MUX(1);
	PORTA_PCR28 =  PORT_PCR_MUX(1);
	PORTA_PCR29 =  PORT_PCR_MUX(1);
	
	GPIOA_PDDR |= (1<<10);
	GPIOA_PDDR |= (1<<11);
	GPIOA_PDDR |= (1<<28);
	GPIOA_PDDR |= (1<<29);
	
	/* Switch buttons settings */
	/* Set input on PORTA pin 19 */
#ifndef _USB_BATT_CHG_APP_H_ 	
	PORTA_PCR19 =  PORT_PCR_MUX(1);
	GPIOA_PDDR &= ~((uint_32)1 << 19);	
	/* Pull up enabled */
	PORTA_PCR19 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;	
	/* GPIO_INT_EDGE_HIGH */
	PORTA_PCR19 |= PORT_PCR_IRQC(9);	
	
	/* Set input on PORTE pin 26 */
	PORTE_PCR26 =  PORT_PCR_MUX(1);
	GPIOE_PDDR &= ~((uint_32)1 << 26);	
	/* Pull up */
	PORTE_PCR26 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;	
	/* GPIO_INT_EDGE_HIGH */
	PORTE_PCR26 |= PORT_PCR_IRQC(9);
#endif
	
	/* Clear interrupt flag */
#ifndef _USB_BATT_CHG_APP_H_ 
	PORTA_ISFR = (1 << 19);
#endif
	PORTE_ISFR = (1 << 26);
	
	/* Enable interrupt port A */
#ifndef _USB_BATT_CHG_APP_H_		
	NVICICPR2 = 1 << ((87)%32);
	NVICISER2 = 1 << ((87)%32);	
#endif	
	
	/* Enable interrupt port E */	
	NVICICPR2 = 1 << ((91)%32);
	NVICISER2 = 1 << ((91)%32);	
#endif	
#ifdef MCU_MK70F12
	/* Enable clock gating to PORTA, PORTD and PORTE */
	SIM_SCGC5 |= 0 | 
			//SIM_SCGC5_PORTA_MASK | 
			SIM_SCGC5_PORTD_MASK | 
			SIM_SCGC5_PORTE_MASK;
	
	/* Set function to GPIO */
	//PORTA_PCR10 = PORT_PCR_MUX(1);
	PORTD_PCR0 = PORT_PCR_MUX(1);
	PORTE_PCR26 = PORT_PCR_MUX(1);
	
	/* Set pin direction to in */
	GPIOD_PDDR &= ~(1<<0);
	GPIOE_PDDR &= ~(1<<26);
	
	/* Set pin direction to out */
	//GPIOA_PDDR |= (1<<10);
	
	/* Enable pull up */
	PORTD_PCR0 |= PORT_PCR_PE_MASK|PORT_PCR_PS_MASK;
	PORTE_PCR26 |= PORT_PCR_PE_MASK|PORT_PCR_PS_MASK;

	/* GPIO_INT_EDGE_HIGH */
	PORTD_PCR0 |= PORT_PCR_IRQC(9);
	PORTE_PCR26 |= PORT_PCR_IRQC(9);

	/* Clear interrupt flag */
	PORTD_ISFR = (1 << 0);
	PORTE_ISFR = (1 << 26);

	/* enable interrupt port D */
	NVICICPR2 = 1 << ((90)%32);
	NVICISER2 = 1 << ((90)%32);

	/* enable interrupt port E */
	NVICICPR2 = 1 << ((91)%32);
	NVICISER2 = 1 << ((91)%32);
#endif	// MCU_MK70F12
#if defined(MCU_MK21D5)  
    /* Enable clock gating to PORTC and PORTD */
    SIM_SCGC5 |= (SIM_SCGC5_PORTC_MASK | SIM_SCGC5_PORTD_MASK | SIM_SCGC5_PORTE_MASK);

    /* LEDs settings */
    PORTD_PCR4 =  PORT_PCR_MUX(1);
    PORTD_PCR5 =  PORT_PCR_MUX(1);
    PORTD_PCR6 =  PORT_PCR_MUX(1);
    PORTD_PCR7 =  PORT_PCR_MUX(1);

    GPIOD_PDDR |= (1<<4) | (1<<5) | (1<<6) | (1<<7);

    /* Switch buttons settings */
    /* Set input on PORTC pin 6 */
#ifndef _USB_BATT_CHG_APP_H_
    PORTC_PCR6 =  PORT_PCR_MUX(1);
    GPIOC_PDDR &= ~((uint_32)1 << 6);
    /* Pull up enabled */
    PORTC_PCR6 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    /* GPIO_INT_EDGE_HIGH */
    PORTC_PCR6 |= PORT_PCR_IRQC(9);

    /* Set input on PORTC pin 7 */
    PORTC_PCR7 =  PORT_PCR_MUX(1);
    GPIOC_PDDR &= ~((uint_32)1 << 7);
    /* Pull up */
    PORTC_PCR7 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    /* GPIO_INT_EDGE_HIGH */
    PORTC_PCR7 |= PORT_PCR_IRQC(9);

    /* Clear interrupt flag */
    PORTC_ISFR = (1 << 6);
    PORTC_ISFR = (1 << 7);

    /* Enable interrupt port C */
    NVICICPR1 = 1 << ((61)%32);
    NVICISER1 = 1 << ((61)%32);
#endif
#endif
#if defined(MCU_MKL25Z4)  
    /* Enable clock gating to PORTA, PORTB, PORTC, PORTD and PORTE */
    SIM_SCGC5 |= (SIM_SCGC5_PORTA_MASK 
              | SIM_SCGC5_PORTB_MASK 
              | SIM_SCGC5_PORTC_MASK 
              | SIM_SCGC5_PORTD_MASK 
              | SIM_SCGC5_PORTE_MASK);

    /* LEDs settings */
    PORTA_PCR5  =  PORT_PCR_MUX(1);
    PORTA_PCR16 =  PORT_PCR_MUX(1);
    PORTA_PCR17 =  PORT_PCR_MUX(1);
    PORTB_PCR8  =  PORT_PCR_MUX(1);

    GPIOA_PDDR |= (1<<5) | (1<<16) | (1<<17);
    GPIOB_PDDR |= (1<<8);

    /* Switch buttons settings */
    /* Set input on PORTC pin 3 */
#ifndef _USB_BATT_CHG_APP_H_
    PORTC_PCR3 =  PORT_PCR_MUX(1);
    GPIOC_PDDR &= ~((uint_32)1 << 3);
    /* Pull up enabled */
    PORTC_PCR3 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    /* GPIO_INT_EDGE_HIGH */
    PORTC_PCR3 |= PORT_PCR_IRQC(9);

    /* Set input on PORTA pin 4 */
    PORTA_PCR4 =  PORT_PCR_MUX(1);
    GPIOA_PDDR &= ~((uint_32)1 << 4);
    /* Pull up */
    PORTA_PCR4 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    /* GPIO_INT_EDGE_HIGH */
    PORTA_PCR4 |= PORT_PCR_IRQC(9);

    /* Clear interrupt flag */
    PORTC_ISFR = (1 << 3);
    PORTA_ISFR = (1 << 4);

    /* Enable interrupt port A */
    NVIC_ICPR = 1 << 30;
    NVIC_ISER = 1 << 30;
#endif
#endif
}

/*****************************************************************************
 *
 *    @name     USB_Init
 *
 *    @brief    This function Initializes USB module
 *
 *    @param    None
 *
 *    @return   None
 *
 ***************************************************************************/
static void USB_Init(uint_8 controller_ID)
{
	SIM_CLKDIV2 &= (uint32_t)(~(SIM_CLKDIV2_USBFRAC_MASK | SIM_CLKDIV2_USBDIV_MASK));
	/* Configure USBFRAC = 0, USBDIV = 0 => frq(USBout) = 1 / 1 * frq(PLLin) */
	SIM_CLKDIV2 = SIM_CLKDIV2_USBDIV(0);

	/* 1. Configure USB to be clocked from PLL */
	SIM_SOPT2 |= SIM_SOPT2_USBSRC_MASK | SIM_SOPT2_PLLFLLSEL_MASK;

#if PLL_96
	/* 2. USB freq divider */
	SIM_CLKDIV2 = 0x02;
#endif /* PLL_96 */

	/* 3. Enable USB-OTG IP clocking */
	SIM_SCGC4 |= (SIM_SCGC4_USBOTG_MASK);      

	/* old documentation writes setting this bit is mandatory */
	USB0_USBTRC0 = 0x40;

	/* Configure enable USB regulator for device */
	SIM_SOPT1 |= SIM_SOPT1_USBREGEN_MASK;

	NVICICPR2 = (1 << 9);	/* Clear any pending interrupts on USB */
	NVICISER2 = (1 << 9);	/* Enable interrupts from USB module */             

}

/******************************************************************************
 * @name       all_led_off
 *
 * @brief      Switch OFF all LEDs on Board
 *
 * @param	   None
 *
 * @return     None
 *
 *****************************************************************************
 * This function switch OFF all LEDs on Board
 *****************************************************************************/
void all_led_off(void)
{
	#if defined (MCU_MK40N512VMD100)
		GPIOC_PSOR = 1 << 7 | 1 << 8 | 1 << 9;
	#elif defined (MCU_MK53N512CMD100)
		GPIOC_PSOR = 1 << 7 | 1 << 8;
	#elif defined (MCU_MK60N512VMD100) 
		GPIOA_PSOR = 1 << 10 | 1 << 11 | 1 << 28 | 1 << 29;
	#elif (defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
		GPIOC_PSOR = 1 << 9 | 1 << 10;
    #elif (defined MCU_MK21D5)
        GPIOD_PSOR = 1 << 4 | 1 << 5 | 1 << 6 | 1 << 7;
    #endif
}

/******************************************************************************
 * @name       display_led
 *
 * @brief      Displays 8bit value on Board LEDs
 *
 * @param	   val
 *
 * @return     None
 *
 *****************************************************************************
 * This function displays 8 bit value on Board LED
 *****************************************************************************/
void display_led(uint_8 val)
{
    uint_8 i = 0;
	UNUSED(i);
    all_led_off();

	#if defined (MCU_MK40N512VMD100)
    	val &= 0x07;
        if(val & 0x1)
    		GPIOC_PCOR = 1 << 7;
    	if(val & 0x2)
    		GPIOC_PCOR = 1 << 8;
    	if(val & 0x4)
    		GPIOC_PCOR = 1 << 9; 
	#elif defined (MCU_MK53N512CMD100)
		val &= 0x03;
	    if(val & 0x1)
			GPIOC_PCOR = 1 << 7;
		if(val & 0x2)
			GPIOC_PCOR = 1 << 8;
	#elif defined (MCU_MK60N512VMD100) 
		val &= 0x0F;
	    if(val & 0x1)
			GPIOA_PCOR = 1 << 11;
		if(val & 0x2)
			GPIOA_PCOR = 1 << 28;		
	    if(val & 0x4)
			GPIOA_PCOR = 1 << 29;
		if(val & 0x8)
			GPIOA_PCOR = 1 << 10;		
	#elif (defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
		val &= 0x03; 
	    if(val & 0x1)
			GPIOC_PCOR = 1 << 9;
		if(val & 0x2)
			GPIOC_PCOR = 1 << 10;		
    #elif (defined MCU_MK21D5)
        val &= 0x0F;
        if(val & 0x1)
            GPIOD_PCOR = 1 << 4;
        if(val & 0x2)
            GPIOD_PCOR = 1 << 5;
        if(val & 0x4)
            GPIOD_PCOR = 1 << 6;
        if(val & 0x8)
            GPIOD_PCOR = 1 << 7;
    #endif
}
/*****************************************************************************
 *
 *    @name     Init_Sys
 *
 *    @brief    This function Initializes the system
 *
 *    @param    None
 *
 *    @return   None
 *
 ****************************************************************************
 * Initializes the MCU, MCG, KBI, RTC modules
 ***************************************************************************/
static void SYS_Init(void)
{
	/* Point the VTOR to the new copy of the vector table */
	SCB_VTOR = (uint_32)___VECTOR_RAM;

    /* SIM Configuration */
	pll_init();
	MPU_CESR=0x00;

}

#ifdef MCU_MK70F12
/********************************************************************/
void trace_clk_init(void)
{
	/* Set the trace clock to the core clock frequency */
	SIM_SOPT2 |= SIM_SOPT2_TRACECLKSEL_MASK;

	/* Enable the TRACE_CLKOUT pin function on PTF23 (alt6 function) */
	PORTF_PCR23 = ( PORT_PCR_MUX(0x6));
}

/********************************************************************/
void fb_clk_init(void)
{
	/* Enable the clock to the FlexBus module */
	SIM_SCGC7 |= SIM_SCGC7_FLEXBUS_MASK;

	/* Enable the FB_CLKOUT function on PTC3 (alt5 function) */
	PORTC_PCR3 = ( PORT_PCR_MUX(0x5));
}
#endif // MCU_MK70F12

#if(!(defined MCU_MK21D5))
/******************************************************************************
*   @name        IRQ_ISR_PORTA
*
*   @brief       Service interrupt routine of IRQ
*
*   @return      None
*
*   @comment	 
*    
*******************************************************************************/
void IRQ_ISR_PORTA(void)
{
#if defined(MCU_MK20D5)
    NVICICPR1 = 1 << ((40)%32);
    NVICISER1 = 1 << ((40)%32);
#elif defined (MCU_MKL25Z4)
    NVIC_ICPR = 1 << 30;
    NVIC_ISER = 1 << 30;
#else
    NVICICPR2 = 1 << ((87)%32);
    NVICISER2 = 1 << ((87)%32);
#endif
	DisableInterrupts;
#if defined MCU_MKL25Z4
    if(PORTA_ISFR & (1<<4))
    {
        kbi_stat |= 0x02;                 /* Update the kbi state */
        PORTA_ISFR = (1 << 4);            /* Clear the bit by writing a 1 to it */
    }
#else
	if(PORTA_ISFR & (1<<19))
	{
		kbi_stat |= 0x02; 				/* Update the kbi state */
		PORTA_ISFR = (1 << 19);			/* Clear the bit by writing a 1 to it */
	}
#endif
	EnableInterrupts;
}
#endif
#if (!defined MCU_MKL25Z4)&&((!(defined MCU_MK20D5)) || (!(defined _USB_BATT_CHG_APP_H_)))
/******************************************************************************
*   @name        IRQ_ISR
*
*   @brief       Service interrupt routine of IRQ
*
*   @return      None
*
*   @comment	 
*    
*******************************************************************************/
void IRQ_ISR_PORTC(void)
	{
		#if defined(MCU_MK20D5)
			NVICICPR1 = (uint32_t)(1 << ((42)%32));
			NVICISER1 = (uint32_t)(1 << ((42)%32));		
        #elif defined(MCU_MK21D5)
            NVICICPR1 = 1 << ((61)%32);
            NVICISER1 = 1 << ((61)%32);
        #else
			NVICICPR2 = (uint32_t)(1 << ((89)%32));		/* Clear any pending interrupt on PORTC */
			NVICISER2 = (uint32_t)(1 << ((89)%32));		/* Set interrupt on PORTC */
		#endif
			
		DisableInterrupts;
	#if(defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
		if(PORTC_ISFR & (1<<1))
    #elif defined(MCU_MK21D5)
        if(PORTC_ISFR & (1<<6))
	#else
		if(PORTC_ISFR & (1<<5))
	#endif	
		{
			kbi_stat |= 0x02; 				/* Update the kbi state */
			
			/* Clear the bit by writing a 1 to it */
		#if(defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
			PORTC_ISFR = (1 << 1);
        #elif defined(MCU_MK21D5)
            PORTC_ISFR = (1 << 6);
		#else
			PORTC_ISFR = (1 << 5);
		#endif							
		}
		
	#if(defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
		if(PORTC_ISFR & (1<<2))
    #elif defined(MCU_MK21D5)
        if(PORTC_ISFR & (1<<7))
	#else
		if(PORTC_ISFR & (1<<13))
	#endif	
		{
			kbi_stat |= 0x08;				/* Update the kbi state */
			
			/* Clear the bit by writing a 1 to it */
			#if(defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
				PORTC_ISFR = (1 << 2);
            #elif defined(MCU_MK21D5)
                PORTC_ISFR = (1 << 7);
			#else
				PORTC_ISFR = (1 << 13);
			#endif				
		}	
		EnableInterrupts;
	}
	#endif
//#endif

#if((!defined MCU_MK21D5)&&(!defined MCU_MKL25Z4))
/******************************************************************************
*   @name        IRQ_ISR
*
*   @brief       Service interrupt routine of IRQ
*
*   @return      None
*
*   @comment	 
*    
*******************************************************************************/
void IRQ_ISR_PORTD(void)
{
	#ifdef MCU_MK20D5
		NVICICPR1 = 1 << ((43)%32);
		NVICISER1 = 1 << ((43)%32);	
	#else
		NVICICPR2 = 1 << ((90)%32);
		NVICISER2 = 1 << ((90)%32);
	#endif	
	
	DisableInterrupts;
	if(PORTD_ISFR & (1<<0))
	{		
		/* Update the kbi state */
		kbi_stat |= 0x02;	// right click
		PORTD_ISFR = (1 << 0);			/* Clear the bit by writing a 1 to it */
	}
	EnableInterrupts;
}
#endif

/******************************************************************************
*   @name        IRQ_ISR_PORTE
*
*   @brief       Service interrupt routine of IRQ
*
*   @return      None
*
*   @comment	 
*    
*******************************************************************************/
#if((!defined MCU_MK21D5)&&(!defined MCU_MKL25Z4))
void IRQ_ISR_PORTE(void)
{
	#ifdef MCU_MK20D5
		NVICICPR1 = 1 << ((44)%32);
		NVICISER1 = 1 << ((44)%32);	
	#else	
		NVICICPR2 = 1 << ((91)%32);
		NVICISER2 = 1 << ((91)%32);	
	#endif
	DisableInterrupts;
	if(PORTE_ISFR & (1<<26))
	{
		/* Update the kbi state */
#ifdef MCU_MK70F12
		kbi_stat |= 0x01;	// left click
#else
		kbi_stat |= 0x08;	// move pointer down
#endif
		PORTE_ISFR = (1 << 26);			/* Clear the bit by writing a 1 to it */
	}
	EnableInterrupts;
}
#endif
/******************************************************************************
*   @name        IRQ_ISR_PORTF
*
*   @brief       Service interrupt routine of IRQ
*
*   @return      None
*
*   @comment	 
*    
*******************************************************************************/
#ifdef MCU_MK70F12
void IRQ_ISR_PORTF(void)
{
}
#endif

#if WATCHDOG_DISABLE
/*****************************************************************************
 * @name     wdog_disable
 *
 * @brief:   Disable watchdog.
 *
 * @param  : None
 *
 * @return : None
 *****************************************************************************
 * It will disable watchdog.
  ****************************************************************************/
static void wdog_disable(void)
{
#ifdef MCU_MKL25Z4
    SIM_COPC = 0x00;
#else
	/* Write 0xC520 to the unlock register */
	WDOG_UNLOCK = 0xC520;
	
	/* Followed by 0xD928 to complete the unlock */
	WDOG_UNLOCK = 0xD928;
	
	/* Clear the WDOGEN bit to disable the watchdog */
	WDOG_STCTRLH &= ~WDOG_STCTRLH_WDOGEN_MASK;
#endif
}
#endif /* WATCHDOG_DISABLE */


/* Addition from AN4503 */
/***************************************************************/
/*
 * Configures the ARM system control register for WAIT(sleep)mode
 * and then executes the WFI instruction to enter the mode.
 *
 * Parameters:
 * none
 *
 */
void sleep( void ) {
	/* Clear the SLEEPDEEP bit to make sure we go into WAIT (sleep)
	 * mode instead of deep sleep.
	 */
	SCB_SCR &= ~SCB_SCR_SLEEPDEEP_MASK;
#ifdef CMSIS
	__wfi();
#else
	/* WFI instruction will start entry into WAIT mode */
	asm("WFI");
#endif
}
/***************************************************************/

/***************************************************************/
/*
 * Configures the ARM system control register for STOP
 * (deepsleep) mode and then executes the WFI instruction
 * to enter the mode.
 *
 * Parameters:
 * none
 *
 */
void deepsleep( void ) {
	/* Set the SLEEPDEEP bit to enable deep sleep mode (STOP) */
	SCB_SCR |= SCB_SCR_SLEEPDEEP_MASK;
#ifdef CMSIS
	__wfi();
#else
	/* WFI instruction will start entry into STOP mode */
	asm("WFI");
#endif
}
/***************************************************************/

/***************************************************************/
/* WAIT mode entry routine. Puts the processor into Wait mode.
 * In this mode the core clock is disabled (no code executing),
 * but bus clocks are enabled (peripheral modules are
 * operational)
 *
 * Mode transitions:
 * RUN to WAIT
 * VLPR to VLPW
 *
 * This function can be used to enter normal wait mode or VLPW
 * mode. If you are executing in normal run mode when calling
 * this function, then you will enter normal wait mode.
 * If you are in VLPR mode when calling this function,
 * then you will enter VLPW mode instead.
 *
 * NOTE: Some modules include a programmable option to disable
 * them in wait mode. If those modules are programmed to disable
 * in wait mode, they will not be able to generate interrupts to
 * wake the core.
 *
 * WAIT mode is exited using any enabled interrupt or RESET,
 * so no exit_wait routine is needed.
 * For Kinetis K:
 * If in VLPW mode, the statue of the SMC_PMCTRL[LPWUI] bit
 * determines if the processor exits to VLPR (LPWUI cleared)
 * or normal run mode (LPWUI set). The enable_lpwui()
 * and disable_lpwui()functions can be used to set this bit
 * to the desired option prior to calling enter_wait().
 * For Kinetis L:
 * LPWUI does not exist.
 * Exits with an interrupt from VLPW will always be back to VLPR.
 * Exits from an interrupt from Wait will always be back to Run.
 *
 * Parameters:
 * none
 */
void enter_wait(void)
{
	sleep();
}
/***************************************************************/

/***************************************************************/
/* STOP mode entry routine.
 * if in Run mode puts the processor into normal stop mode.
 * If in VLPR mode puts the processor into VLPS mode.
 * In this mode core, bus and peripheral clocks are disabled.
 *
 * Mode transitions:
 * RUN to STOP
 * VLPR to VLPS
 *
 * This function can be used to enter normal stop mode.
 * If you are executing in normal run mode when calling this
 * function and AVLP = 0, then you will enter normal stop mode.
 * If AVLP = 1 with previous write to PMPROT
 * then you will enter VLPS mode instead.
 *
 * STOP mode is exited using any enabled interrupt or RESET,
 * so no exit_stop routine is needed.
 *
 * Parameters:
 * none
 */
void enter_stop( void ) {
	volatile unsigned int dummyread;

	/*The PMPROT register may have already been written by init
	 code. If so, then this next write is not done since
	 PMPROT is write once after RESET
	 this write-once bit allows the MCU to enter the
	 normal STOP mode.
	 If AVLP is already a 1, VLPS mode is entered
	 instead of normal STOP
	 is SMC_PMPROT = 0 */

	/* Set the STOPM field to 0b000 for normal STOP mode
	 For Kinetis L: if trying to enter Stop from VLPR user
	 forced to VLPS low power mode */
#if (defined MCU_MK40N512VMD100) || (defined MCU_MK53N512CMD100) || (defined MCU_MK60N512VMD100)		
	MC_PMCTRL &= ~MC_PMCTRL_LPLLSM_MASK;
	MC_PMCTRL |= MC_PMCTRL_LPLLSM(0);
	/*wait for write to complete to SMC before stopping core */
	dummyread = MC_PMCTRL;
#else
	SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK;
	SMC_PMCTRL |= SMC_PMCTRL_STOPM(0);
	/*wait for write to complete to SMC before stopping core */
	dummyread = SMC_PMCTRL;
#endif	
	deepsleep();
}
/***************************************************************/

/***************************************************************/
/* VLPR mode entry routine.Puts the processor into Very Low Power
 * Run Mode. In this mode, all clocks are enabled,
 * but the core, bus, and peripheral clocks are limited
 * to 2 or 4 MHz or less.
 * The flash clock is limited to 1MHz or less.
 *
 * Mode transitions:
 * RUN to VLPR
 *
 * For Kinetis K:
 * While in VLPR, VLPW or VLPS the exit to VLPR is determined by
 * the value passed in from the calling program.
 * LPWUI is static during VLPR mode and
 * should not be written to while in VLPR mode.
 *
 * For Kinetis L:
 * LPWUI does not exist. the parameter pass is a don't care
 * Exits with an interrupt from VLPW will always be back to VLPR.
 * Exits from an interrupt from Wait will always be back to Run.
 *
 * Parameters:
 * lpwui_value - The input determines what is written to the
 * LPWUI bit in the PMCTRL register
 * Clear LPWUI and interrupts keep you in VLPR
 * Set LPWUI and interrupts return you to Run mode
 * Return value : PMSTAT value or error code
 * PMSTAT = 000_0100 Current power mode is VLPR
 * ERROR Code = 0x14 - already in VLPR mode
 * = 0x24 - REGONS never clears
 * indicating stop regulation
 */
int enter_vlpr(char lpwui_value)
{
	int i;
#if (!defined MCU_MK40N512VMD100) && (!defined MCU_MK53N512CMD100) && (!defined MCU_MK60N512VMD100)	
	if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 4){
		return 0x14;
	}
#endif	
	/* The PMPROT register may have been written by init code
If so, then this next write is not done
PMPROT is write once after RESET
This write-once bit allows the MCU to enter the
very low power modes: VLPR, VLPW, and VLPS */
#if (defined MCU_MK40N512VMD100) || (defined MCU_MK53N512CMD100) || (defined MCU_MK60N512VMD100) 
	MC_PMPROT = MC_PMPROT_AVLP_MASK;
#else
	SMC_PMPROT = SMC_PMPROT_AVLP_MASK;
#endif
	/* Set the (for MC1)LPLLSM or (for MC2)STOPM field
to 0b010 for VLPS mode -
and RUNM bits to 0b010 for VLPR mode
Need to set state of LPWUI bit */
	/* Check if memory map has not been already included */
	lpwui_value &= 1;
#if (defined MCU_MK40N512VMD100) || (defined MCU_MK53N512CMD100) || (defined MCU_MK60N512VMD100) 
	MC_PMCTRL &= ~MC_PMCTRL_RUNM_MASK;
	MC_PMCTRL |= MC_PMCTRL_RUNM(0x2) |
			(lpwui_value << MC_PMCTRL_LPWUI_SHIFT);		
#elif (defined MCU_MKL25Z4)	
	SMC_PMCTRL &= ~SMC_PMCTRL_RUNM_MASK;
	SMC_PMCTRL |= SMC_PMCTRL_RUNM(0x2);
#else
	SMC_PMCTRL &= ~SMC_PMCTRL_RUNM_MASK;
	SMC_PMCTRL |= SMC_PMCTRL_RUNM(0x2) |
			(lpwui_value << SMC_PMCTRL_LPWUI_SHIFT);	
#endif

	/* OPTIONAL Wait for VLPS regulator mode to be confirmed */
	for (i = 0 ; i < 10 ; i++)
	{ /* Check that the value of REGONS bit is not 0
When it is a zero, you can stop checking */
		if ((PMC_REGSC & PMC_REGSC_REGONS_MASK) ==0x04){
			/* 0 Regulator is in stop regulation or in transition
to/from it
1 MCU is in Run regulation mode */
		}
		else break;
	}
	if ((PMC_REGSC & PMC_REGSC_REGONS_MASK) ==0x04) {
		return 0x24;
	}
	/* SMC_PMSTAT register only exist in Mode Controller 2 */	
#if (!defined MCU_MK40N512VMD100) && (!defined MCU_MK53N512CMD100) && (!defined MCU_MK60N512VMD100)		
	if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK) == 4) {
		return 0x04;
	}
#endif	
}
/***************************************************************/

/***************************************************************/
/* VLPS mode entry routine. Puts the processor into VLPS mode
 * directly from run or VLPR modes.
 *
 * Mode transitions:
 * RUN to VLPS
 * VLPR to VLPS
 *
 * Kinetis K:
 * * when VLPS is entered directly from RUN mode,
 * exit to VLPR is disabled by hardware and the system will
 * always exit back to RUN.
 *
 * If however VLPS mode is entered from VLPR the state of
 * the LPWUI bit determines the state the MCU will return
 * to upon exit from VLPS.If LPWUI is 1 and an interrupt
 * occurs you will exit to normal run mode instead of VLPR.
 * If LPWUI is 0 and an interrupt occurs you will exit to VLPR.
 *
 * For Kinetis L:
 * when VLPS is entered from run an interrupt will exit to run.
 * When VLPS is entered from VLPR an interrupt will exit to VLPS
 * Parameters:
 * none
 */
/***************************************************************/
void enter_vlps(void)
{
	volatile unsigned int dummyread;
	/*The PMPROT register may have already been written by init
code. If so then this next write is not done since
PMPROT is write once after RESET.
This Write allows the MCU to enter the VLPR, VLPW,
and VLPS modes. If AVLP is already written to 0
Stop is entered instead of VLPS*/
#if (defined MCU_MK40N512VMD100) || (defined MCU_MK53N512CMD100) || (defined MCU_MK60N512VMD100) 	
	MC_PMPROT = MC_PMPROT_AVLP_MASK;
	/* Set the LPLLSM field to 0b010 for VLPS mode */
	MC_PMCTRL &= ~MC_PMCTRL_LPLLSM_MASK;
	MC_PMCTRL |= MC_PMCTRL_LPLLSM(0x2);
	/*wait for write to complete to MC before stopping core */
	dummyread = MC_PMCTRL;
#else
	SMC_PMPROT = SMC_PMPROT_AVLP_MASK;
	/* Set the STOPM field to 0b010 for VLPS mode */
	SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK;
	SMC_PMCTRL |= SMC_PMCTRL_STOPM(0x2);
	/*wait for write to complete to SMC before stopping core */
	dummyread = SMC_PMCTRL;	
#endif	
	/* Now execute the stop instruction to go into VLPS */
	deepsleep();
}
/****************************************************************/

/***************************************************************/
/* LLS mode entry routine. Puts the processor into LLS mode from
 * normal Run mode or VLPR.
 *
 * Mode transitions:
 * RUN to LLS
 * VLPR to LLS
 *
 * NOTE: LLS mode will always exit to Run mode even if you were
 * * in VLPR mode before entering LLS.
 *
 * Wake-up from LLS mode is controlled by the LLWU module. Most
 * modules cannot issue a wake-up interrupt in LLS mode, so make
 * sure to set up the desired wake-up sources in the LLWU before
 * calling this function.
 *
 * Parameters:
 * none
 */
void enter_lls(void)
{
	volatile unsigned int dummyread;
#if (defined MCU_MK40N512VMD100) || (defined MCU_MK53N512CMD100) || (defined MCU_MK60N512VMD100)	
	/* Write to PMPROT to allow LLS power modes this write-once
bit allows the MCU to enter the LLS low power mode*/
	MC_PMPROT = MC_PMPROT_ALLS_MASK;
	/* Set the (for MC1) LPLLSM or
(for MC2)LPLLSM field to 0b011 for LLS mode
Retains LPWUI and RUNM values */
	MC_PMCTRL &= ~MC_PMCTRL_LPLLSM_MASK ;
	MC_PMCTRL |= MC_PMCTRL_LPLLSM(0x3) ;
	/*wait for write to complete to SMC before stopping core */
	dummyread = MC_PMCTRL;
	/* Now execute the stop instruction to go into LLS */
#else
	/* Write to PMPROT to allow LLS power modes this write-once
bit allows the MCU to enter the LLS low power mode*/
	SMC_PMPROT = SMC_PMPROT_ALLS_MASK;
	/* Set the (for MC1) LPLLSM or
(for MC2)STOPM field to 0b011 for LLS mode
Retains LPWUI and RUNM values */
	SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK ;
	SMC_PMCTRL |= SMC_PMCTRL_STOPM(0x3) ;
	/*wait for write to complete to SMC before stopping core */
	dummyread = SMC_PMCTRL;
	/* Now execute the stop instruction to go into LLS */	
#endif	
	deepsleep();
}
/**************************************************************/

/***************************************************************/
/* VLLS3 mode entry routine. Puts the processor into
 * VLLS3 mode from normal Run mode or VLPR.
 *
 * Mode transitions:
 * RUN to VLLS3
 * VLPR to VLLS3
 *
 * NOTE: VLLSx modes will always exit to Run mode even if you were
 * in VLPR mode before entering VLLSx.
 *
 * Wake-up from VLLSx mode is controlled by the LLWU module. Most
 * modules cannot issue a wake-up interrupt in VLLSx mode, so make
 * sure to setup the desired wake-up sources in the LLWU before
 * calling this function.
 *
 * Parameters:
 * none
 */
void enter_vlls3(void)
{
	volatile unsigned int dummyread;
#if (defined MCU_MK40N512VMD100) || (defined MCU_MK53N512CMD100) || (defined MCU_MK60N512VMD100)	
	/* Write to PMPROT to allow VLLS3 power modes */
	MC_PMPROT = MC_PMPROT_AVLLS3_MASK;
	/* Set the VLLSM field to 0b101 */
	MC_PMCTRL &= ~MC_PMCTRL_LPLLSM_MASK ;
	MC_PMCTRL |= MC_PMCTRL_LPLLSM(0x5) ;	
	dummyread = 0x03;	/* VLLS control register compatibility VLLS3 */ 
#elif (defined MCU_MKL25Z4)
	/* Write to PMPROT to allow VLLS3 power modes */
	SMC_PMPROT = SMC_PMPROT_AVLLS_MASK;
	/* Set the VLLSM field to 0b100 for VLLSx(for MC1)
or STOPM field to 0b100 for VLLSx (for MC2)
- Retain state of LPWUI and RUNM */
	SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK ;
	SMC_PMCTRL |= SMC_PMCTRL_STOPM(0x4) ;
	// MC_PMCTRL &= ~MC_PMCTRL_VLLSM_MASK ; //(for MC1)
	// MC_PMCTRL |= MC_PMCTRL_VLLSM(0x4) ; //(for MC1)
	/* set VLLSM = 0b11 in SMC_STOPCTRL (for MC2) */
	SMC_STOPCTRL = SMC_STOPCTRL_VLLSM(3);
	/*wait for write to complete to SMC before stopping core */
	dummyread = SMC_STOPCTRL;	
#else
	/* Write to PMPROT to allow VLLS3 power modes */
	SMC_PMPROT = SMC_PMPROT_AVLLS_MASK;
	/* Set the VLLSM field to 0b100 for VLLSx(for MC1)
or STOPM field to 0b100 for VLLSx (for MC2)
- Retain state of LPWUI and RUNM */
	SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK ;
	SMC_PMCTRL |= SMC_PMCTRL_STOPM(0x4) ;
	// MC_PMCTRL &= ~MC_PMCTRL_VLLSM_MASK ; //(for MC1)
	// MC_PMCTRL |= MC_PMCTRL_VLLSM(0x4) ; //(for MC1)
	/* set VLLSM = 0b11 in SMC_VLLSCTRL (for MC2) */
	SMC_VLLSCTRL = SMC_VLLSCTRL_VLLSM(3);
	/*wait for write to complete to SMC before stopping core */
	dummyread = SMC_VLLSCTRL;		
#endif
	/* Now execute the stop instruction to go into VLLS3 */
	deepsleep();
}
#if (!defined MCU_MKL25Z4)
/***************************************************************/

/***************************************************************/
/* VLLS2 mode entry routine. Puts the processor into
 * VLLS2 mode from normal Run mode or VLPR.
 *
 * Mode transitions:
 * RUN to VLLS2
 * VLPR to VLLS2
 *
 * NOTE: VLLSx modes will always exit to Run mode even
 * if you were in VLPR mode before entering VLLSx.
 *
 * Wake-up from VLLSx mode is controlled by the LLWU module. Most
 * modules cannot issue a wake-up interrupt in VLLSx mode, so make
 * sure to setup the desired wake-up sources in the LLWU before
 * calling this function.
 *
 * Parameters:
 * none
 */
void enter_vlls2(void)
{
	volatile unsigned int dummyread;
#if (defined MCU_MK40N512VMD100) || (defined MCU_MK53N512CMD100) || (defined MCU_MK60N512VMD100)	
	/* Write to PMPROT to allow VLLS2 power modes */
	MC_PMPROT = MC_PMPROT_AVLLS2_MASK;
	/* Set the VLLSM field to 0b110*/
	MC_PMCTRL &= ~MC_PMCTRL_LPLLSM_MASK ;
	MC_PMCTRL |= MC_PMCTRL_LPLLSM(0x6) ;
	dummyread = 0x02;	/* VLLS control register compatibility VLLS2 */ 
#else
	/* Write to PMPROT to allow VLLS2 power modes */
	SMC_PMPROT = SMC_PMPROT_AVLLS_MASK;
	/* Set the VLLSM field to 0b100 for VLLSx(for MC1)
or STOPM field to 0b100 for VLLSx (for MC2)
- Retain state of LPWUI and RUNM */
	SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK ;
	SMC_PMCTRL |= SMC_PMCTRL_STOPM(0x4) ;
	// MC_PMCTRL &= ~MC_PMCTRL_VLLSM_MASK ;
	// MC_PMCTRL |= MC_PMCTRL_VLLSM(0x4) ;
	/* set VLLSM = 0b10 in SMC_VLLSCTRL (for MC2) */
	SMC_VLLSCTRL = SMC_VLLSCTRL_VLLSM(2);
	/*wait for write to complete to SMC before stopping core */
	dummyread = SMC_VLLSCTRL;	
#endif
	/* Now execute the stop instruction to go into VLLS2 */
	deepsleep();
}
#endif
/***************************************************************/

/***************************************************************/
/* VLLS1 mode entry routine. Puts the processor into
 * VLLS1 mode from normal Run mode or VLPR.
 *
 * Mode transitions:
 * RUN to VLLS1
 * VLPR to VLLS1
 *
 * NOTE:VLLSx modes will always exit to Run mode even if you were
 * in VLPR mode before entering VLLSx.
 *
 * Wake-up from VLLSx mode is controlled by the LLWU module. Most
 * modules cannot issue a wake-up interrupt in VLLSx mode, so make
 * sure to setup the desired wake-up sources in the LLWU before
 * calling this function.
 *
 * Parameters:
 * none
 */
void enter_vlls1(void)
{
	volatile unsigned int dummyread;
#if (defined MCU_MK40N512VMD100) || (defined MCU_MK53N512CMD100) || (defined MCU_MK60N512VMD100)	
	/* Write to PMPROT to allow VLLS1 power modes */
	MC_PMPROT = MC_PMPROT_AVLLS1_MASK;
	/* Set the VLLSM field to 0b111*/
	MC_PMCTRL &= ~MC_PMCTRL_LPLLSM_MASK ;
	MC_PMCTRL |= MC_PMCTRL_LPLLSM(0x7) ;
	dummyread = 0x01;	/* VLLS control register compatibility VLLS1 */ 
#elif (defined MCU_MKL25Z4)
	/* Write to PMPROT to allow all possible power modes */
	SMC_PMPROT = SMC_PMPROT_AVLLS_MASK;
	/* Set the VLLSM field to 0b100 for VLLSx(for MC1)
or STOPM field to 0b100 for VLLSx (for MC2)
- Retain state of LPWUI and RUNM */
	SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK ;
	SMC_PMCTRL |= SMC_PMCTRL_STOPM(0x4) ;
	// MC_PMCTRL &= ~MC_PMCTRL_VLLSM_MASK ;
	// MC_PMCTRL |= MC_PMCTRL_VLLSM(0x4) ;
	/* set VLLSM = 0b01 in SMC_STOPCTRL (for MC2) */
	SMC_STOPCTRL = SMC_STOPCTRL_VLLSM(1);
	/*wait for write to complete to SMC before stopping core */
	dummyread = SMC_STOPCTRL;	
#else
	/* Write to PMPROT to allow all possible power modes */
	SMC_PMPROT = SMC_PMPROT_AVLLS_MASK;
	/* Set the VLLSM field to 0b100 for VLLSx(for MC1)
or STOPM field to 0b100 for VLLSx (for MC2)
- Retain state of LPWUI and RUNM */
	SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK ;
	SMC_PMCTRL |= SMC_PMCTRL_STOPM(0x4) ;
	// MC_PMCTRL &= ~MC_PMCTRL_VLLSM_MASK ;
	// MC_PMCTRL |= MC_PMCTRL_VLLSM(0x4) ;
	/* set VLLSM = 0b01 in SMC_VLLSCTRL (for MC2) */
	SMC_VLLSCTRL = SMC_VLLSCTRL_VLLSM(1);
	/*wait for write to complete to SMC before stopping core */
	dummyread = SMC_VLLSCTRL;	
#endif	
	deepsleep();
}
/***************************************************************/

/***************************************************************/
/* VLLS0 mode entry routine. Puts the processor into
 * VLLS0 mode from normal run mode or VLPR.
 *
 * Mode transitions:
 * RUN to VLLS0
 * VLPR to VLLS0
 *
 * NOTE: VLLSx modes will always exit to RUN mode even if you were
 * in VLPR mode before entering VLLSx.
 *
 * Wake-up from VLLSx mode is controlled by the LLWU module. Most
 * modules cannot issue a wake-up interrupt in VLLSx mode, so make
 * sure to setup the desired wake-up sources in the LLWU before
 * calling this function.
 *
 * Parameters:
 * PORPO_value - 0 POR detect circuit is enabled in VLLS0
 * 1 POR detect circuit is disabled in VLLS0
 */
/***************************************************************/
#if (!defined MCU_MK40N512VMD100) && (!defined MCU_MK53N512CMD100) && (!defined MCU_MK60N512VMD100)
void enter_vlls0(unsigned char PORPO_value )
{
	volatile unsigned int dummyread;
#if (defined MCU_MKL25Z4)
	/* Write to PMPROT to allow all possible power modes */
	SMC_PMPROT = SMC_PMPROT_AVLLS_MASK;
	/* Set the VLLSM field to 0b100 for VLLSx(for MC1)
or STOPM field to 0b100 for VLLSx (for MC2)
- Retain state of LPWUI and RUNM */
	SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK ;
	SMC_PMCTRL |= SMC_PMCTRL_STOPM(0x4) ;
	/* set VLLSM = 0b00 */
	SMC_STOPCTRL = SMC_STOPCTRL_VLLSM(0);
	/*wait for write to complete to SMC before stopping core */
	dummyread = SMC_STOPCTRL;	
#else
	/* Write to PMPROT to allow all possible power modes */
	SMC_PMPROT = SMC_PMPROT_AVLLS_MASK;
	/* Set the VLLSM field to 0b100 for VLLSx(for MC1)
or STOPM field to 0b100 for VLLSx (for MC2)
- Retain state of LPWUI and RUNM */
	SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK ;
	SMC_PMCTRL |= SMC_PMCTRL_STOPM(0x4) ;
	/* set VLLSM = 0b00 */
	SMC_VLLSCTRL = SMC_VLLSCTRL_VLLSM(0);
	/*wait for write to complete to SMC before stopping core */
	dummyread = SMC_VLLSCTRL;	
	stop();
#endif			
}
#endif
/***************************************************************/

/* NDB - End addition from AN4503 */

/* Put MCU into Normal STOP mode
 * Wakeup from AWIC will put into RUN mode,
 * but because PLLSTEN=0, MCG will be in PBE mode when STOP is exited,
 * and C1[CLKS] and S[CLKST] will automatically be set to 2'b10
 * 
 * When entering Normal Stop mode from PEE mode and if C5[PLLSTEN0]=0, on exit
the MCG clock mode is forced to PBE mode, the C1[CLKS] and S[CLKST] will be
configured to 2�b10 and S[LOCK0] bit will clear without setting S[LOLS0]. If
C5[PLLSTEN0]=1, the S[LOCK0] bit will not get cleared and on exit the MCG will
continue to run in PEE mode.
 */
void set_MCG_PEE_Mode( void ) {
	
  /* Transition into PEE by setting CLKS to 0
   * CLKS=0, FRDIV=3, IREFS=0, IRCLKEN=0, IREFSTEN=0
   */
  MCG_C1 &= ~MCG_C1_CLKS_MASK;

  /* Wait for clock status bits to update */
  while (((MCG_S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) != 0x3) {};

  	/* Enable the ER clock of oscillators */
  	OSC_CR = OSC_CR_ERCLKEN_MASK; // | OSC_CR_EREFSTEN_MASK; // NDB - External Ref Clock Disabled in STOP
}

/*****************************************************************************
 * @name     pll_init
 *
 * @brief:   Initialization of the MCU.
 *
 * @param  : None
 *
 * @return : None
 *****************************************************************************
 * It will configure the MCU to disable STOP and COP Modules.
 * It also set the MCG configuration and bus clock frequency.
 ****************************************************************************/
static int pll_init()
{
	/* 
	 * First move to FBE mode
	 * Enable external oscillator, RANGE=0, HGO=, EREFS=, LP=, IRCS=
	 */
	MCG_C2 = 0;

	/* Select external oscillator and Reference Divider and clear IREFS 
	 * to start external oscillator
	 * CLKS = 2, FRDIV = 3, IREFS = 0, IRCLKEN = 0, IREFSTEN = 0
	 */
	MCG_C1 = MCG_C1_CLKS(2) | MCG_C1_FRDIV(3);

	/* Wait for oscillator to initialize */
	/* Wait for Reference Clock Status bit to clear */
	while (MCG_S & MCG_S_IREFST_MASK) {};

	/* Wait for clock status bits to show clock source 
	 * is external reference clock */
	while (((MCG_S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) != 0x2) {};

	/* Now in FBE
	 * Configure PLL Reference Divider, PLLCLKEN = 0, PLLSTEN = 0, PRDIV = 0x18
	 * The crystal frequency is used to select the PRDIV value. 
	 * Only even frequency crystals are supported
	 * that will produce a 2MHz reference clock to the PLL.
	 */
	MCG_C5 = MCG_C5_PRDIV(BSP_REF_CLOCK_DIV - 1);

	/* Ensure MCG_C6 is at the reset default of 0. LOLIE disabled, 
	 * PLL disabled, clock monitor disabled, PLL VCO divider is clear
	 */
	MCG_C6 = 0;


	/* Calculate mask for System Clock Divider Register 1 SIM_CLKDIV1 */
	SIM_CLKDIV1 =  	SIM_CLKDIV1_OUTDIV1(BSP_CORE_DIV    - 1) |
			SIM_CLKDIV1_OUTDIV2(BSP_BUS_DIV     - 1) |
			SIM_CLKDIV1_OUTDIV3(BSP_FLEXBUS_DIV - 1) |
			SIM_CLKDIV1_OUTDIV4(BSP_FLASH_DIV   - 1);

	/* Set the VCO divider and enable the PLL, 
	 * LOLIE = 0, PLLS = 1, CME = 0, VDIV = 2MHz * BSP_CLOCK_MUL
	 */
	MCG_C6 = MCG_C6_PLLS_MASK | MCG_C6_VDIV(BSP_CLOCK_MUL - 24);

	/* Wait for PLL status bit to set */
	while (!(MCG_S & MCG_S_PLLST_MASK)) {};

	/* Wait for LOCK bit to set */
	while (!(MCG_S & MCG_S_LOCK_MASK)) {};

	/* Now running PBE Mode */
	set_MCG_PEE_Mode(); 


	/* Now running PEE Mode */
	return 0;
}

/* EOF */
