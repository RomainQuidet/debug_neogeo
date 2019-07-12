//
//  main.c
//  neogeo_front_debug
//
//  Created by Romain Quidet on 30/06/2019.
//  Copyright © 2019 Romain Quidet. All rights reserved.
//

#include <stdio.h>
#include <SDL2/SDL.h>

#include "libretro.h"

Uint32 frame_delay = (1000 / 60);	// ms
Uint32 frame_callback(Uint32 interval, void *param) {
	retro_run();
	return frame_delay;
}

#pragma mark libretro callbacks

void retro_logger(enum retro_log_level level, const char *fmt, ...) {
#if DEBUG
	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
#endif
}

bool set_environment(unsigned cmd, void *data) {
	switch (cmd) {
		case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
			((struct retro_log_callback *)data)->log = &retro_logger;
			break;
		case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
		{
			char *path = "/Users/romainquidet/Documents/RetroArch/system";
			size_t len = strlen(path);
			char *res = malloc(len + 1);
			strcpy(res, path);
			char **tmp = (char **)data;
			*tmp = res;
		}
			break;
		case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
			//nop
			break;
		default:
			return false;
			break;
	}
	return true;
}

size_t buffer_pitch;
uint8_t *buffer;
bool buffer_updated = false;
void video_refresh(const void *data, unsigned width,
				   unsigned height, size_t pitch) {
	buffer_pitch = pitch;
	memcpy(buffer, data, sizeof(uint16_t) * width * height);
	buffer_updated = true;
}

SDL_AudioDeviceID dev;
size_t audio_sample_batch(const int16_t *data, size_t frames) {
	int err = SDL_QueueAudio(dev, data, (uint32_t)frames * 4);
	if (err != 0) {
		SDL_Log("Failed to queue audio: %s", SDL_GetError());
	}
	return 0;
}

void input_poll() {
	
}

int16_t input_state(unsigned port, unsigned device,
					 unsigned index, unsigned id) {
	return 0;
}

void draw_mire(const uint16_t *frameBuffer, uint16_t width, uint16_t height) {
	// test to draw a mire on screen, format is RGB565
	uint16_t white = 0xFFFF;
	uint16_t black = 0x0000;
	uint16_t red = 0xF800;
	uint16_t green = 0x07E0;
	uint16_t blue = 0x001F;
	
	if (width % 5 != 0) {
		printf("draw_mire width must be / 5 !\n");
		return;
	}
	
	uint16_t same_pixels = width / 5;
	uint16_t *pixelsBuffer = (uint16_t *)frameBuffer;
	for (uint16_t scanline = 0; scanline < height; scanline++) {
		for (uint8_t i = 0; i < same_pixels; i++) {
			*pixelsBuffer = white;
			pixelsBuffer++;
		}
		for (uint8_t i = 0; i < same_pixels; i++) {
			*pixelsBuffer = black;
			pixelsBuffer++;
		}
		for (uint8_t i = 0; i < same_pixels; i++) {
			*pixelsBuffer = red;
			pixelsBuffer++;
		}
		for (uint8_t i = 0; i < same_pixels; i++) {
			*pixelsBuffer = green;
			pixelsBuffer++;
		}
		for (uint8_t i = 0; i < same_pixels; i++) {
			*pixelsBuffer = blue;
			pixelsBuffer++;
		}
	}
}

#pragma mark - main

int main(int argc, const char * argv[]) {
	
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	SDL_Event event;
	
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
		return 3;
	}
	
#pragma mark - Retro startup
	
	uint retro_version = retro_api_version();
	if (retro_version != 1) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize retro lib: version is %u", retro_version);
		return 1;
	}
	
	retro_set_environment(&set_environment);
	
	retro_init();
	
	retro_set_video_refresh(&video_refresh);
	//	retro_set_audio_sample(retro_audio_sample_t);
	retro_set_audio_sample_batch(&audio_sample_batch);
	retro_set_input_poll(&input_poll);
	retro_set_input_state(&input_state);
	
	struct retro_system_info system_info;
	retro_get_system_info(&system_info);
	
	struct retro_system_av_info system_av_info;
	retro_get_system_av_info(&system_av_info);
	
	int width = system_av_info.geometry.max_width;
	int height = system_av_info.geometry.max_height;
	
	buffer = malloc(sizeof(uint16_t) * width * height);
	buffer_pitch = width * sizeof(uint16_t);
	draw_mire((uint16_t *)buffer, width, height);
	buffer_updated = true;
	
	const char *game_path = argv[1];
	struct retro_game_info game_info;
	game_info.path = game_path;
	
	bool res = retro_load_game(&game_info);
	if (res == false) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "no game");
		return 1;
	}
	
#pragma mark - SDL init
	
	if (SDL_CreateWindowAndRenderer(width * 3, height * 3, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window and renderer: %s", SDL_GetError());
		return 3;
	}
	
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STATIC, width, height);
	if (!texture) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture: %s", SDL_GetError());
		return 3;
	}
	
	int audiocount = SDL_GetNumAudioDevices(0);
	for (int i = 0; i < audiocount; i++) {
		printf("Audio device %d: %s\n", i, SDL_GetAudioDeviceName(i, 0));
	}
	
	SDL_AudioSpec want, have;
	SDL_memset(&want, 0, sizeof(want));
	want.freq = 44100;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 4096;
	want.callback = NULL;
	
	dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (dev == 0) {
		SDL_Log("Failed to open audio: %s", SDL_GetError());
	} else {
		if (have.format != want.format) { /* we let this one thing change. */
			SDL_Log("We didn't get requested audio format.");
		}
		SDL_PauseAudioDevice(dev, 0);
	}
	
	Uint32 first_launch = 2000;
	SDL_TimerID my_timer_id = SDL_AddTimer(first_launch, frame_callback, NULL);
	
	while (1) {
		SDL_PollEvent(&event);
		if (event.type == SDL_QUIT) {
			break;
		}
		if (buffer_updated) {
			SDL_UpdateTexture(texture, NULL, buffer, (int)buffer_pitch);
			buffer_updated = false;
			SDL_RenderClear(renderer);
			SDL_RenderCopy(renderer, texture, NULL, NULL);
			SDL_RenderPresent(renderer);
		}
		SDL_Delay(20);
	}
	
	SDL_CloseAudioDevice(dev);
	SDL_RemoveTimer(my_timer_id);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	
	SDL_Quit();
	
	return 0;
}
