/*
 * player.h
 *
 *  Created on: 2015-06-27
 *      Author: egorchak
 */

#ifndef PLAYER_H_
#define PLAYER_H_

#include "fat.h"

#define PLAYBACK_MODE_NORMAL 0
#define PLAYBACK_MODE_DOUBLE 1
#define PLAYBACK_MODE_HALF 2
#define PLAYBACK_MODE_DELAY 3
#define PLAYBACK_MODE_REVERSE 4

#define PLAYER_STATE_PLAYING 0
#define PLAYER_STATE_USER_INPUT 1
#define PLAYER_STATE_DEFAULT PLAYER_STATE_USER_INPUT

#define PLAYER_MODE_NORMAL 0
#define PLAYER_MODE_DOUBLE 1
#define PLAYER_MODE_HALF 2
#define PLAYER_MODE_DELAY 3
#define PLAYER_MODE_REVERSE 4

#define BUFFER_SIZE 512
#define MAX_FILES 15

#define BTN_FWD 0x8	
#define BTN_BWD 0x4
#define BTN_PLAY 0x2
#define BTN_STOP 0x1

#define CHANNEL_LEFT 0
#define CHANNEL_RIGHT 1

#define DELAY_SAMPLES 44100

data_file files[MAX_FILES];
BYTE buffer[BUFFER_SIZE];
int cluster_chain[5000];

volatile short player_state;
volatile short player_mode;

int init_system(void);
void play_normal(data_file*, BYTE*, int*);

#endif /* PLAYER_H_ */
