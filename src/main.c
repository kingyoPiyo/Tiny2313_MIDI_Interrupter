/*
 * ATtiny2313使用 DRSSTC制御用 6和音MIDIデコーダ
 * ピッチベンド対応Ver
 *
 * 製作開始: 2013/05/03
 * 最終更新: 2020/08/08
 * 製作者 : kingyo
 *
 * クロック: 20MHz
 * 発音可能ノート番号: 0-108
 *
 * MIDIメッセージ(参考)
 * MIDI_NoteOff             (0x8x)
 * MIDI_NoteOn              (0x9x)
 * MIDI_ControlChange       (0xBx)  // 未使用
 * MIDI_ProgramChange       (0xCx)  // 未使用
 * MIDI_ChannelPressure     (0xDx)  // 未使用
 * MIDI_PitchBend           (0xEx)  // 未使用
 */ 

/*
         ATtiny2313
             ___    ___
RESET       [1  |__| 20] Vcc
PD0(RxD)    [2       19] PB7(SCK)
PD1(Mode)   [3       18] PB6(MISO)
XTAL2       [4       17] PB5(MOSI)
XTAL1       [5       16] PB4(NC)
PD2(Out)    [6       15] PB3(ChMode)
PD3(NC)     [7       14] PB2(ChMode)
PD4(NC)     [8       13] PB1(ChMode)
PD5(OutLED) [9       12] PB0(ChMode)
GND         [10      11] PD6(ErrorLED)
            ~~~~~~~~~~~~
 */

#include <avr/io.h>
#include <avr/interrupt.h>  // 割り込み
#include <avr/pgmspace.h>   // 定数をフラッシュに配置する為に必要

/* 定義値 */
#define DEF_BUFSIZE     (16)    // UART受信バッファサイズ 2^nバイト
#define DEF_WAON_NUM    (6)     // 和音数(変更しないで！)
// 矩形波出力ピン (PD2)
#define DEF_SOUT_PORT   (PORTD) // 出力ポート
#define DEF_SOUT_DDR    (DDRD)  // 出力方向選択
#define DEF_SOUT_BIT    (2)     // 出力bit
// 出力状態表示LED (PD5)
#define DEF_SLED_PORT   (PORTD) // 出力ポート
#define DEF_SLED_DDR    (DDRD)  // 出力方向選択
#define DEF_SLED_BIT    (5)     // 出力bit
// 音源不足状態表示LED (PD6)
#define DEF_ELED_PORT   (PORTD) // 出力ポート
#define DEF_ELED_DDR    (DDRD)  // 出力方向選択
#define DEF_ELED_BIT    (6)     // 出力bit

// グローバル変数
static uint8_t s_playing[DEF_WAON_NUM];     // 音源状態格納（ノート番号）
static uint16_t t_l[DEF_WAON_NUM];          // 音程に対応したパルス幅を格納
static uint16_t tim0Cnt, num[DEF_WAON_NUM]; // タイマ割り込み内で使用
static uint8_t lackCnt = 0;                 // 不足音源数カウンタ
static uint8_t pitchBendMSB, pitchBendSens; // ピッチベンドパラメータ

