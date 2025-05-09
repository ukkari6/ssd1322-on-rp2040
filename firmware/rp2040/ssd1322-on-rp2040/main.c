/*
    2025/5/9
    SSD1322の制御コード
*/


#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "pico/stdio_uart.h"
#include <string.h>

#include "ssd1322_driver.h"

#include <stdlib.h> // abs() 関数のために追加
#include <math.h>  // sin() 用

#ifndef PI // PIが未定義の場合 (ssd1322_driver.cでも定義されているので重複を避ける)
#define PI 3.14159265
#endif



const uint LED = 25; // オンボードLED


//SPIピン（マスター）
#define SPI_PORT spi0
#define PIN_MISO 0      //1pin  データ入力（未使用）
#define PIN_CS   1      //2pin  チップセレクト
#define PIN_SCK  2      //4pin  クロック
#define PIN_MOSI 3      //5pin  データ出力
#define PIN_DC   4      //6pin  コマンド・データ切り替え 
#define PIN_RESET 5     //7pin  リセット


// UART defines
// By default the stdout UART is `uart0`, so we will use the second one
#define TX16_RX17_UART0 uart0
#define BAUD_RATE 115200

// Use pins 4 and 5 for UART1
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define UART_TX_PIN 16
#define UART_RX_PIN 17


//SPIマスターの初期化
void    SPI_Master_init()
{
    // SPI initialisation. This example will use SPI at 1MHz.
    //spi_init(SPI_PORT, 100 * 1000);    // 0.1MHz (100kHz)
    //spi_init(SPI_PORT, 1000*1000);        //1MHz
    spi_init(SPI_PORT, 24 * 1000*1000);  // 24MHz
    //spi_init(SPI_PORT, 31 * 1000*1000);  // 31MHz   SSD1322は31MHzいけるけど念の為少し遅くしたほうがいいかも

    // SPIのフォーマットを設定 (CPOL, CPHA, データビット数, ビットオーダー)
    //モード 0: CPOL=0, CPHA=0 (SPI_CPOL_0, SPI_CPHA_0)     //SSD1322はこれ
    //モード 1: CPOL=0, CPHA=1 (SPI_CPOL_0, SPI_CPHA_1) 
    //モード 2: CPOL=1, CPHA=0 (SPI_CPOL_1, SPI_CPHA_0)
    //モード 3: CPOL=1, CPHA=1 (SPI_CPOL_1, SPI_CPHA_1)
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);
    // For more examples of SPI use see https://github.com/raspberrypi/pico-examples/tree/master/spi

    gpio_init(PIN_DC);
    gpio_set_dir(PIN_DC, GPIO_OUT);

    gpio_init(PIN_RESET);
    gpio_set_dir(PIN_RESET, GPIO_OUT);
}



//UARTの初期化
void    UART_init()
{
    // Set up our UART
    uart_init(TX16_RX17_UART0, BAUD_RATE);
    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    
    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART
    
    // Send out a string, with CR/LF conversions
    //uart_puts(TX16_RX17_UART0, " Hello, UART!\n");
    
    // For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart
}


//GPIOの初期化
void    GPIO_init()
{
    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);
}



//フレームバッファにサイン波を描画する
void ssd1322_draw_sine_wave(float amplitude, float frequency, uint8_t gray) {
    for (uint16_t x = 0; x < SCREEN_WIDTH; x++) {
        float radians = 2 * PI * frequency * ((float)x / SCREEN_WIDTH);
        float y_f = (SCREEN_HEIGHT / 2) + amplitude * sinf(radians);
        uint16_t y = (uint16_t)y_f;

        if (y < SCREEN_HEIGHT) {
            ssd1322_draw_pixel(x, y, gray);
        }
    }
}

