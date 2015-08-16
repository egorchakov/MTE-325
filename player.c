/*
 * player.c
 *
 *  Created on: 2015-06-27
 *      Author: egorchak
 */

#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <io.h>
#include <stdbool.h>

#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h" 
#include "system.h"
#include "SD_Card.h"
#include "fat.h"
#include "LCD.h"
#include "basic_io.h"

#include "player.h"

volatile int edge = 0;
volatile alt_u8 buttons;
volatile int cur_file_index = 0;

volatile int indexed_file_count;
volatile int interrupt_count = 0;
volatile int cur_file_index_update = 1;
volatile int player_state_update = 1;

static void button_ISR(void* context, alt_u32 id){
	
	edge ^= 1;
	buttons = IORD(BUTTON_PIO_BASE, 3) & 0xf;
	
	if (edge == 1) {
		IOWR(LED_PIO_BASE, 0, buttons);
	}
	else {
		switch (player_state) {
			
			case PLAYER_STATE_USER_INPUT:
				switch (buttons) {
					case BTN_PLAY:
						player_state = PLAYER_STATE_PLAYING;
						player_mode = IORD(SWITCH_PIO_BASE, 0);
						player_state_update = 1;
						break;
					
					case BTN_FWD:
						cur_file_index = (cur_file_index < indexed_file_count - 1)  ? (cur_file_index + 1) : 0;
						cur_file_index_update = 1;
						break;
						
					case BTN_BWD:
						cur_file_index = (cur_file_index > 0) ? (cur_file_index - 1) : (indexed_file_count - 1);
						cur_file_index_update = 1;
						break;
				}
				
			case PLAYER_STATE_PLAYING:
				switch (buttons) {
					case BTN_STOP:
						player_state = PLAYER_STATE_USER_INPUT;
						player_state_update = 1;
						break;
				}
		}
		
		IOWR(LED_PIO_BASE, 0, 0x00);
	}
	
	IOWR(BUTTON_PIO_BASE, 3, 0x0);
}


int init_system(void){
	if (SD_card_init() != 0) {
		perror("SD card initialization failed.");
		return(-1);
	}
	
	if (init_mbr() != 0) {
		perror("MBR initialization failed.");
		return(-1);
	}
	
	if (init_bs() != 0) {
		perror("Boot sector initialization failed.");
		return(-1);
	}
	
	init_audio_codec();
	LCD_Init();
	
	return(0);
}


int build_file_index(data_file files[]){
	int i = 0;
	if (!search_for_filetype("WAV", &files[i++], 0, 1) && !search_for_filetype("WAV", &files[i++], 0, 1)){
		while(file_number > 1 && !search_for_filetype("WAV", &files[i++], 0, 1));
	}
	return i-1;
}

void play_normal(data_file* file, BYTE buffer[], int cluster_chain[]){
	int isector = 0, ibyte, retval, num_bytes;
	UINT16 tmp;

	while (player_state == PLAYER_STATE_PLAYING){
		retval = get_rel_sector(file, buffer, cluster_chain, isector++);
		if (retval != -1){
			num_bytes = (retval == 0) ?  BPB_BytsPerSec : retval;

			for (ibyte = 0; (player_state == PLAYER_STATE_PLAYING) && (ibyte < num_bytes); ibyte += 2){
				tmp = (buffer[ibyte + 1] << 8) | (buffer[ibyte]);
				while(IORD(AUD_FULL_BASE, 0));
				IOWR(AUDIO_0_BASE, 0, tmp);
			}
		}
	}
}

void play_double(data_file* file, BYTE buffer[], int cluster_chain[]){
	int isector = 0, ibyte, retval, num_bytes;
	UINT16 tmp;

	while (player_state == PLAYER_STATE_PLAYING){
		retval = get_rel_sector(file, buffer, cluster_chain, isector++);

		if (retval != -1 ){
			num_bytes = (retval == 0) ?  BPB_BytsPerSec : retval;

			for (ibyte = 0; (player_state == PLAYER_STATE_PLAYING) && (ibyte < num_bytes); ibyte += 8){
				tmp = (buffer[ibyte + 1] << 8) | (buffer[ibyte]);
				while(IORD(AUD_FULL_BASE, 0));
				IOWR(AUDIO_0_BASE, 0, tmp);

				tmp = (buffer[ibyte + 3] << 8) | (buffer[ibyte + 2]);
				while(IORD(AUD_FULL_BASE, 0));
				IOWR(AUDIO_0_BASE, 0, tmp);
			}
		}
	}
}

void play_half(data_file* file, BYTE buffer[], int cluster_chain[]){
	int isector = 0, ibyte, retval, num_bytes;
	UINT16 tmp;
	while (player_state == PLAYER_STATE_PLAYING){
		retval = get_rel_sector(file, buffer, cluster_chain, isector++);

		if (retval != -1){
			num_bytes = (retval == 0) ?  BPB_BytsPerSec : retval;

			for (ibyte = 0; player_state == PLAYER_STATE_PLAYING && ibyte < num_bytes; ibyte += 2){
				tmp = (buffer[ibyte + 1] << 8) | (buffer[ibyte]);
				while(IORD(AUD_FULL_BASE, 0));
				IOWR(AUDIO_0_BASE, 0, tmp);

				tmp = (buffer[ibyte + 1] << 8) | (buffer[ibyte]);
				while(IORD(AUD_FULL_BASE, 0));
				IOWR(AUDIO_0_BASE, 0, tmp);
			}
		}
	}
}


