#include "ssd1322_driver.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <string.h> // memset
#include <stdlib.h> // abs
#include "hardware/dma.h"
#include <stdio.h>


// DMAチャネル取得
int dma_chan;

// フレームバッファの実体
uint8_t framebuffer[FRAMEBUFFER_SIZE];

void ssd1322_send_cmd(uint8_t cmd) {
    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 0);      // コマンドモード
    spi_write_blocking(SPI_PORT, &cmd, 1);
    gpio_put(PIN_CS, 1);
}

void ssd1322_send_data(const uint8_t *data, size_t len) {
    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 1);      // データモード
    spi_write_blocking(SPI_PORT, data, len);
    gpio_put(PIN_CS, 1);
}

void ssd1322_reset() {
    gpio_put(PIN_RESET, 1);
    sleep_ms(1);

    gpio_put(PIN_RESET, 0);
    sleep_ms(10);

    gpio_put(PIN_RESET, 1);
    sleep_ms(200);
}

//SSD1322 初期化
void ssd1322_init() {
    ssd1322_reset();

    ssd1322_send_cmd(0xFD); // set Command unlock
    uint8_t unlock = 0x12;
    ssd1322_send_data(&unlock, 1);
    ssd1322_send_cmd(0xAE); // set display off
    //ssd1322_send_cmd(0xB3); // set display clock divide ratio、画面のチラツキに関するコマンド
    //uint8_t clock_ratio = 0x91;   //OLEDの動画を撮ると画面がチラつくことがあるのでここをもっと早くするべきかも？
    //ssd1322_send_data(&clock_ratio, 1);
    ssd1322_send_cmd(0xCA); // set multiplex ratio
    uint8_t multiplex_ratio = 0x3F; // 64MUX for 64 rows
    ssd1322_send_data(&multiplex_ratio, 1);
    ssd1322_send_cmd(0xA2); // set display offset to 0
    uint8_t display_offset = 0x00;
    ssd1322_send_data(&display_offset, 1);
    ssd1322_send_cmd(0xA1); // start display start line to 0
    uint8_t start_line = 0x00;
    ssd1322_send_data(&start_line, 1);
    ssd1322_send_cmd(0xA0); // set remap and dual COM Line Mode
    uint8_t remap_data[2] = {0x14, 0x11}; // Default: 0x14, 0x11 (for 256x64)
    ssd1322_send_data(remap_data, 2);
    ssd1322_send_cmd(0xB5); // disable IO input
    uint8_t io_disable = 0x00;
    ssd1322_send_data(&io_disable, 1);
    ssd1322_send_cmd(0xAB); // function select
    uint8_t function_select = 0x01; // Enable internal VDD regulator
    ssd1322_send_data(&function_select, 1);
    ssd1322_send_cmd(0xB4); // enable VSL extern
    uint8_t vsl_extern[2] = {0xA0, 0xFD}; // External VSL
    ssd1322_send_data(vsl_extern, 2);
    ssd1322_send_cmd(0xC1); // set contrast current
    uint8_t contrast = 0xFF; // Max contrast
    ssd1322_send_data(&contrast, 1);
    ssd1322_send_cmd(0xC7); // set master contrast current
    uint8_t master_contrast = 0x0F; // Max master contrast
    ssd1322_send_data(&master_contrast, 1);
    ssd1322_send_cmd(0xB9); // default grayscale
    ssd1322_send_cmd(0xB1); // set phase length
    uint8_t phase_length = 0xE2; // Default
    ssd1322_send_data(&phase_length, 1);
    ssd1322_send_cmd(0xD1); // enhance driving scheme capability
    uint8_t driving_scheme[2] = {0x82, 0x20}; // Default
    ssd1322_send_data(driving_scheme, 2);
    ssd1322_send_cmd(0xBB); // first pre charge voltage
    uint8_t precharge1 = 0x1F; // Default
    ssd1322_send_data(&precharge1, 1);
    ssd1322_send_cmd(0xB6); // second pre charge voltage
    uint8_t precharge2 = 0x08; // Default
    ssd1322_send_data(&precharge2, 1);
    ssd1322_send_cmd(0xBE); // VCOMH
    uint8_t vcomh = 0x07; // Default
    ssd1322_send_data(&vcomh, 1);
    ssd1322_send_cmd(0xA6); // set normal display mode
    ssd1322_send_cmd(0xA9); // no partial mode
    sleep_ms(10);           // stabilize VDD
    ssd1322_send_cmd(0xAF); // display on
    sleep_ms(50);           // stabilize VDD
}


//直接OLEDをクリアする
void ssd1322_clear_screen() {
    // GDDRAMは4bit/pixel、2ピクセルで1バイト
    // Columnは0x1C〜0x5B = 240バイト（480px / 2）
    ssd1322_send_cmd(0x15); // Column Address
    ssd1322_send_data((uint8_t[]){0x1C, 0x5B}, 2);

    ssd1322_send_cmd(0x75); // Row Address
    ssd1322_send_data((uint8_t[]){0x00, 0x3F}, 2); // 64行

    ssd1322_send_cmd(0x5C); // Write RAM

    for (int i = 0; i < 128 * 240; i++) {
        uint8_t zero = 0x00;  // 全ピクセル OFF
        ssd1322_send_data(&zero, 1);
    }
}