//回数を指定してサイン波を描画する
void sine_wave(int cnt){
    for(int k = 0; k < cnt; k++){
        for(float i = 0.0f; i <= 10.0f; i += 0.1f){
            ssd1322_clear_buffer();
            ssd1322_draw_sine_wave(20.0f, i, 15);  // 振幅20px、周波数2波、白で描画
            ssd1322_flush_buffer();           // 描画反映
            sleep_ms(1);
        }
        for(float i = 10.0f; i >= 0.0f; i -= 0.1f){
            ssd1322_clear_buffer();
            ssd1322_draw_sine_wave(20.0f, i, 15);  // 振幅20px、周波数2波、白で描画
            ssd1322_flush_buffer();           // 描画反映
            sleep_ms(1);
        }
    }
}

//箱の描画アニメーション
void draw_box_anime(){
    ssd1322_clear_buffer();
    ssd1322_draw_line(0, 0, 255, 0, 10);   // 横線
    ssd1322_flush_buffer();                  //フレームバッファをOLEDに書き出し
    sleep_ms(500);

    ssd1322_draw_line(0, 0, 0, 63, 10);   // 横線
    ssd1322_flush_buffer();                  //フレームバッファをOLEDに書き出し
    sleep_ms(500);

    ssd1322_draw_line(0, 63, 255, 63, 10);   // 横線
    ssd1322_flush_buffer();                  //フレームバッファをOLEDに書き出し
    sleep_ms(500);

    ssd1322_draw_line(255, 0, 255, 63, 10);   // 横線
    ssd1322_flush_buffer();                  //フレームバッファをOLEDに書き出し
    sleep_ms(500);
}

void draw_random_dot_anime(){
    ssd1322_clear_buffer();
    for(int i = 0; i < 100; i++){
        uint16_t x = rand() % SCREEN_WIDTH;    // 0 から SCREEN_WIDTH-1 の乱数
        uint16_t y = rand() % SCREEN_HEIGHT;   // 0 から SCREEN_HEIGHT-1 の乱数
        uint8_t gray = rand() % 16;          // 0 から 15 の乱数 (グレースケール)
        ssd1322_draw_pixel(x, y, gray);
        ssd1322_flush_buffer();           // 描画反映
        sleep_ms(10); // 連続で描画する場合はコメントアウトしても良いかもしれません
    }
}



int main()
{
    //stdio_init_all();   //UARTの初期化、デバッグ出力用

    //デバッグの出力UART0をGPIO16とGPIO17に変更
    //printfはUSB-C端子からではなくGPIO16とGPIO17からの出力になります
    stdio_uart_init_full(uart0, BAUD_RATE, UART_TX_PIN, UART_RX_PIN);

    GPIO_init();
    //UART_init();
    SPI_Master_init();
    ssd1322_init();     //SSD1322の初期化
    ssd1322_clear_screen();     //OLEDの表示を直接クリア
    ssd1322_clear_buffer();     //フレームバッファをクリア

    //フレームバッファの指定座標にグレースケール（0〜15）でドットを描画
    //void ssd1322_draw_pixel(uint16_t x, uint16_t y, uint8_t gray);

    //フレームバッファのの指定箇所にラインを引く
    //void ssd1322_draw_line(int x0, int y0, int x1, int y1, uint8_t gray);

    //フレームバッファ全体をクリア（0: 黒）
    //void ssd1322_clear_buffer(void);

    // フレームバッファ全体を指定色で塗りつぶす
    //void ssd1322_fill_buffer(uint8_t gray);

    //フレームバッファをOLEDに書き出す
    //void ssd1322_flush_buffer(void);

    gpio_put(LED, 1);   //電源ON

    while (true) {
        printf("SSD1322\n");

        sine_wave(2);    //サイン波アニメーションの描画
        draw_box_anime(); //上下左右にラインを描画するアニメーション
        draw_random_dot_anime();    //ドットを乱数で描画

        //ssd1322_draw_pixel(255, 0, 15);      // 白ドット
        //ssd1322_draw_pixel(0, 63, 10);       // 中間グレー
        //ssd1322_draw_line(0, 0, 255, 63, 5);   //ラインを引く
        //ssd1322_flush_buffer();           // OLEDに書き出す（draw後は必ずOLEDに書き出すこと）
    }
}