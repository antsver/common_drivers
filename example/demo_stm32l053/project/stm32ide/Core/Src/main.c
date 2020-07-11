/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
#include <assert.h>
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "drv_swtimers.h"
#include "drv_leds.h"
#include "drv_buttons.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
enum {
	TIMER_LED_1,
	TIMER_BTN_1,
	TIMER_APP_1,
	TIMER_DELAY,
	TIMERS_NUM
};

enum {
	LED_1,
	LEDS_NUM
};

enum {
	BTN_1,
	BTNS_NUM
};

enum {
	GPIO_BTN1,
	GPIO_LED1,
	GPIO_NUM
};

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

void ISR_SysTick_Handler_cb(void);

static void hw_timer_isr_enable(void * hw_timer_p);
static void hw_timer_isr_disable(void * hw_timer_p);
static void hw_gpio_write(void * hw_gpio_p, uint32_t pin_idx, uint8_t pin_state);
static void hw_gpio_toggle(void * hw_gpio_p, uint32_t pin_idx);
static bool hw_gpio_read(void * hw_gpio_p, uint32_t pin_idx);

static void app_led_all_test_blink(void);
static void app_timer_handler(uint32_t timer_idx, void * arg_1_p, void * arg_2_p);
static void app_button_handler(uint32_t button_idx, buttons_event_t event, void * arg_p);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//------------------------------------------------------------------------------
// Software timers driver instance
//------------------------------------------------------------------------------
static swtimers_t timers_inst;
static swtimers_timer_t timers_table[TIMERS_NUM];

//------------------------------------------------------------------------------
// Hardware timer interface
//------------------------------------------------------------------------------
static const swtimers_hw_interface_t timers_hw_itf = {
    .hw_timer_p = NULL,
    .isr_enable_cb = hw_timer_isr_enable,
    .isr_disable_cb = hw_timer_isr_disable,
    .hw_start_cb = NULL,
    .hw_stop_cb = NULL,
    .hw_is_started_cb = NULL,
    .tick_ms = 1,
};

//------------------------------------------------------------------------------
// LEDs driver instance
//------------------------------------------------------------------------------
static leds_t leds_inst;
static leds_led_t leds_table[LEDS_NUM];

//------------------------------------------------------------------------------
// Hardware GPIO interface for LEDs driver
//------------------------------------------------------------------------------
static const leds_hw_interface_t leds_hw_itf = {
    .hw_gpio_p = NULL,
    .gpio_write = hw_gpio_write,
	.gpio_toggle = hw_gpio_toggle,
};

//------------------------------------------------------------------------------
// Buttons driver instance
//------------------------------------------------------------------------------
static buttons_t buttons_inst;
static buttons_button_t buttons_table[BTNS_NUM];

static const buttons_time_settings_t buttons_time_settings = {
	.bouncing_ms = 50,
	.double_click_ms = 500,
	.hold_ms = 3000,
};