PROGMEM const uint16_t table[] = {12741, 12718, 12695, 12672, 12649, 12626, 12604, 12581, 12558, 12536, 12513, 12490, 12468, 12445, 12423, 12401, 12378, 12356, 12334, 12311, 12289, 12267, 12245, 12223, 12201, 12179, 12157, 12135, 12113, 12091, 12069, 12047, 12026, 12004, 11982, 11961, 11939, 11918, 11896, 11875, 11853, 11832, 11811, 11789, 11768, 11747, 11726, 11705, 11683, 11662, 11641, 11620, 11599, 11578, 11558, 11537, 11516, 11495, 11474, 11454, 11433, 11412, 11392, 11371, 11351, 11330, 11310, 11290, 11269, 11249, 11229, 11208, 11188, 11168, 11148, 11128, 11108, 11088, 11068, 11048, 11028, 11008, 10988, 10968, 10948, 10929, 10909, 10889, 10870, 10850, 10830, 10811, 10791, 10772, 10752, 10733, 10714, 10694, 10675, 10656, 10637, 10617, 10598, 10579, 10560, 10541, 10522, 10503, 10484, 10465, 10446, 10428, 10409, 10390, 10371, 10353, 10334, 10315, 10297, 10278, 10260, 10241, 10223, 10204, 10186, 10167, 10149, 10131, 10112, 10094, 10076, 10058, 10040, 10022, 10003, 9985, 9967, 9949, 9932, 9914, 9896, 9878, 9860, 9842, 9825, 9807, 9789, 9771, 9754, 9736, 9719, 9701, 9684, 9666, 9649, 9631, 9614, 9597, 9579, 9562, 9545, 9528, 9510, 9493, 9476, 9459, 9442, 9425, 9408, 9391, 9374, 9357, 9340, 9323, 9307, 9290, 9273, 9256, 9240, 9223, 9206, 9190, 9173, 9157, 9140, 9124, 9107, 9091, 9074, 9058, 9042, 9025, 9009, 8993, 8977, 8960, 8944, 8928, 8912, 8896, 8880, 8864, 8848, 8832, 8816, 8800, 8784, 8768, 8753, 8737, 8721, 8705, 8690, 8674, 8658, 8643, 8627, 8612, 8596, 8581, 8565, 8550, 8534, 8519, 8504, 8488, 8473, 8458, 8442, 8427, 8412, 8397, 8382, 8366, 8351, 8336, 8321, 8306, 8291, 8276, 8261, 8247, 8232, 8217, 8202, 8187, 8172, 8158, 8143, 8128, 8114, 8099, 8084, 8070, 8055, 8041, 8026, 8012, 7997, 7983, 7968, 7954, 7940, 7925, 7911, 7897, 7883, 7868, 7854, 7840, 7826, 7812, 7798, 7784, 7770, 7756, 7742, 7728, 7714, 7700, 7686, 7672, 7658, 7644, 7631, 7617, 7603, 7589, 7576, 7562, 7548, 7535, 7521, 7508, 7494, 7481, 7467, 7454, 7440, 7427, 7413, 7400, 7387, 7373, 7360, 7347, 7334, 7320, 7307, 7294, 7281, 7268, 7255, 7241, 7228, 7215, 7202, 7189, 7176, 7163, 7151, 7138, 7125, 7112, 7099, 7086, 7074, 7061, 7048, 7035, 7023, 7010, 6997, 6985, 6972, 6960, 6947, 6934, 6922, 6909, 6897, 6885, 6872, 6860, 6847, 6835, 6823, 6810, 6798, 6786, 6774, 6761, 6749, 6737, 6725, 6713, 6701, 6689, 6677, 6664, 6652, 6640, 6628, 6617, 6605, 6593, 6581, 6569, 6557, 6545, 6533, 6522, 6510, 6498, 6486, 6475, 6463, 6451, 6440, 6428, 6417, 6405, 6393, 6382};
// リングバッファ用変数
volatile static uint8_t RxBuf[DEF_BUFSIZE];     // バッファ
volatile static uint8_t Pdata;                  // ポインタ

/*****************************
 * 矩形波再生関数
 *  - note:ノート番号
 *****************************/
void playSound(uint8_t note)
{
    uint8_t i;
    uint8_t oct;
    uint16_t index;
    uint16_t intTone;
    
    if (note >= 0 && note <= 108) {             // 発音可能範囲のみ(0-108)
        for (i = 0; i < DEF_WAON_NUM; i++) {    // 音源1から順に発音中か確認して発音していないならそれを使用する
            if (s_playing[i] == 0x80) {
                // ノート番号を32倍して内部トーンに変換
                intTone = note << 5;
                // ピッチベンドの中央からの差分とピッチベンドセンシティビティを乗算してトーンに加算
                intTone += ((pitchBendMSB - 64) * pitchBendSens) >> 1;
                oct = intTone / 384;
                index = intTone % 384;
                cli();
                t_l[i] = pgm_read_word(&table[index]) >> oct;
                num[i] = tim0Cnt + t_l[i];            // 初期値計算
                s_playing[i] = note;
                sei();
                return;
            }
        }
    }
    lackCnt++;
}

