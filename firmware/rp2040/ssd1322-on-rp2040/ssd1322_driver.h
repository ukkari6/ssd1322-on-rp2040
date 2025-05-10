#ifndef SSD1322_DRIVER_H
#define SSD1322_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h> 


//SSD1322の解像度
#define SCREEN_WIDTH    256  // SSD1322は最大256px（横）
#define SCREEN_HEIGHT   64   // 縦は64 or 128によくある
#define FRAMEBUFFER_SIZE ((SCREEN_WIDTH * SCREEN_HEIGHT) / 2)

// フレームバッファ（1バイトに2ピクセル）
extern uint8_t framebuffer[FRAMEBUFFER_SIZE];

#define SPI_PORT spi0
#define PIN_MISO 0      //1pin  データ入力（未使用想定）
#define PIN_CS   1      //2pin  チップセレクト
#define PIN_SCK  2      //4pin  クロック
#define PIN_MOSI 3      //5pin  データ出力
#define PIN_DC   4      //6pin  コマンド・データ切り替え (ファイル内でDCだと他と被る可能性があるので変更)
#define PIN_RESET 5     //7pin  リセット (ファイル内でRESETだと他と被る可能性があるので変更)

//SSD1322 通信用関数
void ssd1322_send_cmd(uint8_t cmd);
void ssd1322_send_data(const uint8_t *data, size_t len);
void ssd1322_reset();

//SSD1322 初期化
void ssd1322_init();

//直接画面をクリアする
void ssd1322_clear_screen();

//フレームバッファの指定座標にグレースケール（0〜15）でドットを描画
void ssd1322_draw_pixel(uint16_t x, uint16_t y, uint8_t gray);

//フレームバッファのの指定箇所にラインを引く
void ssd1322_draw_line(int x0, int y0, int x1, int y1, uint8_t gray);

//フレームバッファ全体をクリア（0: 黒）
void ssd1322_clear_buffer(void);

// フレームバッファ全体を指定色で塗りつぶす
void ssd1322_fill_buffer(uint8_t gray);

//フレームバッファをポーリングで送信（転送終了まで待つので遅い）
void ssd1322_flush_buffer(void);

//フレームバッファのDMA転送（自動転送で早い）、true = 転送スタート、false = DMA使用中エラー
bool ssd1322_flush_buffer_dma(void);


//DMA完了割り込みハンドラ
void ssd1322_dma_irq_handler(void);



#endif // SSD1322_DRIVER_H