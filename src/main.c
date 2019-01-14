/*
 * ATtiny2313使用 DRSSTC制御用 6和音MIDIデコーダ ver2.1
 *
 * 製作開始: 2013/05/03
 * 最終更新: 2013/07/07
 * 製作者 : kingyo
 *
 * クロック: 20MHz
 * 発音可能ノート番号: 0-108
 *
 * MIDIメッセージ(参考)
 * MIDI_NoteOff			(0x8x)
 * MIDI_NoteOn				(0x9x)
 * MIDI_ControlChange		(0xBx)	// 未使用
 * MIDI_ProgramChange		(0xCx)	// 未使用
 * MIDI_ChannelPressure	(0xDx)	// 未使用
 * MIDI_PitchBend			(0xEx)	// 未使用
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>	// 割り込み
#include <avr/pgmspace.h>	// 定数をフラッシュに配置する為に必要
#include <avr/eeprom.h>		// EEPROM使用のため

// 定義値
#define BUFSIZE	16			// バッファサイズ 2^nバイト
#define WAON		6			// 和音数(変更しないで！)

// グローバル変数
uint8_t out = 0x07;			// out初期値
uint8_t s_playing[WAON+1];	// 6和音 + 音源不足検知用
uint8_t out_port[6];			// 出力ピン指定用
uint8_t out_flg[3] = {0, 0, 0};
uint8_t s_l, s_h;				// ノート番号で振り分ける際の閾値
uint16_t t_l[WAON];			// 音程に対応したパルス幅を格納
uint16_t c,  num[WAON];		// タイマ割り込み内で使用
PROGMEM const uint16_t table[] = {12740, 12025, 11350, 10713, 10111, 9544, 9008, 8503, 8025, 7575, 7150, 6748, 6369, 6012, 5674, 5356, 5055, 4771, 4504, 4251, 4012, 3787, 3574, 3374, 3184, 3005, 2837, 2677, 2527, 2385, 2251, 2125, 2006, 1893, 1787, 1686, 1592, 1502, 1418, 1338, 1263, 1192, 1125, 1062, 1002, 946, 893, 843, 795, 751, 708, 669, 631, 596, 562, 530, 501, 472, 446, 421, 397, 375, 354, 334, 315, 297, 281, 265, 250, 236, 222, 210, 198, 187, 176, 166, 157, 148, 140, 132, 124, 117, 111, 104, 99, 93, 88, 83, 78, 74, 69, 65, 62, 58, 55, 52, 49, 46, 43, 41, 39, 36, 34, 32, 30, 29, 27, 25, 24};

// リングバッファ用変数
uint8_t RxBuf[BUFSIZE];		// バッファ
uint8_t Pdata;					// ポインタ

// delay関数(アバウト)
void delay_ms(uint16_t time){
	volatile uint16_t lp1,lp2;
	
	for(lp2=0; lp2 < time;lp2++){
		for(lp1=0; lp1 < 1050; lp1++){}
	}
}

// 矩形波再生関数(ノート番号, 出力ピン(0-2))
void sound_play(uint8_t note, uint8_t pin)
{
	uint8_t i;
	
	if(note >= 0 && note <= 108){					// 発音可能範囲のみ(0-108)
		for(i=0; i<6; i++){						// 音源1から順に発音中か確認して発音していないならそれを使用する
			if(s_playing[i] == 0x80){				// 発音していないか？
				t_l[i] = pgm_read_word(&table[note]);	// 音程に対応したパルス幅を格納
				num[i] = c + t_l[i];				// 初期値計算
				s_playing[i] = note;
				switch(pin){
					case 0: out_port[i] = 0b11111110; out_flg[0]++; break;
					case 1: out_port[i] = 0b11111101; out_flg[1]++; break;
					case 2: out_port[i] = 0b11111011; out_flg[2]++; break;
					default: out_port[i] = 0xff; break;
				}
				return;							// 関数から抜ける
			}
		}
		s_playing[WAON]++;						// 6個の音源をすべて使いきった（音源不足カウンタをインクリメント）
	}		
}

// 矩形波停止関数(ノート番号, 出力ピン(0-2))
void sound_stop(uint8_t note, uint8_t pin)
{
	uint8_t prt, i;
	
	switch(pin){
		case 0: prt = 0b11111110; break;
		case 1: prt = 0b11111101; break;
		case 2: prt = 0b11111011; break;
		default: prt = 0xff; break;
	}
	
	for(i=0; i<6; i++){							// 音源1から順にノートオフに当たる音源を探し、見つかれば発音停止
		if(s_playing[i] == note && out_port[i] == prt){		// 探しているノート番号でかつポートも同じか？
			s_playing[i] = 0x80;					// 無音に設定(0x80は無音)
			switch(pin){
				case 0: out_flg[0]--; break;
				case 1: out_flg[1]--; break;
				case 2: out_flg[2]--; break;
				default: break;
			}
			return;								// 関数から抜ける
		}
	}
	s_playing[WAON]=0;							// 音源不足カウンタをデクリメント
}

// UART受信割り込み
ISR(USART_RX_vect)
{
	// リングバッファに保存
	Pdata++;
	Pdata &= (BUFSIZE - 1);
	RxBuf[Pdata] = UDR;
}

// タイマ0比較一致A割り込み(9.6usごとに呼ばれる)
ISR(TIMER0_COMPA_vect)
{
	PORTB = out;						// 出力ポート状態更新
	c++;								// 16bit変数カウントアップ
	out = 0xff;						// Hiに戻す
	
	if(s_playing[0] != 0x80){			// 音源1
		if(c == num[0]){
			num[0] += t_l[0];
			out &= out_port[0];		// 対応ポートをLoにする
		}
	}

	if(s_playing[1] != 0x80){			// 音源2
		if(c == num[1]){
			num[1] += t_l[1];
			out &= out_port[1];
		}
	}
	
	if(s_playing[2] != 0x80){			// 音源3
		if(c == num[2]){
			num[2] += t_l[2];
			out &= out_port[2];
		}
	}
	
	if(s_playing[3] != 0x80){			// 音源4
		if(c == num[3]){
			num[3] += t_l[3];
			out &= out_port[3];
		}
	}
	
	if(s_playing[4] != 0x80){			// 音源5
		if(c == num[4]){
			num[4] += t_l[4];
			out &= out_port[4];
		}
	}
	
	if(s_playing[5] != 0x80){			// 音源6
		if(c == num[5]){
			num[5] += t_l[5];
			out &= out_port[5];
		}
	}
}

// USART設定
void Usart_init(void)
{
	UBRRH = 0x00;
	UBRRL = 0x27;
	
	UCSRB = 0b10010000;		// 受信割り込み許可/受信許可
	UCSRC = 0b00000110;		// 非同期動作/パリティ無し/ストップ1bit/データ8bit
}

void main(void)
{
	uint8_t status, byte_1, byte_cnt, RxByte, Pout, mode, i, mode4_cnt = 0;
	
	// IOポート設定
	DDRA = 0x00;				// ポートAは入力
	DDRB = 0xff;				// ポートBは出力
	DDRD = 0b01011100;		// ポートDの2,3,4,6だけ出力
	PORTA = PORTD = 0;			// ポート状態初期化
	PORTB = 0b00000111;
	
	for(i=0; i < WAON; i++){	// 配列初期化
		s_playing[i] = 0x80;
	}
	s_playing[WAON] = 0x00;		// 音源不足カウント用
	
	// モード判別
	switch(PIND & 0x22){
		case 0x00: mode = 1; break;	// チャンネル区別再生
		case 0x02: mode = 3; break;	// 設定ノート番号で別れる
		case 0x20: mode = 2; break;	// チャンネルを区別せずに再生
		case 0x22: mode = 4; PORTD = PIND | 0b01000000;	break;	// 機能設定モード(LED4を点灯させる)
		default: mode = 0; break;
	}
	
	// EEPROMから設定値読み出し
	s_l = eeprom_read_byte(0);
	s_h = eeprom_read_byte(1);
	
	// タイマ0設定
	TCCR0A = 0b00000010;		// CTC動作
	TCCR0B = 0b00000011;		// Clk/64
	OCR0A = 2;					// カウンタトップ値(割り込み周期 9.6us) [OCR0A = (9.6us/3.2us)-1 = 2]
	TIMSK = 0b00000001;		// 比較A割り込み許可
	
	Pdata = Pout = 0;			// バッファ初期化
	Usart_init();				// Usart初期化
	sei();						// 全割り込み許可
	
	///////////// メインループ //////////////
	while(1) {
		// 音源使用確認LED点灯処理
		if(mode != 4){
			if(s_playing[WAON] != 0){
				PORTD = PIND | 0b01000000;	// 音源不足(LED4)
			} else {
				PORTD = PIND & 0b10111111;
			}
			if(out_flg[0] != 0){
				PORTD = PIND | 0b00000100;	// ch1出力中(LED1)
			} else {
				PORTD = PIND & 0b11111011;
			}		
			if(out_flg[1] != 0){
				PORTD = PIND | 0b00001000;	// ch2出力中(LED2)
			} else {
				PORTD = PIND & 0b11110111;
			}
			if(out_flg[2] != 0){
				PORTD = PIND | 0b00010000;	// ch3出力中(LED3)
			} else {
				PORTD = PIND & 0b11101111;
			}
		}		
		
		if(Pdata == Pout){
			continue;			// バッファが空のとき
		}
		
		// リングバッファから1バイト取り出し
		Pout++;
		Pout &= (BUFSIZE - 1);
		RxByte = RxBuf[Pout];
		
		// データ解析
		if(RxByte & 0x80){		// ステータスバイト(MSB = 1)
			status = RxByte;
			byte_cnt = 1;
			continue;
		}
		
		if(byte_cnt == 1){		// 第一データバイト
			byte_1 = RxByte;
			byte_cnt = 2;
			continue;
		}
		
		if(byte_cnt == 2){					// 第二データバイト
			if(status >> 4 == 0x09){		// ノートオンイベント
				if(RxByte == 0){			// ベロシティが0(ノートオフ)
					if(mode == 1){
						if((status & 0x0f) <= 2){
							sound_stop(byte_1, status & 0x0f);
						}
					} else if(mode == 2){
						sound_stop(byte_1, 0);
					} else if(mode == 3){
						if(byte_1 < s_l){
							sound_stop(byte_1, 0);
						} else if(s_l <= byte_1 && byte_1 < s_h){
							sound_stop(byte_1, 1);
						} else if(s_h <= byte_1){
							sound_stop(byte_1, 2);
						}
					}						
				} else {
					if(mode == 1){
						if((status & 0x0f) <= 2){
							sound_play(byte_1, status & 0x0f);
						}
					} else if(mode == 2){
						sound_play(byte_1, 0);
					} else if(mode == 3){
						if(byte_1 < s_l){
							sound_play(byte_1, 0);
						} else if(s_l <= byte_1 && byte_1 < s_h){
							sound_play(byte_1, 1);
						} else if(s_h <= byte_1){
							sound_play(byte_1, 2);
						}
					} else if(mode == 4){
						if(mode4_cnt == 0){
							if(byte_1 == 69){
								PORTD = PIND | 0b00000100;	// LED1点灯
								mode4_cnt = 1;
							}
						}else if(mode4_cnt == 1){
							PORTD = PIND | 0b00001000;		// LED2点灯
							s_l = byte_1;
							mode4_cnt = 2;
						} else if(mode4_cnt == 2){
							s_h = byte_1;
							if(s_l < s_h){					// 設定条件OK
								eeprom_write_byte(0, s_l);	// EEPROMに保存
								eeprom_write_byte(1, s_h);
								while(1){
									PORTD = PIND | 0b00010000;		// LED3点灯
									delay_ms(100);
									PORTD = PIND & 0b11101111;		// LED3消灯
									delay_ms(100);
								}								
							} else {						// 設定条件NG
								PORTD = PIND & 0b11110111;			// LED2消灯
								mode4_cnt = 1;
							}
						}
					}
				}
			}else if(status >> 4 == 0x08){				// ノートオフイベント
				if(mode == 1){
					if((status & 0x0f) <= 2){
						sound_stop(byte_1, status & 0x0f);
					}
				} else if(mode == 2){
					sound_stop(byte_1, 0);
				} else if(mode == 3){
					if(byte_1 < s_l){
						sound_stop(byte_1, 0);
					} else if(s_l <= byte_1 && byte_1 < s_h){
						sound_stop(byte_1, 1);
					} else if(s_h <= byte_1){
						sound_stop(byte_1, 2);
					}
				}
			}
			byte_cnt = 1;
		}
	}
}