void play_reverse(data_file* file, BYTE buffer[], int cluster_chain[]){
	int isector = ceil(file->FileSize/BPB_BytsPerSec) - 1, ibyte, retval, num_bytes;
	UINT16 tmp;

	while (player_state == PLAYER_STATE_PLAYING){
		retval = get_rel_sector(file, buffer, cluster_chain, isector--);

		if (retval != -1){
			num_bytes = (retval == 0) ?  BPB_BytsPerSec : retval;

			for (ibyte = num_bytes - 1; ibyte >= 1; ibyte -= 2){
				tmp = (buffer[ibyte] << 8) | (buffer[ibyte-1]);
				while(IORD(AUD_FULL_BASE, 0));
				IOWR(AUDIO_0_BASE, 0, tmp);
			}
		}
	}
}

void play_delay(data_file* file, BYTE buffer[], int cluster_chain[]){
	UINT16 tmp, data;
	int isector = 0, ibyte, ilagbuf = 0, channel = CHANNEL_LEFT, retval, num_bytes;
	UINT16 lag_buffer[DELAY_SAMPLES] = {0x0000};
	
	while (player_state == PLAYER_STATE_PLAYING){
		retval = get_rel_sector(file, buffer, cluster_chain, isector++);

		if (retval != -1) {
			num_bytes = (retval == 0) ?  BPB_BytsPerSec : retval;

			for (ibyte = 0; (player_state == PLAYER_STATE_PLAYING) && (ibyte < num_bytes); ibyte += 2){

				data = (buffer[ibyte + 1] << 8) | (buffer[ibyte]);

				if (channel == CHANNEL_LEFT){
					tmp = data;
				} else {
					if (ilagbuf == DELAY_SAMPLES) ilagbuf = 0;
					tmp = lag_buffer[ilagbuf];
					lag_buffer[ilagbuf++] = data;
				}

				while(IORD(AUD_FULL_BASE, 0));
				IOWR(AUDIO_0_BASE, 0, tmp);
				channel ^= 1;
			}
		}
	}

	int i;
	for (i = 0; i < DELAY_SAMPLES*2 && player_state == PLAYER_STATE_PLAYING; i++){
		if (ilagbuf == DELAY_SAMPLES) ilagbuf = 0;
		tmp = (channel == CHANNEL_LEFT) ? 0x0000 : lag_buffer[ilagbuf++];
		while(IORD(AUD_FULL_BASE, 0));
		IOWR(AUDIO_0_BASE, 0, tmp);
		channel ^=1 ;
	}
}


void play(data_file* file, BYTE buffer[], int cluster_chain[], int mode){
	switch(mode){
		case PLAYER_MODE_DOUBLE:
			return play_double(file, buffer, cluster_chain);
			
		case PLAYER_MODE_HALF:
			return play_half(file, buffer, cluster_chain);
		
		case PLAYER_MODE_REVERSE:
			return play_reverse(file, buffer, cluster_chain);
			
		case PLAYER_MODE_DELAY:
			return play_delay(file, buffer, cluster_chain);
			
		default:
			return play_normal(file, buffer, cluster_chain);
	}

}

int main(void) {

	if (init_system() != 0) return(-1);
	
	indexed_file_count = build_file_index(files);
	printf("Indexed files: %d\n", indexed_file_count);
	
	IOWR(BUTTON_PIO_BASE, 2, 0xf);
	IOWR(BUTTON_PIO_BASE, 3, 0x0);
	alt_irq_register( BUTTON_PIO_IRQ, (void*)0, button_ISR );
	
	IOWR(LED_PIO_BASE, 0, 0x00);
	
	player_state = PLAYER_STATE_DEFAULT;	
	
	int length;
	data_file file;
	
	while(1){
		if ((cur_file_index_update == 1) || (player_state_update == 1)) {

			if (cur_file_index_update)
				file = files[cur_file_index];

			switch (player_state){

				case PLAYER_STATE_PLAYING:
					LCD_File_Buffering(file.Name);
					length = 1 + ceil(file.FileSize/(BPB_BytsPerSec*BPB_SecPerClus));
					build_cluster_chain(cluster_chain, length, &file);

					LCD_Display(file.Name, player_mode);
					play(&file, buffer, cluster_chain, player_mode);
					player_state = PLAYER_STATE_USER_INPUT;

				case PLAYER_STATE_USER_INPUT:
					LCD_Display(file.Name, player_mode);
			}

			player_state_update = 0;
			cur_file_index_update = 0;
		}
	}
	
	return(0);
}
