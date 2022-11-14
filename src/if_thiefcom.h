#pragma once
#include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型

namespace thiefcom
{

enum CODE : uint8_t
{
	CODE_NOP		= 0x00,
	CODE_SYSINFO	= 0x01,	// システム情報
	CODE_WAIT		= 0x02,	// OPLL(YM2413)
	CODE_OPLL		= 0x03,	// OPLL(YM2413)
	CODE_PSG		= 0x04,	// PSG(YM2149,YMZ294)
	CODE_SCC_90		= 0x08,	// SCC 90xxh
	CODE_SCC_98		= 0x09,	// SCC 98xxh
	CODE_SCC_B0		= 0x0A,	// SCC B0xxh
	CODE_SCC_B8		= 0x0B,	// SCC B8xxh
	CODE_SCC_BF		= 0x0C,	// SCC BFxxh
	CODE_TC			= 0x80,		// MSBのみ。bit6-0はタイムコードの上位7bitとして使用される
};

// XIOTHIEFボードから送信されてくるデータの構造体
union THIEFCOM {
	struct {
		CODE	code;
		uint8_t	data1;
		uint8_t	data2;
	};
	uint8_t	data[3];
};

} // namespace thiefcom