//フレームバッファの指定座標にグレースケール（0〜15）でドットを描画
void ssd1322_draw_pixel(uint16_t x, uint16_t y, uint8_t gray) {
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;  // 範囲外保護
    gray &= 0x0F;  // 4bitにマスク

    size_t index = (y * (SCREEN_WIDTH / 2)) + (x / 2);
    if (x % 2 == 0) {
        // 左ピクセル（上位4bit）
        framebuffer[index] &= 0x0F;
        framebuffer[index] |= (gray << 4);
    } else {
        // 右ピクセル（下位4bit）
        framebuffer[index] &= 0xF0;
        framebuffer[index] |= gray;
    }
}

//フレームバッファのの指定箇所にラインを引く
void ssd1322_draw_line(int x0, int y0, int x1, int y1, uint8_t gray) {
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;  // error value

    while (1) {
        ssd1322_draw_pixel(x0, y0, gray);  // 1ドット描画

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}


//フレームバッファ全体をクリア（0: 黒）
void ssd1322_clear_buffer(void) {
    memset(framebuffer, 0x00, FRAMEBUFFER_SIZE);
}

// フレームバッファ全体を白（15）で塗りつぶす
void ssd1322_fill_buffer(uint8_t gray) {
    gray &= 0x0F;
    uint8_t packed = (gray << 4) | gray;
    memset(framebuffer, packed, FRAMEBUFFER_SIZE);
}

//フレームバッファをOLEDに書き出す
void ssd1322_flush_buffer(void) {
    //OLED書き込み先アドレスの設定
    //GDDRAMは4bit/pixel、2ピクセルで1バイト
    //Columnは0x1C〜0x5B = 240バイト（480px / 2）
    ssd1322_send_cmd(0x15); // Column Address
    ssd1322_send_data((uint8_t[]){0x1C, 0x5B}, 2);
    ssd1322_send_cmd(0x75); // Row Address
    ssd1322_send_data((uint8_t[]){0x00, 0x3F}, 2); // 64行
    ssd1322_send_cmd(0x5C); // Write RAM

    ssd1322_send_data(framebuffer, FRAMEBUFFER_SIZE);
}

//フレームバッファのDMA転送（自動転送で早い）、true = 転送スタート、false = DMA使用中エラー
bool ssd1322_flush_buffer_dma(void) {

    //DMA転送中なら転送せず戻る
    if (dma_channel_is_busy(dma_chan)) {
        return false;   //転送エラー
    }

    //OLED書き込み先アドレスの設定
    //GDDRAMは4bit/pixel、2ピクセルで1バイト
    //Columnは0x1C〜0x5B = 240バイト（480px / 2）
    ssd1322_send_cmd(0x15); // Column Address
    ssd1322_send_data((uint8_t[]){0x1C, 0x5B}, 2);
    ssd1322_send_cmd(0x75); // Row Address
    ssd1322_send_data((uint8_t[]){0x00, 0x3F}, 2); // 64行
    ssd1322_send_cmd(0x5C); // Write RAM

    gpio_put(PIN_CS, 0);    //OLEDを有効
    gpio_put(PIN_DC, 1);    //データモード

    // DMAチャネル取得
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_dreq(&cfg, spi_get_dreq(SPI_PORT, true));  // SPI TX DREQ
    channel_config_set_read_increment(&cfg, true);  // フレームバッファ側はインクリメント
    channel_config_set_write_increment(&cfg, false); // SPI FIFOアドレス固定

    //DMA転送終了後に割り込みを発生させる
    dma_channel_set_irq0_enabled(dma_chan, true);
    //DMA_IRQ_0割り込みが発生したらssd1322_dma_irq_handlerを実行させる
    irq_set_exclusive_handler(DMA_IRQ_0, ssd1322_dma_irq_handler);
    //割り込みを有効
    irq_set_enabled(DMA_IRQ_0, true);

    dma_channel_configure(
        dma_chan, &cfg,
        &spi_get_hw(SPI_PORT)->dr,  // 書き込み先（SPI FIFO）
        framebuffer,                       // 読み出し元
        FRAMEBUFFER_SIZE,                       // バイト数
        true                        // 即スタート
    );

    return true;    //転送成功
}

//DMA転送終了割り込みハンドラ
void ssd1322_dma_irq_handler(void){
    //DMA割り込みフラグのクリア
    dma_hw->ints0 = 1u << dma_chan;
    //SPI送信が完全に終わるまで待機
    while (spi_is_busy(SPI_PORT)) {
        tight_loop_contents();
    }

    gpio_put(PIN_CS, 1);    //OLEDチップセレクトをハイ
    dma_channel_unclaim(dma_chan);  //DMAチャンネルの解放
}
