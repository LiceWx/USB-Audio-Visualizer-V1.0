/**
  ******************************************************************************
  * @file    audio_bridge.h
  * @brief   PC→Headset audio bridge: shared declarations.
  *          This is a custom file — CubeMX will NEVER touch it.
  ******************************************************************************
  */
#ifndef __AUDIO_BRIDGE_H__
#define __AUDIO_BRIDGE_H__

#include "usbh_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_FRAME_SIZE     192U
#define BRIDGE_BUF_SIZE      19200U

extern uint8_t  bridge_buffer[BRIDGE_BUF_SIZE];
extern volatile uint16_t bridge_wr;
extern volatile uint16_t bridge_rd;

/* Debug counters */
extern volatile uint32_t dbg_pc_packets;   /* PC→STM32 audio OUT packets */
extern volatile uint32_t dbg_hp_frames;    /* STM32→headset audio frames sent */
extern volatile uint8_t  dbg_hp_connected; /* 1 = headset class active */
extern volatile uint8_t  dbg_audio_init;   /* 1 = AUDIO_Init_FS called */
extern volatile uint8_t  dbg_audio_start;  /* 1 = AUDIO_CMD_START received */

void AudioIsochPump(USBH_HandleTypeDef *phost);

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_BRIDGE_H__ */
