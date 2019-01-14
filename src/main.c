/*
 * ATtiny2313�g�p DRSSTC����p 6�a��MIDI�f�R�[�_ ver2.1
 *
 * ����J�n: 2013/05/03
 * �ŏI�X�V: 2013/07/07
 * ����� : kingyo
 *
 * �N���b�N: 20MHz
 * �����\�m�[�g�ԍ�: 0-108
 *
 * MIDI���b�Z�[�W(�Q�l)
 * MIDI_NoteOff			(0x8x)
 * MIDI_NoteOn				(0x9x)
 * MIDI_ControlChange		(0xBx)	// ���g�p
 * MIDI_ProgramChange		(0xCx)	// ���g�p
 * MIDI_ChannelPressure	(0xDx)	// ���g�p
 * MIDI_PitchBend			(0xEx)	// ���g�p
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>	// ���荞��
#include <avr/pgmspace.h>	// �萔���t���b�V���ɔz�u����ׂɕK�v
#include <avr/eeprom.h>		// EEPROM�g�p�̂���

// ��`�l
#define BUFSIZE	16			// �o�b�t�@�T�C�Y 2^n�o�C�g
#define WAON		6			// �a����(�ύX���Ȃ��ŁI)

// �O���[�o���ϐ�
uint8_t out = 0x07;			// out�����l
uint8_t s_playing[WAON+1];	// 6�a�� + �����s�����m�p
uint8_t out_port[6];			// �o�̓s���w��p
uint8_t out_flg[3] = {0, 0, 0};
uint8_t s_l, s_h;				// �m�[�g�ԍ��ŐU�蕪����ۂ�臒l
uint16_t t_l[WAON];			// �����ɑΉ������p���X�����i�[
uint16_t c,  num[WAON];		// �^�C�}���荞�ݓ��Ŏg�p
PROGMEM const uint16_t table[] = {12740, 12025, 11350, 10713, 10111, 9544, 9008, 8503, 8025, 7575, 7150, 6748, 6369, 6012, 5674, 5356, 5055, 4771, 4504, 4251, 4012, 3787, 3574, 3374, 3184, 3005, 2837, 2677, 2527, 2385, 2251, 2125, 2006, 1893, 1787, 1686, 1592, 1502, 1418, 1338, 1263, 1192, 1125, 1062, 1002, 946, 893, 843, 795, 751, 708, 669, 631, 596, 562, 530, 501, 472, 446, 421, 397, 375, 354, 334, 315, 297, 281, 265, 250, 236, 222, 210, 198, 187, 176, 166, 157, 148, 140, 132, 124, 117, 111, 104, 99, 93, 88, 83, 78, 74, 69, 65, 62, 58, 55, 52, 49, 46, 43, 41, 39, 36, 34, 32, 30, 29, 27, 25, 24};

// �����O�o�b�t�@�p�ϐ�
uint8_t RxBuf[BUFSIZE];		// �o�b�t�@
uint8_t Pdata;					// �|�C���^

// delay�֐�(�A�o�E�g)
void delay_ms(uint16_t time){
	volatile uint16_t lp1,lp2;
	
	for(lp2=0; lp2 < time;lp2++){
		for(lp1=0; lp1 < 1050; lp1++){}
	}
}

// ��`�g�Đ��֐�(�m�[�g�ԍ�, �o�̓s��(0-2))
void sound_play(uint8_t note, uint8_t pin)
{
	uint8_t i;
	
	if(note >= 0 && note <= 108){					// �����\�͈͂̂�(0-108)
		for(i=0; i<6; i++){						// ����1���珇�ɔ��������m�F���Ĕ������Ă��Ȃ��Ȃ炻����g�p����
			if(s_playing[i] == 0x80){				// �������Ă��Ȃ����H
				t_l[i] = pgm_read_word(&table[note]);	// �����ɑΉ������p���X�����i�[
				num[i] = c + t_l[i];				// �����l�v�Z
				s_playing[i] = note;
				switch(pin){
					case 0: out_port[i] = 0b11111110; out_flg[0]++; break;
					case 1: out_port[i] = 0b11111101; out_flg[1]++; break;
					case 2: out_port[i] = 0b11111011; out_flg[2]++; break;
					default: out_port[i] = 0xff; break;
				}
				return;							// �֐����甲����
			}
		}
		s_playing[WAON]++;						// 6�̉��������ׂĎg���������i�����s���J�E���^���C���N�������g�j
	}		
}

// ��`�g��~�֐�(�m�[�g�ԍ�, �o�̓s��(0-2))
void sound_stop(uint8_t note, uint8_t pin)
{
	uint8_t prt, i;
	
	switch(pin){
		case 0: prt = 0b11111110; break;
		case 1: prt = 0b11111101; break;
		case 2: prt = 0b11111011; break;
		default: prt = 0xff; break;
	}
	
	for(i=0; i<6; i++){							// ����1���珇�Ƀm�[�g�I�t�ɓ����鉹����T���A������Δ�����~
		if(s_playing[i] == note && out_port[i] == prt){		// �T���Ă���m�[�g�ԍ��ł��|�[�g���������H
			s_playing[i] = 0x80;					// �����ɐݒ�(0x80�͖���)
			switch(pin){
				case 0: out_flg[0]--; break;
				case 1: out_flg[1]--; break;
				case 2: out_flg[2]--; break;
				default: break;
			}
			return;								// �֐����甲����
		}
	}
	s_playing[WAON]=0;							// �����s���J�E���^���f�N�������g
}

// UART��M���荞��
ISR(USART_RX_vect)
{
	// �����O�o�b�t�@�ɕۑ�
	Pdata++;
	Pdata &= (BUFSIZE - 1);
	RxBuf[Pdata] = UDR;
}

// �^�C�}0��r��vA���荞��(9.6us���ƂɌĂ΂��)
ISR(TIMER0_COMPA_vect)
{
	PORTB = out;						// �o�̓|�[�g��ԍX�V
	c++;								// 16bit�ϐ��J�E���g�A�b�v
	out = 0xff;						// Hi�ɖ߂�
	
	if(s_playing[0] != 0x80){			// ����1
		if(c == num[0]){
			num[0] += t_l[0];
			out &= out_port[0];		// �Ή��|�[�g��Lo�ɂ���
		}
	}

	if(s_playing[1] != 0x80){			// ����2
		if(c == num[1]){
			num[1] += t_l[1];
			out &= out_port[1];
		}
	}
	
	if(s_playing[2] != 0x80){			// ����3
		if(c == num[2]){
			num[2] += t_l[2];
			out &= out_port[2];
		}
	}
	
	if(s_playing[3] != 0x80){			// ����4
		if(c == num[3]){
			num[3] += t_l[3];
			out &= out_port[3];
		}
	}
	
	if(s_playing[4] != 0x80){			// ����5
		if(c == num[4]){
			num[4] += t_l[4];
			out &= out_port[4];
		}
	}
	
	if(s_playing[5] != 0x80){			// ����6
		if(c == num[5]){
			num[5] += t_l[5];
			out &= out_port[5];
		}
	}
}

// USART�ݒ�
void Usart_init(void)
{
	UBRRH = 0x00;
	UBRRL = 0x27;
	
	UCSRB = 0b10010000;		// ��M���荞�݋���/��M����
	UCSRC = 0b00000110;		// �񓯊�����/�p���e�B����/�X�g�b�v1bit/�f�[�^8bit
}

void main(void)
{
	uint8_t status, byte_1, byte_cnt, RxByte, Pout, mode, i, mode4_cnt = 0;
	
	// IO�|�[�g�ݒ�
	DDRA = 0x00;				// �|�[�gA�͓���
	DDRB = 0xff;				// �|�[�gB�͏o��
	DDRD = 0b01011100;		// �|�[�gD��2,3,4,6�����o��
	PORTA = PORTD = 0;			// �|�[�g��ԏ�����
	PORTB = 0b00000111;
	
	for(i=0; i < WAON; i++){	// �z�񏉊���
		s_playing[i] = 0x80;
	}
	s_playing[WAON] = 0x00;		// �����s���J�E���g�p
	
	// ���[�h����
	switch(PIND & 0x22){
		case 0x00: mode = 1; break;	// �`�����l����ʍĐ�
		case 0x02: mode = 3; break;	// �ݒ�m�[�g�ԍ��ŕʂ��
		case 0x20: mode = 2; break;	// �`�����l������ʂ����ɍĐ�
		case 0x22: mode = 4; PORTD = PIND | 0b01000000;	break;	// �@�\�ݒ胂�[�h(LED4��_��������)
		default: mode = 0; break;
	}
	
	// EEPROM����ݒ�l�ǂݏo��
	s_l = eeprom_read_byte(0);
	s_h = eeprom_read_byte(1);
	
	// �^�C�}0�ݒ�
	TCCR0A = 0b00000010;		// CTC����
	TCCR0B = 0b00000011;		// Clk/64
	OCR0A = 2;					// �J�E���^�g�b�v�l(���荞�ݎ��� 9.6us) [OCR0A = (9.6us/3.2us)-1 = 2]
	TIMSK = 0b00000001;		// ��rA���荞�݋���
	
	Pdata = Pout = 0;			// �o�b�t�@������
	Usart_init();				// Usart������
	sei();						// �S���荞�݋���
	
	///////////// ���C�����[�v //////////////
	while(1) {
		// �����g�p�m�FLED�_������
		if(mode != 4){
			if(s_playing[WAON] != 0){
				PORTD = PIND | 0b01000000;	// �����s��(LED4)
			} else {
				PORTD = PIND & 0b10111111;
			}
			if(out_flg[0] != 0){
				PORTD = PIND | 0b00000100;	// ch1�o�͒�(LED1)
			} else {
				PORTD = PIND & 0b11111011;
			}		
			if(out_flg[1] != 0){
				PORTD = PIND | 0b00001000;	// ch2�o�͒�(LED2)
			} else {
				PORTD = PIND & 0b11110111;
			}
			if(out_flg[2] != 0){
				PORTD = PIND | 0b00010000;	// ch3�o�͒�(LED3)
			} else {
				PORTD = PIND & 0b11101111;
			}
		}		
		
		if(Pdata == Pout){
			continue;			// �o�b�t�@����̂Ƃ�
		}
		
		// �����O�o�b�t�@����1�o�C�g���o��
		Pout++;
		Pout &= (BUFSIZE - 1);
		RxByte = RxBuf[Pout];
		
		// �f�[�^���
		if(RxByte & 0x80){		// �X�e�[�^�X�o�C�g(MSB = 1)
			status = RxByte;
			byte_cnt = 1;
			continue;
		}
		
		if(byte_cnt == 1){		// ���f�[�^�o�C�g
			byte_1 = RxByte;
			byte_cnt = 2;
			continue;
		}
		
		if(byte_cnt == 2){					// ���f�[�^�o�C�g
			if(status >> 4 == 0x09){		// �m�[�g�I���C�x���g
				if(RxByte == 0){			// �x���V�e�B��0(�m�[�g�I�t)
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
								PORTD = PIND | 0b00000100;	// LED1�_��
								mode4_cnt = 1;
							}
						}else if(mode4_cnt == 1){
							PORTD = PIND | 0b00001000;		// LED2�_��
							s_l = byte_1;
							mode4_cnt = 2;
						} else if(mode4_cnt == 2){
							s_h = byte_1;
							if(s_l < s_h){					// �ݒ����OK
								eeprom_write_byte(0, s_l);	// EEPROM�ɕۑ�
								eeprom_write_byte(1, s_h);
								while(1){
									PORTD = PIND | 0b00010000;		// LED3�_��
									delay_ms(100);
									PORTD = PIND & 0b11101111;		// LED3����
									delay_ms(100);
								}								
							} else {						// �ݒ����NG
								PORTD = PIND & 0b11110111;			// LED2����
								mode4_cnt = 1;
							}
						}
					}
				}
			}else if(status >> 4 == 0x08){				// �m�[�g�I�t�C�x���g
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