/*****************************
 * 矩形波停止関数
 *  - note:ノート番号
 *****************************/
void stopSound(uint8_t note)
{
    uint8_t i;
    
    for (i = 0; i < DEF_WAON_NUM; i++) {   // 音源1から順にノートオフに当たる音源を探し、見つかれば発音停止
        if (s_playing[i] == note) {   // 探しているノート番号でかつポートも同じか？
            s_playing[i] = 0x80;      // 無音に設定(0x80は無音)
            return;
        }
    }
    lackCnt--;
}

/*****************************
 * ピッチベンド関数
 * 既に鳴っている音の周波数を変更する
 *****************************/
void bendSound(void)
{
    uint8_t i;
    uint8_t oct;
    uint16_t index;
    uint16_t intTone;
    
    for (i = 0; i < DEF_WAON_NUM; i++) {
        if (s_playing[i] != 0x80) {
            // ノート番号を32倍して内部トーンに変換
            intTone = s_playing[i] << 5;
            // ピッチベンドの中央からの差分とピッチベンドセンシティビティを乗算してトーンに加算
            intTone += ((pitchBendMSB - 64) * pitchBendSens) >> 1;
            oct = intTone / 384;
            index = intTone % 384;
            cli();
            t_l[i] = pgm_read_word(&table[index]) >> oct;
            sei();
        }
    }
}

/*****************************
 * UART受信割込み
 *****************************/
ISR(USART_RX_vect)
{
    // リングバッファに保存
    Pdata++;
    Pdata &= (DEF_BUFSIZE - 1);
    RxBuf[Pdata] = UDR;
}

/*****************************
 * タイマ0比較一致A割り込み
 * ※9.6us周期
 *****************************/
ISR(TIMER0_COMPA_vect)
{
    uint8_t tmp = 0;

    tim0Cnt++;
    
    if (s_playing[0] != 0x80) {     // 音源1
        if (tim0Cnt == num[0]) {
            num[0] += t_l[0];
            tmp = 1;
        }
    }

    if (s_playing[1] != 0x80) {     // 音源2
        if (tim0Cnt == num[1]) {
            num[1] += t_l[1];
            tmp = 1;
        }
    }
    
    if (s_playing[2] != 0x80) {     // 音源3
        if (tim0Cnt == num[2]) {
            num[2] += t_l[2];
            tmp = 1;
        }
    }
    
    if (s_playing[3] != 0x80) {     // 音源4
        if (tim0Cnt == num[3]) {
            num[3] += t_l[3];
            tmp = 1;
        }
    }

    if (s_playing[4] != 0x80) {     // 音源5
        if (tim0Cnt == num[4]) {
            num[4] += t_l[4];
            tmp = 1;
        }
    }

    if (s_playing[5] != 0x80) {     // 音源6
        if (tim0Cnt == num[5]) {
            num[5] += t_l[5];
            tmp = 1;
        }
    }

    // 出力ポート反映
    if (tmp) {
        DEF_SOUT_PORT |= (1 << DEF_SOUT_BIT);
    } else {
        DEF_SOUT_PORT &= ~(1 << DEF_SOUT_BIT);
    }
}

/*****************************
 * 音源再生状態LED表示
 *****************************/
void updateSoundStatusLED(void)
{
    uint8_t i, tmp;

    // 音源再生状態
    for (tmp = 0, i = 0; i < DEF_WAON_NUM; i++)
    {
        if (s_playing[i] != 0x80) {
            tmp = 1;
            break;
        }
    }

    if (tmp) {
        DEF_SLED_PORT |= (1 << DEF_SLED_BIT);
    } else {
        DEF_SLED_PORT &= ~(1 << DEF_SLED_BIT);
    }

    // 音源不足状態表示LED
    if (lackCnt != 0) {
        DEF_ELED_PORT |= (1 << DEF_ELED_BIT);
    } else {
        DEF_ELED_PORT &= ~(1 << DEF_ELED_BIT);
    }
}

