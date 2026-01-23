#include <Wire.h>
#include <SPI.h>
#include <Arduino.h>
#include "TouchDrvCSTXXX.hpp"
#include "pin_config.h"

#include <lvgl.h>
#include "Arduino_GFX_Library.h"
#include "lv_conf.h"
#include <demos/lv_demos.h>

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2

TouchDrvCSTXXX touch;
int16_t x[5], y[5];
bool isPressed = false;

uint32_t screenWidth;
uint32_t screenHeight;

static lv_disp_draw_buf_t draw_buf;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS /* CS */, LCD_SCLK /* SCK */, LCD_SDIO0 /* SDIO0 */, LCD_SDIO1 /* SDIO1 */,
  LCD_SDIO2 /* SDIO2 */, LCD_SDIO3 /* SDIO3 */);

Arduino_GFX *gfx = new Arduino_CO5300(
  bus,
  LCD_RESET /* RST */,
  0 /* rotation */,
  false /* IPS */,
  LCD_WIDTH,
  LCD_HEIGHT,
  6 /* col_offset1 */,
  0 /* row_offset1 */,
  0 /* col_offset2 */,
  0 /* row_offset2 */
);
#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char *buf) {
  Serial.printf(buf);
  Serial.flush();
}
#endif

void example_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area)
{
    if(area->x1 % 2 !=0)area->x1--;
    if(area->y1 % 2 !=0)area->y1--;
    // 变为奇数(如果是偶数就加 1)
    if(area->x2 %2 ==0)area->x2++;
    if(area->y2 %2 ==0)area->y2++;
}

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

  lv_disp_flush_ready(disp);
}

void example_increase_lvgl_tick(void *arg) {
  /* Tell LVGL how many milliseconds has elapsed */
  lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static uint8_t count = 0;
void example_increase_reboot(void *arg) {
  count++;
  if (count == 30) {
    esp_restart();
  }
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  uint8_t touched = touch.getPoint(x, y, touch.getSupportTouchPoint());

  if (touched > 0) {
    data->state = LV_INDEV_STATE_PR;  // 设置为按下状态
    data->point.x = x[0];             // 使用第一个触摸点的坐标
    data->point.y = y[0];

    Serial.print("X: ");
    Serial.print(data->point.x);
    Serial.print(" Y: ");
    Serial.println(data->point.y);
  } else {
    data->state = LV_INDEV_STATE_REL;  // 设置为释放状态
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;
  
  Wire.begin(IIC_SDA, IIC_SCL);

  uint8_t address = 0x5A;

  touch.setPins(-1, 11);

  bool result = touch.begin(Wire, address, 15, 14);

  touch.setMaxCoordinates(466, 466);
  touch.setMirrorXY(true, true);
  attachInterrupt(
    11, []() {
      isPressed = true;
    },
    FALLING);
  gfx->begin(30000000);
  gfx->Display_Brightness(200);

  screenWidth = gfx->width();
  screenHeight = gfx->height();

  lv_init();

  lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(screenWidth * screenHeight / 4 * sizeof(lv_color_t), MALLOC_CAP_DMA);

  lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(screenWidth * screenHeight / 4 * sizeof(lv_color_t), MALLOC_CAP_DMA);


#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif




  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, screenWidth * screenHeight / 4);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.rounder_cb = example_lvgl_rounder_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  const esp_timer_create_args_t lvgl_tick_timer_args = {
    .callback = &example_increase_lvgl_tick,
    .name = "lvgl_tick"
  };

  const esp_timer_create_args_t reboot_timer_args = {
    .callback = &example_increase_reboot,
    .name = "reboot"
  };

  esp_timer_handle_t lvgl_tick_timer = NULL;
  esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
  esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);

  // lv_demo_widgets();
  // lv_demo_benchmark();
  // lv_demo_keypad_encoder();
  lv_demo_music();
  // lv_demo_stress();

  Serial.println("Setup done");
}

void loop() {
  lv_timer_handler(); /* let the GUI do its work */

  delay(5);
}