//------------------------------------------------------------------------------
// Hardware GPIO interface for Buttons driver
//------------------------------------------------------------------------------
static const buttons_hw_interface_t buttons_hw_itf = {
    .hw_gpio_p = NULL,
    .isr_enable_cb = NULL,
    .isr_disable_cb = NULL,
    .gpio_read = hw_gpio_read,
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void __assert_func (const char * file, int line, const char * func, const char * expr)
{
	(void)file;
	(void)line;
	(void)func;
	(void)expr;

	while(1) {
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void ISR_SysTick_Handler_cb(void)
{
    swtimers_isr(&timers_inst);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void hw_timer_isr_enable(void * hw_timer_p)
{
    (void)hw_timer_p;

    HAL_ResumeTick();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void hw_timer_isr_disable(void * hw_timer_p)
{
    (void)hw_timer_p;

    HAL_SuspendTick();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void delay_ms(uint32_t ms)
{
	swtimers_start(&timers_inst, TIMER_DELAY, ms, SWTIMERS_MODE_SINGLE_FROM_ISR, NULL, NULL, NULL);
	while (swtimers_is_run(&timers_inst, TIMER_DELAY, NULL) == true) {
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void hw_gpio_write(void * hw_gpio_p, uint32_t pin_idx, uint8_t pin_state)
{
	(void)hw_gpio_p;

	if (pin_idx == GPIO_LED1) {
	    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, pin_state);
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void hw_gpio_toggle(void * hw_gpio_p, uint32_t pin_idx)
{
	(void)hw_gpio_p;

	if (pin_idx == GPIO_LED1) {
        HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool hw_gpio_read(void * hw_gpio_p, uint32_t pin_idx)
{
	(void)hw_gpio_p;

	if (pin_idx == GPIO_BTN1) {
		GPIO_PinState pin_state = HAL_GPIO_ReadPin(BTN1_GPIO_Port, BTN1_Pin);
		return (pin_state == GPIO_PIN_SET);
	}

	return false;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void app_led_all_test_blink(void)
{
	delay_ms(100);
	HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
    delay_ms(100);
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
	delay_ms(100);
	HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
    delay_ms(100);
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
    delay_ms(100);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void app_timer_handler(uint32_t timer_idx, void * arg_1_p, void * arg_2_p)
{
    (void)timer_idx;
    (void)arg_1_p;
    (void)arg_2_p;

    leds_blink(&leds_inst, LED_1, 3, 50, 100, 0);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void app_button_handler(uint32_t button_idx, buttons_event_t event, void * arg_p)
{
	(void)arg_p;

	if (button_idx == BTN_1) {

		// Stop app timer and blinking
		swtimers_stop(&timers_inst, TIMER_APP_1);
		leds_off(&leds_inst, LED_1);

		// Indicate button event
		if (event & BUTTONS_PRESSED) {
			leds_switch_on(&leds_inst, LED_1);
		}

		if (event & BUTTONS_RELEASED) {
			leds_switch_off(&leds_inst, LED_1);
		}

		if (event & BUTTONS_HOLD) {
			// Blink twice for 500 ms in 1s
			leds_blink_ext(&leds_inst, LED_1, 2, 500, 100, 0, 1000, false);
		}

		if (event & BUTTONS_DOUBLE) {
			// Blink twice for 100 ms in 1s
			leds_blink_ext(&leds_inst, LED_1, 2, 100, 100, 0, 1000, false);
		}
	}
}



/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  // Run tests
  int32_t res = swtimers_tests();
  assert(res == 0);
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */

  // Init software timers driver
  swtimers_init(&timers_inst, &timers_hw_itf, TIMERS_NUM, timers_table);

  // Init LEDs driver
  leds_init(&leds_inst, &leds_hw_itf, LEDS_NUM, leds_table, &timers_inst);
  leds_set_pin(&leds_inst, LED_1, GPIO_LED1, TIMER_LED_1, true);

  // Init Buttons driver
  buttons_init(&buttons_inst, &buttons_hw_itf, BTNS_NUM, buttons_table, &timers_inst);
  buttons_configure(&buttons_inst, BTN_1, GPIO_BTN1, TIMER_BTN_1, true, BUTTONS_CHECK_IN_POLLING,
		  &buttons_time_settings, app_button_handler, NULL);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  // Initial blink
  app_led_all_test_blink();

  // Run app timer
  swtimers_start(&timers_inst, TIMER_APP_1, 3000, SWTIMERS_MODE_PERIODIC_FROM_LOOP, app_timer_handler, NULL, NULL);

  while (1) {
      // Driver routines
      swtimers_task(&timers_inst);
      buttons_task(&buttons_inst);

      delay_ms(5);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage 
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_4;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : BTN1_Pin */
  GPIO_InitStruct.Pin = BTN1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BTN1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LED1_Pin */
  GPIO_InitStruct.Pin = LED1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED1_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