void initSound(void)
{
    uint8_t i;

    pitchBendMSB = 64;
    pitchBendSens = 2;
    lackCnt = 0;
    for (i = 0; i < DEF_WAON_NUM; i++) {
        s_playing[i] = 0x80;
    }
}

/*****************************
 * MIDIメッセージ処理
 *****************************/
void procMidiMsg(uint8_t midi_ch)
{
    static uint8_t statusByte, byte_1, byte_cnt, RxByte, Pout;
    static uint8_t rpn_msb = 127, rpn_lsb = 127;


    // UART受信バッファが空
    if (Pdata == Pout) {
        return;
    }        

    // リングバッファから1バイト取り出し
    Pout++;
    Pout &= (DEF_BUFSIZE - 1);
    RxByte = RxBuf[Pout];

    // データ解析
    if (RxByte & 0x80) {    // ステータスバイト(MSB = 1)
        statusByte = RxByte;
        byte_cnt = 1;
        return;
    }

    if (byte_cnt == 1) {    // 第一データバイト
        byte_1 = RxByte;
        byte_cnt = 2;
        return;
    }

    if (byte_cnt == 2) {    // 第二データバイト
        if (statusByte == (0x90 | midi_ch)) {  // ノートオンイベント
            if (RxByte == 0) {      // ベロシティが0(ノートオフ)
                stopSound(byte_1);
            } else {
                playSound(byte_1);
            }
        } else if (statusByte == (0x80 | midi_ch)) {    // ノートオフイベント
            stopSound(byte_1);
        } else if (statusByte == (0xE0 | midi_ch)) {    // ピッチベンド
            pitchBendMSB = RxByte;
            // 既に鳴っている音の周波数を変更する
            bendSound();
        } else if (statusByte == (0xB0 | midi_ch)) {
            // コントロールチェンジ
            switch (byte_1)
            {
                // Data Entry MSB
                case 6:
                    if (rpn_msb == 0 && rpn_lsb == 0) {
                        pitchBendSens = RxByte;
                        rpn_msb = 127;
                        rpn_lsb = 127;
                    }
                    break;

                // RPN(LSB)
                case 100:
                    rpn_lsb = RxByte;
                    break;

                // RPN(MSB)
                case 101:
                    rpn_msb = RxByte;
                    break;

                // Reset All Controller
                case 121:
                    initSound();
                    break;

                default:
                    break;
            }
        }
        byte_cnt = 1;
    }
}

int main(void)
{
    uint8_t midi_ch;
    
    // IOポート設定
    PORTB = 0b00001111;         // PB0 - 3 PullUp Enable
    DEF_SOUT_DDR |= 1 << DEF_SOUT_BIT;  // 矩形波出力ピン
    DEF_SLED_DDR |= 1 << DEF_SLED_BIT;  // 音源状態表示LED
    DEF_ELED_DDR |= 1 << DEF_ELED_BIT;  // 音源不足表示LED

    // タイマ0設定
    TCCR0A = 0b00000010;        // CTC動作
    TCCR0B = 0b00000001;        // Clk/1（前置分周なし）
    OCR0A  = 192 - 1;           // カウンタトップ値(割り込み周期 9.6us) [OCR0A = (9.6us/0.05us) - 1 = 191]
    TIMSK  = 0b00000001;        // 比較A割り込み許可

    // UART設定
#if 0
    // 31.25kbps@20MHz
    UBRRH = 0x00;
    UBRRL = 0x27;
#else
    // 38.4kbps@20MHz
    UBRRH = 0x00;
    UBRRL = 0x20;
#endif
    UCSRB = 0b10010000;     // 受信割り込み許可/受信許可
    UCSRC = 0b00000110;     // 非同期動作/パリティ無し/ストップ1bit/データ8bit

    // 音源状態初期化
    initSound();
    
    // MIDI CH判別
    midi_ch = PINB & 0x0F;

    // 全割り込み許可
    sei();
    
    while (1)
    {
        procMidiMsg(midi_ch);
        updateSoundStatusLED();
    }
}
