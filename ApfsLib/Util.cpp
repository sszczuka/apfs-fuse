/*
	This file is part of apfs-fuse, a read-only implementation of APFS
	(Apple File System) for FUSE.
	Copyright (C) 2017 Simon Gander

	Apfs-fuse is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	Apfs-fuse is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with apfs-fuse.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef __linux__
#include <unicode/utypes.h>
#include <unicode/normalizer2.h>
#else
#include "Unicode.h"
#endif

#include <iostream>
#include <iomanip>
#include <vector>
#include <termios.h>
#include <stdio.h>

#include "Util.h"
#include "Crc32.h"

static const uint16_t lowercase_table[0x500] =
{
	// 0000
	0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F,
	0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F,
	0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
	0x0040, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
	0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
	0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F,
	// 0080
	0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087, 0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F,
	0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, 0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F,
	0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
	0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x03BC, 0x00B6, 0x00B7, 0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
	0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
	0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00D7, 0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00DF,
	0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
	0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF,
	// 0100
	0x0101, 0x0101, 0x0103, 0x0103, 0x0105, 0x0105, 0x0107, 0x0107, 0x0109, 0x0109, 0x010B, 0x010B, 0x010D, 0x010D, 0x010F, 0x010F,
	0x0111, 0x0111, 0x0113, 0x0113, 0x0115, 0x0115, 0x0117, 0x0117, 0x0119, 0x0119, 0x011B, 0x011B, 0x011D, 0x011D, 0x011F, 0x011F,
	0x0121, 0x0121, 0x0123, 0x0123, 0x0125, 0x0125, 0x0127, 0x0127, 0x0129, 0x0129, 0x012B, 0x012B, 0x012D, 0x012D, 0x012F, 0x012F,
	0x0130, 0x0131, 0x0133, 0x0133, 0x0135, 0x0135, 0x0137, 0x0137, 0x0138, 0x013A, 0x013A, 0x013C, 0x013C, 0x013E, 0x013E, 0x0140,
	0x0140, 0x0142, 0x0142, 0x0144, 0x0144, 0x0146, 0x0146, 0x0148, 0x0148, 0x0149, 0x014B, 0x014B, 0x014D, 0x014D, 0x014F, 0x014F,
	0x0151, 0x0151, 0x0153, 0x0153, 0x0155, 0x0155, 0x0157, 0x0157, 0x0159, 0x0159, 0x015B, 0x015B, 0x015D, 0x015D, 0x015F, 0x015F,
	0x0161, 0x0161, 0x0163, 0x0163, 0x0165, 0x0165, 0x0167, 0x0167, 0x0169, 0x0169, 0x016B, 0x016B, 0x016D, 0x016D, 0x016F, 0x016F,
	0x0171, 0x0171, 0x0173, 0x0173, 0x0175, 0x0175, 0x0177, 0x0177, 0x00FF, 0x017A, 0x017A, 0x017C, 0x017C, 0x017E, 0x017E, 0x0073,
	// 0180
	0x0180, 0x0253, 0x0183, 0x0183, 0x0185, 0x0185, 0x0254, 0x0188, 0x0188, 0x0256, 0x0257, 0x018C, 0x018C, 0x018D, 0x01DD, 0x0259,
	0x025B, 0x0192, 0x0192, 0x0260, 0x0263, 0x0195, 0x0269, 0x0268, 0x0199, 0x0199, 0x019A, 0x019B, 0x026F, 0x0272, 0x019E, 0x0275,
	0x01A1, 0x01A1, 0x01A3, 0x01A3, 0x01A5, 0x01A5, 0x0280, 0x01A8, 0x01A8, 0x0283, 0x01AA, 0x01AB, 0x01AD, 0x01AD, 0x0288, 0x01B0,
	0x01B0, 0x028A, 0x028B, 0x01B4, 0x01B4, 0x01B6, 0x01B6, 0x0292, 0x01B9, 0x01B9, 0x01BA, 0x01BB, 0x01BD, 0x01BD, 0x01BE, 0x01BF,
	0x01C0, 0x01C1, 0x01C2, 0x01C3, 0x01C6, 0x01C6, 0x01C6, 0x01C9, 0x01C9, 0x01C9, 0x01CC, 0x01CC, 0x01CC, 0x01CE, 0x01CE, 0x01D0,
	0x01D0, 0x01D2, 0x01D2, 0x01D4, 0x01D4, 0x01D6, 0x01D6, 0x01D8, 0x01D8, 0x01DA, 0x01DA, 0x01DC, 0x01DC, 0x01DD, 0x01DF, 0x01DF,
	0x01E1, 0x01E1, 0x01E3, 0x01E3, 0x01E5, 0x01E5, 0x01E7, 0x01E7, 0x01E9, 0x01E9, 0x01EB, 0x01EB, 0x01ED, 0x01ED, 0x01EF, 0x01EF,
	0x01F0, 0x01F3, 0x01F3, 0x01F3, 0x01F5, 0x01F5, 0x0195, 0x01BF, 0x01F9, 0x01F9, 0x01FB, 0x01FB, 0x01FD, 0x01FD, 0x01FF, 0x01FF,
	// 0200
	0x0201, 0x0201, 0x0203, 0x0203, 0x0205, 0x0205, 0x0207, 0x0207, 0x0209, 0x0209, 0x020B, 0x020B, 0x020D, 0x020D, 0x020F, 0x020F,
	0x0211, 0x0211, 0x0213, 0x0213, 0x0215, 0x0215, 0x0217, 0x0217, 0x0219, 0x0219, 0x021B, 0x021B, 0x021D, 0x021D, 0x021F, 0x021F,
	0x019E, 0x0221, 0x0223, 0x0223, 0x0225, 0x0225, 0x0227, 0x0227, 0x0229, 0x0229, 0x022B, 0x022B, 0x022D, 0x022D, 0x022F, 0x022F,
	0x0231, 0x0231, 0x0233, 0x0233, 0x0234, 0x0235, 0x0236, 0x0237, 0x0238, 0x0239, 0x2C65, 0x023C, 0x023C, 0x019A, 0x2C66, 0x023F,
	0x0240, 0x0242, 0x0242, 0x0180, 0x0289, 0x028C, 0x0247, 0x0247, 0x0249, 0x0249, 0x024B, 0x024B, 0x024D, 0x024D, 0x024F, 0x024F,
	0x0250, 0x0251, 0x0252, 0x0253, 0x0254, 0x0255, 0x0256, 0x0257, 0x0258, 0x0259, 0x025A, 0x025B, 0x025C, 0x025D, 0x025E, 0x025F,
	0x0260, 0x0261, 0x0262, 0x0263, 0x0264, 0x0265, 0x0266, 0x0267, 0x0268, 0x0269, 0x026A, 0x026B, 0x026C, 0x026D, 0x026E, 0x026F,
	0x0270, 0x0271, 0x0272, 0x0273, 0x0274, 0x0275, 0x0276, 0x0277, 0x0278, 0x0279, 0x027A, 0x027B, 0x027C, 0x027D, 0x027E, 0x027F,
	// 0280
	0x0280, 0x0281, 0x0282, 0x0283, 0x0284, 0x0285, 0x0286, 0x0287, 0x0288, 0x0289, 0x028A, 0x028B, 0x028C, 0x028D, 0x028E, 0x028F,
	0x0290, 0x0291, 0x0292, 0x0293, 0x0294, 0x0295, 0x0296, 0x0297, 0x0298, 0x0299, 0x029A, 0x029B, 0x029C, 0x029D, 0x029E, 0x029F,
	0x02A0, 0x02A1, 0x02A2, 0x02A3, 0x02A4, 0x02A5, 0x02A6, 0x02A7, 0x02A8, 0x02A9, 0x02AA, 0x02AB, 0x02AC, 0x02AD, 0x02AE, 0x02AF,
	0x02B0, 0x02B1, 0x02B2, 0x02B3, 0x02B4, 0x02B5, 0x02B6, 0x02B7, 0x02B8, 0x02B9, 0x02BA, 0x02BB, 0x02BC, 0x02BD, 0x02BE, 0x02BF,
	0x02C0, 0x02C1, 0x02C2, 0x02C3, 0x02C4, 0x02C5, 0x02C6, 0x02C7, 0x02C8, 0x02C9, 0x02CA, 0x02CB, 0x02CC, 0x02CD, 0x02CE, 0x02CF,
	0x02D0, 0x02D1, 0x02D2, 0x02D3, 0x02D4, 0x02D5, 0x02D6, 0x02D7, 0x02D8, 0x02D9, 0x02DA, 0x02DB, 0x02DC, 0x02DD, 0x02DE, 0x02DF,
	0x02E0, 0x02E1, 0x02E2, 0x02E3, 0x02E4, 0x02E5, 0x02E6, 0x02E7, 0x02E8, 0x02E9, 0x02EA, 0x02EB, 0x02EC, 0x02ED, 0x02EE, 0x02EF,
	0x02F0, 0x02F1, 0x02F2, 0x02F3, 0x02F4, 0x02F5, 0x02F6, 0x02F7, 0x02F8, 0x02F9, 0x02FA, 0x02FB, 0x02FC, 0x02FD, 0x02FE, 0x02FF,
	// 0300
	0x0300, 0x0301, 0x0302, 0x0303, 0x0304, 0x0305, 0x0306, 0x0307, 0x0308, 0x0309, 0x030A, 0x030B, 0x030C, 0x030D, 0x030E, 0x030F,
	0x0310, 0x0311, 0x0312, 0x0313, 0x0314, 0x0315, 0x0316, 0x0317, 0x0318, 0x0319, 0x031A, 0x031B, 0x031C, 0x031D, 0x031E, 0x031F,
	0x0320, 0x0321, 0x0322, 0x0323, 0x0324, 0x0325, 0x0326, 0x0327, 0x0328, 0x0329, 0x032A, 0x032B, 0x032C, 0x032D, 0x032E, 0x032F,
	0x0330, 0x0331, 0x0332, 0x0333, 0x0334, 0x0335, 0x0336, 0x0337, 0x0338, 0x0339, 0x033A, 0x033B, 0x033C, 0x033D, 0x033E, 0x033F,
	0x0340, 0x0341, 0x0342, 0x0343, 0x0344, 0x03B9, 0x0346, 0x0347, 0x0348, 0x0349, 0x034A, 0x034B, 0x034C, 0x034D, 0x034E, 0x034F,
	0x0350, 0x0351, 0x0352, 0x0353, 0x0354, 0x0355, 0x0356, 0x0357, 0x0358, 0x0359, 0x035A, 0x035B, 0x035C, 0x035D, 0x035E, 0x035F,
	0x0360, 0x0361, 0x0362, 0x0363, 0x0364, 0x0365, 0x0366, 0x0367, 0x0368, 0x0369, 0x036A, 0x036B, 0x036C, 0x036D, 0x036E, 0x036F,
	0x0371, 0x0371, 0x0373, 0x0373, 0x0374, 0x0375, 0x0377, 0x0377, 0x0378, 0x0379, 0x037A, 0x037B, 0x037C, 0x037D, 0x037E, 0x03F3,
	// 0380
	0x0380, 0x0381, 0x0382, 0x0383, 0x0384, 0x0385, 0x03AC, 0x0387, 0x03AD, 0x03AE, 0x03AF, 0x038B, 0x03CC, 0x038D, 0x03CD, 0x03CE,
	0x0390, 0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7, 0x03B8, 0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF,
	0x03C0, 0x03C1, 0x03A2, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7, 0x03C8, 0x03C9, 0x03CA, 0x03CB, 0x03AC, 0x03AD, 0x03AE, 0x03AF,
	0x03B0, 0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7, 0x03B8, 0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF,
	0x03C0, 0x03C1, 0x03C3, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7, 0x03C8, 0x03C9, 0x03CA, 0x03CB, 0x03CC, 0x03CD, 0x03CE, 0x03D7,
	0x03B2, 0x03B8, 0x03D2, 0x03D3, 0x03D4, 0x03C6, 0x03C0, 0x03D7, 0x03D9, 0x03D9, 0x03DB, 0x03DB, 0x03DD, 0x03DD, 0x03DF, 0x03DF,
	0x03E1, 0x03E1, 0x03E3, 0x03E3, 0x03E5, 0x03E5, 0x03E7, 0x03E7, 0x03E9, 0x03E9, 0x03EB, 0x03EB, 0x03ED, 0x03ED, 0x03EF, 0x03EF,
	0x03BA, 0x03C1, 0x03F2, 0x03F3, 0x03B8, 0x03B5, 0x03F6, 0x03F8, 0x03F8, 0x03F2, 0x03FB, 0x03FB, 0x03FC, 0x037B, 0x037C, 0x037D,
	// 0400
	0x0450, 0x0451, 0x0452, 0x0453, 0x0454, 0x0455, 0x0456, 0x0457, 0x0458, 0x0459, 0x045A, 0x045B, 0x045C, 0x045D, 0x045E, 0x045F,
	0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
	0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
	0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
	0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
	0x0450, 0x0451, 0x0452, 0x0453, 0x0454, 0x0455, 0x0456, 0x0457, 0x0458, 0x0459, 0x045A, 0x045B, 0x045C, 0x045D, 0x045E, 0x045F,
	0x0461, 0x0461, 0x0463, 0x0463, 0x0465, 0x0465, 0x0467, 0x0467, 0x0469, 0x0469, 0x046B, 0x046B, 0x046D, 0x046D, 0x046F, 0x046F,
	0x0471, 0x0471, 0x0473, 0x0473, 0x0475, 0x0475, 0x0477, 0x0477, 0x0479, 0x0479, 0x047B, 0x047B, 0x047D, 0x047D, 0x047F, 0x047F,
	// 0480
	0x0481, 0x0481, 0x0482, 0x0483, 0x0484, 0x0485, 0x0486, 0x0487, 0x0488, 0x0489, 0x048B, 0x048B, 0x048D, 0x048D, 0x048F, 0x048F,
	0x0491, 0x0491, 0x0493, 0x0493, 0x0495, 0x0495, 0x0497, 0x0497, 0x0499, 0x0499, 0x049B, 0x049B, 0x049D, 0x049D, 0x049F, 0x049F,
	0x04A1, 0x04A1, 0x04A3, 0x04A3, 0x04A5, 0x04A5, 0x04A7, 0x04A7, 0x04A9, 0x04A9, 0x04AB, 0x04AB, 0x04AD, 0x04AD, 0x04AF, 0x04AF,
	0x04B1, 0x04B1, 0x04B3, 0x04B3, 0x04B5, 0x04B5, 0x04B7, 0x04B7, 0x04B9, 0x04B9, 0x04BB, 0x04BB, 0x04BD, 0x04BD, 0x04BF, 0x04BF,
	0x04CF, 0x04C2, 0x04C2, 0x04C4, 0x04C4, 0x04C6, 0x04C6, 0x04C8, 0x04C8, 0x04CA, 0x04CA, 0x04CC, 0x04CC, 0x04CE, 0x04CE, 0x04CF,
	0x04D1, 0x04D1, 0x04D3, 0x04D3, 0x04D5, 0x04D5, 0x04D7, 0x04D7, 0x04D9, 0x04D9, 0x04DB, 0x04DB, 0x04DD, 0x04DD, 0x04DF, 0x04DF,
	0x04E1, 0x04E1, 0x04E3, 0x04E3, 0x04E5, 0x04E5, 0x04E7, 0x04E7, 0x04E9, 0x04E9, 0x04EB, 0x04EB, 0x04ED, 0x04ED, 0x04EF, 0x04EF,
	0x04F1, 0x04F1, 0x04F3, 0x04F3, 0x04F5, 0x04F5, 0x04F7, 0x04F7, 0x04F9, 0x04F9, 0x04FB, 0x04FB, 0x04FD, 0x04FD, 0x04FF, 0x04FF
};


static Crc32 g_crc(true, 0x1EDC6F41);

uint64_t Fletcher64(const uint32_t *data, size_t cnt, uint64_t init)
{
	size_t k;

	uint64_t sum1 = init & 0xFFFFFFFFU;
	uint64_t sum2 = (init >> 32);

	for (k = 0; k < cnt; k++)
	{
		sum1 = (sum1 + data[k]);
		sum2 = (sum2 + sum1);
	}

	sum1 = sum1 % 0xFFFFFFFF;
	sum2 = sum2 % 0xFFFFFFFF;

	return (static_cast<uint64_t>(sum2) << 32) | static_cast<uint64_t>(sum1);
}

bool VerifyBlock(const void *block, size_t size)
{
	uint64_t cs;
	const uint32_t * const data = reinterpret_cast<const uint32_t *>(block);

	size /= sizeof(uint32_t);

	cs = *reinterpret_cast<const uint64_t *>(block);
	if (cs == 0)
		return false;
	if (cs == 0xFFFFFFFFFFFFFFFFULL)
		return false;

	cs = Fletcher64(data + 2, size - 2, 0);
	cs = Fletcher64(data, 2, cs);

	return cs == 0;
}

bool IsZero(const byte_t *data, size_t size)
{
	for (size_t k = 0; k < size; k++)
	{
		if (data[k] != 0)
			return false;
	}
	return true;
}

bool IsEmptyBlock(const void *data, size_t blksize)
{
	blksize /= sizeof(uint64_t);
	const uint64_t *qdata = reinterpret_cast<const uint64_t *>(data);

	for (size_t k = 0; k < blksize; k++)
	{
		if (qdata[k] != 0)
			return false;
	}
	return true;
}

void DumpHex(std::ostream &os, const byte_t *data, size_t size, size_t lineSize)
{
	using namespace std;

	size_t i, j;
	byte_t b;

	if (size == 0)
		return;

	os << hex << uppercase << setfill('0');

	for (i = 0; i < size; i += lineSize)
	{
		os << setw(4) << i << ": ";
		for (j = 0; (j < lineSize) && ((i + j) < size); j++)
			os << setw(2) << static_cast<unsigned int>(data[i + j]) << ' ';
		for (; j < lineSize; j++)
			os << "   ";

		os << "- ";

		for (j = 0; (j < lineSize) && ((i + j) < size); j++)
		{
			b = data[i + j];
			if (b >= 0x20 && b < 0x7F)
				os << b;
			else
				os << '.';
		}

		os << endl;
	}

	// os << dec;
}

#undef DEBUG_OUT

#ifdef __linux__

uint32_t HashFilename(const char *utf8str, uint16_t name_len, bool case_insensitive)
{
	icu::StringPiece utf8(utf8str);
	icu::UnicodeString str = icu::UnicodeString::fromUTF8(utf8);
	icu::UnicodeString normalized;
	UErrorCode err = U_ZERO_ERROR;
	std::vector<UChar32> hashdata;
	uint32_t hash;

	const icu::Normalizer2 *norm = icu::Normalizer2::getNFDInstance(err);

#ifdef DEBUG_OUT
	if (U_FAILURE(err))
	{
		std::cerr << "getNFDInstance failed." << std::endl;
		return 0;
	}
	else
	{
		std::cerr << "norm = " << norm << ", err = " << err << std::endl;
	}
#endif

	normalized = norm->normalize(str, err);

#ifdef DEBUG_OUT
	if (U_FAILURE(err))
	{
		std::cerr << "Normalization failed." << std::endl;
		return 0;
	}
	else
	{
		std::cerr << "Normalization ok, err = " << err << std::endl;
	}
#endif

	hashdata.resize(normalized.countChar32() + 1);
	normalized.toUTF32(hashdata.data(), hashdata.size(), err);

#ifdef DEBUG_OUT
	if (U_FAILURE(err))
	{
		std::cerr << "Conversion to UTF32 failed." << std::endl;
		return 0;
	}
	else
	{
		std::cerr << "Conversion to UTF32 ok, err = " << err << std::endl;
	}
#endif

	if (case_insensitive)
	{
		for (size_t k = 0; k < hashdata.size(); k++)
		{
			if (hashdata[k] < 0x500)
				hashdata[k] = lowercase_table[hashdata[k]];
		}
	}

#ifdef DEBUG_OUT
	for (size_t k = 0; k < hashdata.size(); k++)
	{
		if (hashdata[k] > 0x100)
			std::cerr << '?';
		else
			std::cerr << (char)hashdata[k];
	}
	std::cerr << std::endl;
#endif

	hashdata.pop_back();


	g_crc.SetCRC(0xFFFFFFFF);
	g_crc.Calc(reinterpret_cast<const byte_t *>(hashdata.data()), hashdata.size() * sizeof(UChar32));
	hash = g_crc.GetCRC();

	hash = ((hash & 0x3FFFFF) << 10) | (name_len & 0x3FF);

	return hash;
}

#else

uint32_t HashFilename(const char *utf8str, uint16_t name_len, bool case_insensitive)
{
	std::vector<char32_t> u32_name;
	std::vector<char32_t> u32_normalized;
	uint32_t hash;

	Utf8toU32(u32_name, reinterpret_cast<const uint8_t *>(utf8str));
	NormalizeFilename(u32_normalized, u32_name, case_insensitive);

	g_crc.SetCRC(0xFFFFFFFF);
	g_crc.Calc(reinterpret_cast<const byte_t *>(u32_normalized.data()), u32_normalized.size() * sizeof(char32_t));

	hash = g_crc.GetCRC();

	hash = ((hash & 0x3FFFFF) << 10) | (name_len & 0x3FF);

	return hash;
}

#endif

bool GetPassword(std::string &pw)
{
	struct termios told, tnew;
	FILE *stream = stdin;

	/* Turn echoing off and fail if we can’t. */
	if (tcgetattr (fileno (stream), &told) != 0)
		return false;
	tnew = told;
	tnew.c_lflag &= ~ECHO;
	if (tcsetattr (fileno (stream), TCSAFLUSH, &tnew) != 0)
		return false;

	/* Read the password. */
	std::getline(std::cin, pw);

	/* Restore terminal. */
	(void) tcsetattr (fileno (stream), TCSAFLUSH, &told);

	std::cout << std::endl;

	return true;
}

// Like DumpHex, but prints a label.
void DumpBuffer(const uint8_t *data, size_t len, const char *label)
{
	std::cout << "dumping " << label << std::endl;
	DumpHex(std::cout, data, len);
}
