#ifndef PLATFORM_PC_PCAVATAR_HPP
#define PLATFORM_PC_PCAVATAR_HPP

class BufferedImage;

BufferedImage* PcRenderAvatarHead(int quadrant, int size);
BufferedImage* PcRenderSkinHead(unsigned int skinId, int defaultIndex, int size);

#endif
