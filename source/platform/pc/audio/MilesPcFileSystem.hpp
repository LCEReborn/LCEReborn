#ifndef PLATFORM_PC_AUDIO_MILESPCFILESYSTEM_HPP
#define PLATFORM_PC_AUDIO_MILESPCFILESYSTEM_HPP

unsigned int file_open_callback(const char* filename, unsigned int* handleOut);
void file_close_callback(unsigned int handle);
int file_seek_callback(unsigned int handle, int offset, unsigned int origin);
int file_read_callback(unsigned int handle, void* buffer, unsigned int bytes);

#endif
