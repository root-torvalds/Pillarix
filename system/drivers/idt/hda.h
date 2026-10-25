#pragma once
#include <stdint.h>

void init_intel_hda(void);
void hda_init_corb_rirb(volatile uint32_t* hda_regs);
void hda_init_codec(volatile uint32_t* hda_regs);
void hda_start_playback(volatile uint32_t* hda_regs);
