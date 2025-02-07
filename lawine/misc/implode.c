/************************************************************************/
/* File Name   : implode.c                                              */
/* Creator     : ax.minaduki@gmail.com                                  */
/* Create Time : May 23rd, 2010                                         */
/* Module      : Lawine library                                         */
/* Descript    : PKWare DCL implode compression API implementation      */
/************************************************************************/

#include "implode.h"

/************************************************************************/

#define COPY_LENGTH_MAX		516		/* PKZip一次复制所允许的最大(复制长度-2) */
#define COPY_LENGTH_END		517		/* 如果(复制长度-2)为此值则表示PKZip编码结束 */
#define COPY_LENGTH_NUM		518		/* (复制长度-2)映射表大小（包含结束符） */

#define READ_BUF_SIZE		4096	/* 压缩时一次读取数据的最大字节数 */
#define HASH_NUM_MAX		2296	/* 所有可能的哈希值数目（即最大的哈希值0xff*4+0xff*5加1） */
#define HASH_BUFFER_MAX		8708	/* 计算哈希的数据缓冲的最大长度（等于sizeof(dict_buf)+sizeof(copy_buf)+sizeof(read_buf)） */

/* 计算字典的字节数 */
#define dict_byte_size(n)	(0x40 << (n))

/* 计算“字节对”的哈希值 */
/* 在压缩时会进行串比较，其算法是假设了哈希相同且第一个字节相同则第二个字节也一定相同。 */
/* 因此为了保证对于给定的哈希值和p[0]而有唯一的p[1]，p[0]与p[1]所乘的两个系数应该互素。 */
#define make_hash(p)		((p)[0] * 4 + (p)[1] * 5)

/************************************************************************/

/* Bit sequences used to represent literal bytes */
static CONST WORD s_ChCode[] = {
	0x0490, 0x0fe0, 0x07e0, 0x0be0, 0x03e0, 0x0de0, 0x05e0, 0x09e0,
	0x01e0, 0x00b8, 0x0062, 0x0ee0, 0x06e0, 0x0022, 0x0ae0, 0x02e0,
	0x0ce0, 0x04e0, 0x08e0, 0x00e0, 0x0f60, 0x0760, 0x0b60, 0x0360,
	0x0d60, 0x0560, 0x1240, 0x0960, 0x0160, 0x0e60, 0x0660, 0x0a60,
	0x000f, 0x0250, 0x0038, 0x0260, 0x0050, 0x0c60, 0x0390, 0x00d8,
	0x0042, 0x0002, 0x0058, 0x01b0, 0x007c, 0x0029, 0x003c, 0x0098,
	0x005c, 0x0009, 0x001c, 0x006c, 0x002c, 0x004c, 0x0018, 0x000c,
	0x0074, 0x00e8, 0x0068, 0x0460, 0x0090, 0x0034, 0x00b0, 0x0710,
	0x0860, 0x0031, 0x0054, 0x0011, 0x0021, 0x0017, 0x0014, 0x00a8,
	0x0028, 0x0001, 0x0310, 0x0130, 0x003e, 0x0064, 0x001e, 0x002e,
	0x0024, 0x0510, 0x000e, 0x0036, 0x0016, 0x0044, 0x0030, 0x00c8,
	0x01d0, 0x00d0, 0x0110, 0x0048, 0x0610, 0x0150, 0x0060, 0x0088,
	0x0fa0, 0x0007, 0x0026, 0x0006, 0x003a, 0x001b, 0x001a, 0x002a,
	0x000a, 0x000b, 0x0210, 0x0004, 0x0013, 0x0032, 0x0003, 0x001d,
	0x0012, 0x0190, 0x000d, 0x0015, 0x0005, 0x0019, 0x0008, 0x0078,
	0x00f0, 0x0070, 0x0290, 0x0410, 0x0010, 0x07a0, 0x0ba0, 0x03a0,
	0x0240, 0x1c40, 0x0c40, 0x1440, 0x0440, 0x1840, 0x0840, 0x1040,
	0x0040, 0x1f80, 0x0f80, 0x1780, 0x0780, 0x1b80, 0x0b80, 0x1380,
	0x0380, 0x1d80, 0x0d80, 0x1580, 0x0580, 0x1980, 0x0980, 0x1180,
	0x0180, 0x1e80, 0x0e80, 0x1680, 0x0680, 0x1a80, 0x0a80, 0x1280,
	0x0280, 0x1c80, 0x0c80, 0x1480, 0x0480, 0x1880, 0x0880, 0x1080,
	0x0080, 0x1f00, 0x0f00, 0x1700, 0x0700, 0x1b00, 0x0b00, 0x1300,
	0x0da0, 0x05a0, 0x09a0, 0x01a0, 0x0ea0, 0x06a0, 0x0aa0, 0x02a0,
	0x0ca0, 0x04a0, 0x08a0, 0x00a0, 0x0f20, 0x0720, 0x0b20, 0x0320,
	0x0d20, 0x0520, 0x0920, 0x0120, 0x0e20, 0x0620, 0x0a20, 0x0220,
	0x0c20, 0x0420, 0x0820, 0x0020, 0x0fc0, 0x07c0, 0x0bc0, 0x03c0,
	0x0dc0, 0x05c0, 0x09c0, 0x01c0, 0x0ec0, 0x06c0, 0x0ac0, 0x02c0,
	0x0cc0, 0x04c0, 0x08c0, 0x00c0, 0x0f40, 0x0740, 0x0b40, 0x0340,
	0x0300, 0x0d40, 0x1d00, 0x0d00, 0x1500, 0x0540, 0x0500, 0x1900,
	0x0900, 0x0940, 0x1100, 0x0100, 0x1e00, 0x0e00, 0x0140, 0x1600,
	0x0600, 0x1a00, 0x0e40, 0x0640, 0x0a40, 0x0a00, 0x1200, 0x0200,
	0x1c00, 0x0c00, 0x1400, 0x0400, 0x1800, 0x0800, 0x1000, 0x0000,
};

/* Lengths of bit sequences used to represent literal bytes */
static CONST BYTE s_ChBits[] = {
	11, 12, 12, 12, 12, 12, 12, 12, 12,  8,  7, 12, 12,  7, 12, 12,
	12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 12, 12, 12, 12, 12,
	 4, 10,  8, 12, 10, 12, 10,  8,  7,  7,  8,  9,  7,  6,  7,  8,
	 7,  6,  7,  7,  7,  7,  8,  7,  7,  8,  8, 12, 11,  7,  9, 11,
	12,  6,  7,  6,  6,  5,  7,  8,  8,  6, 11,  9,  6,  7,  6,  6,
	 7, 11,  6,  6,  6,  7,  9,  8,  9,  9, 11,  8, 11,  9, 12,  8,
	12,  5,  6,  6,  6,  5,  6,  6,  6,  5, 11,  7,  5,  6,  5,  5,
	 6, 10,  5,  5,  5,  5,  8,  7,  8,  8, 10, 11, 11, 12, 12, 12,
	13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
	13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
	13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
	12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
	12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
	12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
	13, 12, 13, 13, 13, 12, 13, 13, 13, 12, 13, 13, 13, 13, 12, 13,
	13, 13, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
};

/* Bit sequences used to represent the base values of the copy length */
static CONST BYTE s_LenCode[] = {
	0x05, 0x03, 0x01, 0x06, 0x0a, 0x02, 0x0c, 0x14, 0x04, 0x18, 0x08, 0x30, 0x10, 0x20, 0x40, 0x00,
};

/* Lengths of bit sequences used to represent the base values of the copy length */
static CONST BYTE s_LenBits[] = { 3, 2, 3, 3, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7 };

/* Base values used for the copy length */
static CONST WORD s_LenBase[] = { 0, 0, 0, 0, 0, 0, 0, 0, 8, 10, 14, 22, 38, 70, 134, 262 };

/* Lengths of extra bits used to represent the copy length */
static CONST BYTE s_ExLenBits[] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8 };

/* Bit sequences used to represent the most significant 6 bits of the copy offset */
static CONST BYTE s_OffCode[] = {
	0x03, 0x0d, 0x05, 0x19, 0x09, 0x11, 0x01, 0x3e, 0x1e, 0x2e, 0x0e, 0x36, 0x16, 0x26, 0x06, 0x3a,
	0x1a, 0x2a, 0x0a, 0x32, 0x12, 0x22, 0x42, 0x02, 0x7c, 0x3c, 0x5c, 0x1c, 0x6c, 0x2c, 0x4c, 0x0c,
	0x74, 0x34, 0x54, 0x14, 0x64, 0x24, 0x44, 0x04, 0x78, 0x38, 0x58, 0x18, 0x68, 0x28, 0x48, 0x08,
	0xf0, 0x70, 0xb0, 0x30, 0xd0, 0x50, 0x90, 0x10, 0xe0, 0x60, 0xa0, 0x20, 0xc0, 0x40, 0x80, 0x00,
};

/* Lengths of bit sequences used to represent the most significant 6 bits of the copy offset */
static CONST BYTE s_OffBits[] = {
	2, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
};

/************************************************************************/

/* 位读写数据流 */
struct BIT_STREAM {
	BUFPTR cur_ptr;							/* 当前数据指针 */
	BUFCPTR end_ptr;						/* 结束位置指针 */
	UINT bit_cnt;							/* 缓冲位数 */
	UINT bit_buf;							/* 缓冲数据 */
};

/* 压缩用数据结构 */
struct IMP_CONTEXT {
	BYTE type;								/* 压缩类型（0为二进制方式，1为ASCII文本方式） */
	BYTE dict;								/* 字典大小（4表示1KB，5表示2KB，6表示4KB，其余均为无效） */
	BYTE ch_bits[256];						/* “ASCII字符->编码位数”映射表（256个字符，编码包含前导位0） */
	WORD ch_code[256];						/* “ASCII字符->编码位串”映射表（256个字符，编码包含前导位0） */
	BYTE len_bits[COPY_LENGTH_NUM];			/* “(复制长度-2)->编码位数”映射表（最大复制长度为518个字节，编码包含前导位1） */
	WORD len_code[COPY_LENGTH_NUM];			/* “(复制长度-2)->编码位串”映射表（最大复制长度为518个字节，编码包含前导位1） */
	BYTE dict_buf[4096];					/* 字典缓冲（对于1KB、2KB的情况，缓冲的前半部分不会被用到） */
	BYTE copy_buf[COPY_LENGTH_MAX];			/* 复制缓冲，用以补足最长可复制516字节的部分 */
	BYTE read_buf[READ_BUF_SIZE];			/* 读数据缓冲（不论字典多大，都一次读取4K个字节） */
	BYTE hash_dummy;						/* 由于哈希算法一次处理两个字节，最后需要多留一个字节以防止内存访问越界 */
	WORD hash_idx[HASH_NUM_MAX];			/* “字节对”哈希值对应的哈希索引表（通过hash_map最终可以定向到数据缓冲的对应位置） */
	BUFCPTR hash_map[HASH_BUFFER_MAX];		/* 哈希索引对应数据缓冲头指针（指向“字节对”的首字节）的映射表 */
	struct BIT_STREAM bs;					/* 压缩用位数据流 */
};

/* 解压用数据结构 */
struct EXP_CONTEXT {
	BYTE type;								/* 压缩类型（0为二进制方式，1为ASCII文本方式） */
	BYTE dict;								/* 字典大小（4表示1KB，5表示2KB，6表示4KB，其余均为无效） */
	BYTE len_tab[128];						/* “编码位串->(复制长度-2)”映射表（编码不包括扩展部分，编码不包含前导位1） */
	BYTE off_tab[256];						/* “编码位串->(复制起始偏移-1)的高6位”映射表（复制起始偏移的具体数值和字典大小有关） */
	BYTE ch_bits[256];						/* “ASCII字符->映射表实际存放的有效数据的位数”映射表（“映射表”为下面的4张“编码位串->ASCII字符”映射表） */
	BYTE ch_code_tab1[256];					/* “编码位串->ASCII字符”映射表1（仅存放8位以内的字符编码） */
	BYTE ch_code_tab2[256];					/* “编码位串->ASCII字符”映射表2（仅存放8位以上且低6位不全为0的编码，“编码位串”为低4位以外部分的编码）*/
	BYTE ch_code_tab3[128];					/* “编码位串->ASCII字符”映射表3（仅对应低6位全为0的编码，“编码位串”为低6位以外部分的编码） */
	BYTE ch_code_tab4[32];					/* “编码位串->ASCII字符”映射表4（仅对应低8位全为0的编码，“编码位串”为低8位以外部分的编码） */
	struct BIT_STREAM bs;					/* 解压用位数据流 */
};

/* 位数据流操作函数 */
static VOID init_bits(struct BIT_STREAM *bs, BUFCPTR ptr, UINT size);
static UINT peek_bits(struct BIT_STREAM *bs, UINT num);
static BOOL skip_bits(struct BIT_STREAM *bs, UINT num);
static BOOL put_bits(struct BIT_STREAM *bs, UINT num, UINT bits);
static BOOL flush_bits(struct BIT_STREAM *bs);

/* 压缩用函数 */
static BOOL init_implode(struct IMP_CONTEXT *ctx, INT type, INT dict, BUFPTR buf_ptr, UINT buf_size);
static BUFPTR do_implode(struct IMP_CONTEXT *ctx, BUFCPTR rd_ptr, BUFCPTR rd_end);
static VOID hash_buffer(struct IMP_CONTEXT *ctx, BUFCPTR buf_ptr, BUFCPTR buf_end);
static BUFCPTR compress_buffer(struct IMP_CONTEXT *ctx, BUFCPTR buf_ptr, BUFCPTR buf_end, BUFCPTR hash_end);
static BOOL put_raw_byte(struct IMP_CONTEXT *ctx, BYTE byte);
static BOOL put_copy_cmd(struct IMP_CONTEXT *ctx, UINT copy_len, UINT copy_off);
static UINT find_repetition(struct IMP_CONTEXT *ctx, BUFCPTR buf_ptr, UINT *offset);
static UINT best_repetition(struct IMP_CONTEXT *ctx, UINT hash_idx, UINT copy_len, BUFCPTR buf_ptr, UINT *offset);

/* 解压用函数 */
static BOOL init_explode(struct EXP_CONTEXT *ctx, BUFCPTR buf_ptr, UINT buf_size);
static BUFPTR do_explode(struct EXP_CONTEXT *ctx, BUFPTR wrt_ptr, BUFPTR wrt_end);
static INT get_one_byte(struct EXP_CONTEXT *ctx);
static INT get_copy_length(struct EXP_CONTEXT *ctx);
static INT get_copy_offset(struct EXP_CONTEXT *ctx, UINT copy_len);

/************************************************************************/

BOOL implode(INT type, INT dict, VCPTR src, UINT src_size, VPTR dest, UINT *dest_size)
{
	BUFCPTR p;
	struct IMP_CONTEXT ctx;

	/* 参数有效性检查 */
	if (!src || !src_size || !dest || !dest_size)
		return FALSE;

	/* 初始化压缩状态 */
	if (!init_implode(&ctx, type, dict, dest, *dest_size))
		return FALSE;

	/* 执行压缩计算 */
	p = src;
	p = do_implode(&ctx, p, p + src_size);
	if (!p)
		return FALSE;

	/* 计算已写数据大小 */
	*dest_size = (UINT)(p - (BUFPTR)dest);

	/* 成功 */
	return TRUE;
}

BOOL explode(VCPTR src, UINT src_size, VPTR dest, UINT *dest_size)
{
	BUFPTR p;
	struct EXP_CONTEXT ctx;

	/* 参数有效性检查 */
	if (!src || !src_size || !dest || !dest_size)
		return FALSE;

	/* 初始化解压状态 */
	if (!init_explode(&ctx, src, src_size))
		return FALSE;

	/* 执行解压计算 */
	p = dest;
	p = do_explode(&ctx, p, p + *dest_size);
	if (!p)
		return FALSE;

	/* 计算已写数据大小 */
	*dest_size = (UINT)(p - (BUFPTR)dest);

	/* 成功 */
	return TRUE;
}

/************************************************************************/

static VOID init_bits(struct BIT_STREAM *bs, BUFCPTR ptr, UINT size)
{
	DAssert(bs && ptr);

	bs->cur_ptr = (BUFPTR)ptr;
	bs->end_ptr = ptr + size;
	bs->bit_buf = 0;
	bs->bit_cnt = 0;
}

static UINT peek_bits(struct BIT_STREAM *bs, UINT num)
{
	UINT mask;

	DAssert(bs && bs->cur_ptr && num <= 8);

	/* 如果位缓冲不足要求的个数 */
	if (bs->bit_cnt < num) {

		/* 如果已经到达缓冲区的末尾，则多出来的高位部分补0 */
		if (bs->cur_ptr >= bs->end_ptr)
			return bs->bit_buf;

		/* 先读取一个字节进位缓冲 */
		bs->bit_buf |= (*bs->cur_ptr++ << bs->bit_cnt);
		bs->bit_cnt += 8;
	}

	/* 计算掩码 */
	mask = (1 << num) - 1;

	/* 返回当前位数据 */
	return bs->bit_buf & mask;
}

static BOOL skip_bits(struct BIT_STREAM *bs, UINT num)
{
	DAssert(bs && bs->cur_ptr && num <= 8);

	/* 如果所要跳过的位都在位缓冲里则仅需要将它们移出位缓冲 */
	if (num <= bs->bit_cnt) {
		bs->bit_buf >>= num;
		bs->bit_cnt -= num;
		return TRUE;
	}

	/* 移除位缓冲的所有内容 */
	num -= bs->bit_cnt;

	/* 如果已经到达缓冲区的末尾则失败 */
	if (bs->cur_ptr >= bs->end_ptr)
		return FALSE;

	/* 读取一个字节进位缓冲并跳过剩余的位 */
	bs->bit_buf = *bs->cur_ptr++ >> num;
	bs->bit_cnt = 8 - num;

	/* 成功 */
	return TRUE;
}

static BOOL put_bits(struct BIT_STREAM *bs, UINT num, UINT bits)
{
	DAssert(bs && bs->cur_ptr && num <= 16);

	/* 先追加输入位数据到位缓冲 */
	bs->bit_buf |= bits << bs->bit_cnt;
	bs->bit_cnt += num;

	/* 数据写回处理 */
	while (bs->bit_cnt >= 8) {

		/* 如果已经到达写缓冲的末尾则失败 */
		if (bs->cur_ptr >= bs->end_ptr)
			return FALSE;

		/* 回写一个字节 */
		*bs->cur_ptr++ = bs->bit_buf;

		/* 消耗流中的一个字节 */
		bs->bit_buf >>= 8;
		bs->bit_cnt -= 8;
	}

	/* 成功 */
	return TRUE;
}

static BOOL flush_bits(struct BIT_STREAM *bs)
{
	DAssert(bs && bs->cur_ptr);

	/* 如果已经没有要写的数据了则结束 */
	while (bs->bit_cnt) {

		/* 如果已经到达写缓冲的末尾则失败 */
		if (bs->cur_ptr >= bs->end_ptr)
			return FALSE;

		/* 回写一个字节 */
		*bs->cur_ptr++ = bs->bit_buf;

		/* 残存数据不足一个字节时，作为一个完整的字节写完后立即结束 */
		if (bs->bit_cnt < 8) {
			bs->bit_buf = 0;
			bs->bit_cnt = 0;
			break;
		}

		/* 消耗流中的一个字节 */
		bs->bit_buf >>= 8;
		bs->bit_cnt -= 8;
	}

	/* 成功 */
	return TRUE;
}

static BOOL init_implode(struct IMP_CONTEXT *ctx, INT type, INT dict, BUFPTR buf_ptr, UINT buf_size)
{
	INT i, j;
	BYTE *p;
	WORD *q;

	DAssert(ctx);

	/* 压缩数据最小4字节 */
	if (buf_size < 4)
		return FALSE;

	/* 仅支持两种类型 */
	if (type != IMPLODE_BINARY && type != IMPLODE_ASCII)
		return FALSE;

	/* 字典大小必须是1K/2K/4K */
	if (dict < IMPLODE_DICT_1K || dict > IMPLODE_DICT_4K)
		return FALSE;

	/* 记录压缩类型和字典大小 */
	ctx->type = type;
	ctx->dict = dict;

	/* 写压缩类型和字典大小作为压缩数据头部 */
	*buf_ptr++ = type;
	*buf_ptr++ = dict;
	buf_size -= 2;

	/* 初始化输出位缓冲 */
	init_bits(&ctx->bs, buf_ptr, buf_size);

	/* 创建“ASCII字符->编码位数”和“ASCII字符->编码位串”映射表 */
	p = ctx->ch_bits;
	q = ctx->ch_code;
	for (i = 0; i < DCount(s_ChCode); i++) {
		*p++ = s_ChBits[i] + 1;
		*q++ = s_ChCode[i] << 1;
	}

	/* 创建“(复制长度-2)->编码位数”和“(复制长度-2)->编码位串”映射表 */
	p = ctx->len_bits;
	q = ctx->len_code;
	for (i = 0; i < DCount(s_LenBits); i++) {
		for (j = 0; j < (1 << s_ExLenBits[i]); j++) {
			*p++ = s_ExLenBits[i] + s_LenBits[i] + 1;
			*q++ = (j << (s_LenBits[i] + 1)) | (s_LenCode[i] << 1) | 1;
		}
	}

	/* 成功 */
	return TRUE;
}

/************************************************************************/

static BUFPTR do_implode(struct IMP_CONTEXT *ctx, BUFCPTR rd_ptr, BUFCPTR rd_end)
{
	UINT dict_size, copy_size;
	BUFCPTR buf_ptr, buf_end, hash_end;
	BUFPTR hash_ptr;

	DAssert(ctx && rd_ptr && rd_end);

	/* dict_buf仅仅复制一段连续的输入缓冲，没有对源数据做任何改写操作，应该可以优化掉，转为直接使用输入缓冲 */
	/* TODO: 但这么做输出结果可能会和Staredit有差异（理论上会更优），以后加编译宏实现 */

	dict_size = dict_byte_size(ctx->dict);

	/* 第一块总是从read_buf处开始处理 */
	buf_ptr = ctx->read_buf;
	hash_ptr = ctx->read_buf;

	/* 压缩处理循环 */
	while (rd_ptr < rd_end) {

		DAssert(buf_ptr >= hash_ptr);

		/* 无论字典多大，一次都读取4KB输入数据 */
		copy_size = READ_BUF_SIZE;
		if (rd_ptr + copy_size > rd_end)
			copy_size = (UINT)(rd_end - rd_ptr);

		/* 总是从输入读取数据到压缩缓冲区的read_buf位置 */
		DMemCpy(ctx->read_buf, rd_ptr, copy_size);
		rd_ptr += copy_size;

		/* hash_end同时也是源复制起始指针的最末尾。这意味着除了最后一块之外，hash_end在buf_end-516处 */
		buf_end = ctx->read_buf + copy_size;
		hash_end = (rd_ptr >= rd_end) ? buf_end : (buf_end - COPY_LENGTH_MAX);

		/* 根据缓冲中的源数据构造当前压缩处理的哈希表 */
		hash_buffer(ctx, hash_ptr, hash_end);

		/* 数据块压缩处理 */
		while (buf_ptr < hash_end) {
			buf_ptr = compress_buffer(ctx, buf_ptr, buf_end, hash_end);
			if (!buf_ptr)
				return NULL;
		}

		/* 如果已经是最后一块数据则结束循环处理 */
		if (rd_ptr >= rd_end)
			break;

		/* 为了和Staredit保持一致这里采用与pklib相同的缓冲使用策略 */
		buf_ptr -= READ_BUF_SIZE;

		/* 4KB字典的情况，由于第一次移动缓存时已读取数据不足以填满dict_buf和copy_buf而需要特殊处理 */
		if (hash_ptr == ctx->read_buf && ctx->dict == IMPLODE_DICT_4K)
			copy_size = dict_size - (buf_ptr - ctx->copy_buf);	/* 其实这里可以固定使用4096字节提高字典效率，但为了和pklib一致保持该用法 */
		else
			copy_size = dict_size + (ctx->read_buf - buf_ptr);

		/* 总是拷贝以read_buff的末尾（即&ctx->hash_dummy）为末端的copy_size个字节 */
		/* 注意由于存在重叠区域，因此下面不能使用memcpy */
		hash_ptr = ctx->read_buf - copy_size;
		DMemMov(hash_ptr, &ctx->hash_dummy - copy_size, copy_size);
	}

	/* 最后书写结束标志 */
	if (!put_bits(&ctx->bs, ctx->len_bits[COPY_LENGTH_END], ctx->len_code[COPY_LENGTH_END]))
		return NULL;

	/* 将位数据流中所有缓冲写回 */
	if (!flush_bits(&ctx->bs))
		return NULL;

	/* 返回最后的写位置 */
	return ctx->bs.cur_ptr;
}

static VOID hash_buffer(struct IMP_CONTEXT *ctx, BUFCPTR buf_ptr, BUFCPTR buf_end)
{
	INT i;
	UINT hash, sum;
	BUFCPTR p;

	DAssert(ctx && buf_ptr && buf_end);

	/* 注意该函数会读取到buf_end处的字节，因此有hash_dummy这么一个字节的预留空间以防越界访问 */
	/* buf_end处的字节的值是什么都无所谓（因此不需要初始化hash_dummy）。 */

	/* 首先将哈希索引表清零 */
	DVarClr(ctx->hash_idx);

	/* 首先计算每对相邻字节对应的哈希值在给定缓冲中的出现次数 */
	for (p = buf_ptr; p < buf_end; p++) {
		hash = make_hash(p);
		ctx->hash_idx[hash]++;
	}

	/* 累加哈希值出现次数 */
	for (i = 0, sum = 0; i < HASH_NUM_MAX; i++) {
		sum += ctx->hash_idx[i];
		ctx->hash_idx[i] = sum;
	}

	/* 经过下面的运算后，hash_map[hash_idx[hash]]指向哈希值hash首次在缓冲中出现的位置 */
	/* 如果同一个哈希值多次出现（哈希冲突），则hash_map[hash_idx[hash]+i]指向哈希值hash第i次在缓冲中出现的位置（第0次为首次） */
	for (p = buf_end - 1; p >= buf_ptr; p--) {
		hash = make_hash(p);
		i = --ctx->hash_idx[hash];
		ctx->hash_map[i] = p;
	}
}

static BUFCPTR compress_buffer(struct IMP_CONTEXT *ctx, BUFCPTR buf_ptr, BUFCPTR buf_end, BUFCPTR hash_end)
{
	UINT copy_len, copy_off, new_len, new_off;

	DAssert(ctx && buf_ptr && buf_end && hash_end);

	/* 查找符合当前字节流的重复数据 */
	copy_len = find_repetition(ctx, buf_ptr, &copy_off);

	/* 没有找到重复数据，直接写单字节原生数据 */
	if (!copy_len) {

		if (!put_raw_byte(ctx, *buf_ptr++))
			return NULL;

		return buf_ptr;
	}

	/* 循环查找尽可能长的可复制数据块 */
	while (TRUE) {

		DAssert(copy_len > 0);

		/* 已到达输入数据的末尾，阻止复制一切超过buf_end的数据 */
		if (buf_ptr + copy_len > buf_end) {
			copy_len = buf_end - buf_ptr;
			break;
		}

		/* 为了提高平均压缩速度，如果复制长度已经超过8字节就不再继续寻找更长的可复制数据 */
		if (copy_len >= 8 || buf_ptr + 1 >= hash_end)
			break;

		/* 尝试查找从下一个字节起的重复数据，并和之前的结果进行比较，从而选取更优的复制方案 */
		new_len = find_repetition(ctx, buf_ptr + 1, &new_off);

		/* 如果新复制长度仅仅比之前的复制长度多出一个字节，但同时之前的复制偏移小于等于128则直接使用之前的复制数据块输出 */
		if (new_len <= copy_len || (new_len == copy_len + 1 && copy_off <= 128))
			break;

		/* 找到了更优的复制数据块，先把前一个字节做单字节输出 */
		if (!put_raw_byte(ctx, *buf_ptr++))
			return NULL;

		/* 更新复制长度和偏移为更优值，并继续循环查找 */
		copy_len = new_len;
		copy_off = new_off;
	}

	/* 对于2个字节的重复数据，如果偏移达到了256字节可能会占用比写单个输出字节更多的空间，因此按单字节输出 */
	if (copy_len < 2 || (copy_len == 2 && copy_off >= 256)) {

		if (!put_raw_byte(ctx, *buf_ptr++))
			return NULL;

		return buf_ptr;
	}

	/* 写复制命令到输出缓冲（编码用copy_len的值为(复制长度-2)） */
	if (!put_copy_cmd(ctx, copy_len - 2, copy_off))
		return NULL;

	/* 返回当前输出位置指针 */
	return buf_ptr + copy_len;
}

static BOOL put_raw_byte(struct IMP_CONTEXT *ctx, BYTE byte)
{
	DAssert(ctx);

	if (ctx->type == IMPLODE_BINARY) {
		if (!put_bits(&ctx->bs, 9, (UINT)byte << 1))
			return FALSE;
	} else {
		if (!put_bits(&ctx->bs, ctx->ch_bits[byte], ctx->ch_code[byte]))
			return FALSE;
	}

	return TRUE;
}

static BOOL put_copy_cmd(struct IMP_CONTEXT *ctx, UINT copy_len, UINT copy_off)
{
	DAssert(ctx);

	if (!put_bits(&ctx->bs, ctx->len_bits[copy_len], ctx->len_code[copy_len]))
		return FALSE;

	/* 仅复制2个字节时特殊处理 */
	if (!copy_len) {
		if (!put_bits(&ctx->bs, s_OffBits[copy_off >> 2], s_OffCode[copy_off >> 2]))
			return FALSE;
		if (!put_bits(&ctx->bs, 2, copy_off & 0x03))
			return FALSE;
	} else {
		if (!put_bits(&ctx->bs, s_OffBits[copy_off >> ctx->dict], s_OffCode[copy_off >> ctx->dict]))
			return FALSE;
		if (!put_bits(&ctx->bs, ctx->dict, ((1 << ctx->dict) - 1) & copy_off))
			return FALSE;
	}

	return TRUE;
}

static UINT find_repetition(struct IMP_CONTEXT *ctx, BUFCPTR buf_ptr, UINT *offset)
{
	UINT cnt, hash, hash_idx, copy_len;
	BUFCPTR p, limit, rep_ptr;

	DAssert(ctx && buf_ptr && offset);

	/* 计算当前“字节对”的哈希值 */
	hash = make_hash(buf_ptr);
	hash_idx = ctx->hash_idx[hash];

	/* 计算字典边界 */
	limit = buf_ptr - dict_byte_size(ctx->dict);

	/* 查找在哈希表中字节对首次出现的位置，若超出字典范围则抛弃 */
	/* 此处的下标递增操作不会导致越界或失效（哈希不再匹配），因为最差会匹配到buf_ptr处 */
	/* TODO: 理论上是可以等于limit的，可能是lzlib的BUG，以后加编译宏区别以提高压缩率 */
	while (ctx->hash_map[hash_idx] <= limit)
		hash_idx++;

	/* 更新哈希索引表以提高访问效率 */
	ctx->hash_idx[hash] = hash_idx;

	/* 循环查找最优匹配 */
	for (copy_len = 0; ; hash_idx++) {

		/* 获得候选的“字节对”重复位置 */
		rep_ptr = ctx->hash_map[hash_idx];

		/* 字典越界检查。由于一次匹配一组“字节对”，因此字典中最后一个字节处不能作为一个“字节对”的起始 */
		if (rep_ptr >= buf_ptr - 1)
			return copy_len;

		/* 从当前缓冲位置开始匹配 */
		p = buf_ptr;

		/* 由于哈希值有可能有冲突，这里需要验证是否实际匹配 */
		if (*p != *rep_ptr)
			continue;

		/* 跳过已经匹配的2个字节（哈希“字节对”的长度） */
		rep_ptr += 2;
		p += 2;

		/* 该循环计算最长匹配长度，最大不超过516个字节 */
		for (cnt = 2; cnt < COPY_LENGTH_MAX; cnt++, p++, rep_ptr++) {
			if (*p != *rep_ptr)
				break;
		}

		/* 在匹配长度相等时，总是优先使用距离当前位置buf_ptr最近的匹配 */
		if (cnt < copy_len)
			continue;

		/* 计算并保存复制起始位置相对于当前位置的(复制起始偏移-1) */
		*offset = (UINT)(buf_ptr - rep_ptr + cnt - 1);

		/* 如果已经到达最大复制长度516则直接返回 */
		if (cnt == COPY_LENGTH_MAX) 
			return COPY_LENGTH_MAX;

		/* 如果复制长度大于10会使用更多的位编码，需要特殊处理 */
		if (cnt > 10)
			return best_repetition(ctx, hash_idx, cnt, buf_ptr, offset);

		/* 更新复制长度 */
		copy_len = cnt;
	}
}

static UINT best_repetition(struct IMP_CONTEXT *ctx, UINT hash_idx, UINT copy_len, BUFCPTR buf_ptr, UINT *offset)
{
	BUFCPTR rep_ptr, rep_end;
	BYTE pre_last_byte;
	UINT rep_len, off_max;
	SHORT cur_off, off_tab[COPY_LENGTH_MAX];

	DAssert(copy_len >= 2 && copy_len < COPY_LENGTH_MAX);

	/* 如果下一个哈希项已超出范围，直接返回当前结果 */
	if (ctx->hash_map[hash_idx + 1] >= buf_ptr - 1)
		return copy_len;

	/* 初始化偏移表及相关索引 */
	off_tab[0] = -1;			/* -1表示无效偏移 */
	off_tab[1] = 0;				/* 初始有效偏移 */
	cur_off = 0;				/* 当前偏移初始化 */
	off_max = 1;				/* 偏移表有效偏移数（不包括最初的-1，也可以认为是偏移表的当前最大有效索引值） */

	/* 初始化当前匹配结果 */
	rep_ptr = ctx->hash_map[hash_idx];
	rep_end = rep_ptr + copy_len;
	rep_len = copy_len;

	/* 循环寻找最优的匹配 */
	while (TRUE) {

		if (rep_len >= copy_len) {

			/* 更新最长匹配信息 */
			copy_len = rep_len;
			*offset = (UINT)(buf_ptr - rep_ptr - 1);

			/* 扩展偏移表以覆盖新的长度 */
			while (off_max < copy_len) {
				if (buf_ptr[off_max] != buf_ptr[cur_off]) {
					cur_off = off_tab[cur_off];
					if (cur_off >= 0)
						continue;
				}
				off_tab[++off_max] = ++cur_off;
			}
		}

		/* 获取下一个可能的重复长度 */
		rep_len = max(off_tab[rep_len], 0);

		/* 寻找下一个有效重复块 */
		do {
			rep_ptr = ctx->hash_map[++hash_idx];
			if (rep_ptr >= buf_ptr - 1)
				return copy_len;
		} while (rep_ptr + rep_len < rep_end);

		pre_last_byte = buf_ptr[copy_len - 2];

		/* 验证倒数第二个字节是否匹配 */
		if (pre_last_byte == rep_ptr[copy_len - 2]) {

			/* 如果新重复块超出原范围，重置长度 */
			if (rep_ptr + rep_len != rep_end) {
				rep_end = rep_ptr;
				rep_len = 0;
			}

		} else {

			/* 查找匹配倒数第二个字节的重复块 */
			do {
				rep_ptr = ctx->hash_map[++hash_idx];
				if (rep_ptr >= buf_ptr - 1)
					return copy_len;
			} while (rep_ptr[copy_len - 2] != pre_last_byte || rep_ptr[0] != buf_ptr[0]);

			/* 重置为最小匹配长度 */
			rep_end = rep_ptr + 2;
			rep_len = 2;
		}

		/* 扩展匹配长度 */
		while (*rep_end == buf_ptr[rep_len]) {
			if (++rep_len >= COPY_LENGTH_MAX) {
				*offset = (UINT)(buf_ptr - rep_ptr - 1);
				return COPY_LENGTH_MAX;
			}
			rep_end++;
		}
	}
}

/************************************************************************/

static BOOL init_explode(struct EXP_CONTEXT *ctx, BUFCPTR buf_ptr, UINT buf_size)
{
	INT i, ch;
	BYTE v;

	DAssert(ctx && buf_ptr);

	/* 压缩数据最小4字节 */
	if (buf_size < 4)
		return FALSE;

	/* 从压缩数据头部获得压缩类型和字典大小 */
	ctx->type = *buf_ptr++;
	ctx->dict = *buf_ptr++;
	buf_size -= 2;

	/* 仅支持两种类型 */
	if (ctx->type != IMPLODE_BINARY && ctx->type != IMPLODE_ASCII)
		return FALSE;

	/* 字典大小必须是1K/2K/4K */
	if (ctx->dict < IMPLODE_DICT_1K || ctx->dict > IMPLODE_DICT_4K)
		return FALSE;

	/* 初始化输入位缓冲 */
	init_bits(&ctx->bs, buf_ptr, buf_size);

	/* 创建“编码位串->(复制长度-2)”映射表 */
	for (v = 0; v < DCount(s_LenCode); v++) {
		for (i = s_LenCode[v]; i < DCount(ctx->len_tab); i += 1 << s_LenBits[v])
			ctx->len_tab[i] = v;
	}

	/* 创建“编码位串->(复制起始偏移-1)的高6位”映射表 */
	for (v = 0; v < DCount(s_OffCode); v++) {
		for (i = s_OffCode[v]; i < DCount(ctx->off_tab); i += 1 << s_OffBits[v])
			ctx->off_tab[i] = v;
	}

	/* 创建ASCII方式专用“编码位串->字符”映射表 */
	for (ch = 0; ch < DCount(s_ChBits); ch++) {

		/* 初始化编码位长 */
		v = s_ChBits[ch];

		/* 编码位长是否在8以内？ */
		if (v <= 8) {

			/* 表1中存放着编码长度为8位以内的“编码位串->字符”映射 */
			for (i = s_ChCode[ch]; i < DCount(ctx->ch_code_tab1); i += 1 << v)
				ctx->ch_code_tab1[i] = ch;

		} else {

			/* 取编码的低8位 */
			i = DLoByte(s_ChCode[ch]);

			/* 编码的低8位是否全为0？ */
			if (i) {

				/* 将表1中低8位所对应的字符标记为'\xff'以表示此种情况该查表2和表3 */
				/* 注意：由于'\xff'真正对应的编码为13个位0，因此查表时要先检查编码的低8位是否全为0来决定查表1还是表4以避免冲突 */
				ctx->ch_code_tab1[i] = '\xff';

				/* 编码的低6位是否全0？ */
				if (s_ChCode[ch] & 0x3f) {

					/* 表2中存放长度大于8且低6位不全为0的编码的“低4位以外编码位串->字符”映射 */
					/* 注意：由于长度大于8的编码的低4位都为0，而低6位不全为0的编码最大长度为12位，因此表2仅含有256（=2^(12-4)）对映射 */
					v -= 4;
					for (i = s_ChCode[ch] >> 4; i < DCount(ctx->ch_code_tab2); i += 1 << v)
						ctx->ch_code_tab2[i] = ch;

				} else {

					/* 表3中存放低6位全为0的编码的“低6位以外编码位串->字符”映射 */
					/* 注意：由于“低6位以外编码位串”最大长度为7位，因此表3仅含有128（=2^7）对映射 */
					v -= 6;
					for (i = s_ChCode[ch] >> 6; i < DCount(ctx->ch_code_tab3); i += 1 << v)
						ctx->ch_code_tab3[i] = ch;
				}

			} else {

				/* 表4中存放低8位全为0的编码的“低8位以外编码位串->字符”映射 */
				/* 注意：由于“低8位以外编码位串”最大长度为5位，因此表4仅含有32（=2^5）对映射 */
				v -= 8;
				for (i = DHiByte(s_ChCode[ch]); i < DCount(ctx->ch_code_tab4); i += 1 << v)
					ctx->ch_code_tab4[i] = ch;
			}
		}

		/* 记录字符对应的编码长度（映射表中实际存放的有效数据位数） */
		ctx->ch_bits[ch] = v;
	}

	/* 成功 */
	return TRUE;
}

static BUFPTR do_explode(struct EXP_CONTEXT *ctx, BUFPTR wrt_ptr, BUFPTR wrt_end)
{
	INT ch, bit, copy_len, copy_off;
	BUFPTR org, src, dest;

	DAssert(ctx && wrt_ptr && wrt_end && (wrt_ptr <= wrt_end));

	/* 首先保存写缓存的起始位置 */
	org = wrt_ptr;

	/* 解压处理循环 */
	while (TRUE) {

		/* 起始位表示后面的位串是字符数据编码还是复制命令 */
		bit = peek_bits(&ctx->bs, 1);
		if (!skip_bits(&ctx->bs, 1))
			return NULL;

		/* 如果是字符数据编码 */
		if (!bit) {

			/* 至少还应该有一个字节空余可写 */
			if (wrt_ptr >= wrt_end)
				return NULL;

			/* 解码一个字节的数据 */
			ch = get_one_byte(ctx);
			if (ch < 0)
				return NULL;

			/* 输出到缓冲 */
			*wrt_ptr++ = ch;
			continue;
		}

		/* 如果是复制命令则计算要从字典里复制的数据长度 */
		copy_len = get_copy_length(ctx);
		if (copy_len < 0)
			return NULL;

		/* 是否已经完成解压 */
		if (copy_len == COPY_LENGTH_END)
			break;

		/* 先保存当前写位置作为复制目标的起始位置 */
		dest = wrt_ptr;

		/* 更新复制后的写位置 */
		wrt_ptr += copy_len + 2;

		/* 如果缓冲不足则失败 */
		if (wrt_ptr > wrt_end)
			return NULL;

		/* 获得复制起始位置的偏移 */
		copy_off = get_copy_offset(ctx, copy_len);
		if (copy_off < 0 || copy_off >= wrt_ptr - org)
			return NULL;

		/* 计算实际要复制的字节数 */
		copy_len += 2;

		/* 计算复制源的起始位置 */
		src = dest - copy_off - 1;

		/*
			如copy_len为16且有以下数据的情况：
				ABCDEF????????????????
				^     ^
				src   dest
			期待结果为：
				ABCDEFABCDEFABCDEFABCD
			而memmove只能做到：
				ABCDEFABCDEF??????????
			因此下面的复制循环不能用单纯的memmove替代！
		*/

		/* 复制数据 */
		while (copy_len--)
			*dest++ = *src++;
	}

	/* 成功，返回最后的写位置 */
	return wrt_ptr;
}

static INT get_one_byte(struct EXP_CONTEXT *ctx)
{
	BYTE v, ch;

	DAssert(ctx);

	/* 如果是二进制压缩方式则接下来的8个位就是输出字节 */
	if (ctx->type == IMPLODE_BINARY) {
		ch = peek_bits(&ctx->bs, 8);
		return skip_bits(&ctx->bs, 8) ? ch : -1;
	}

	/* 如果是ASCII压缩则首先检查编码的低8位是否全0 */
	v = peek_bits(&ctx->bs, 8);

	/* 如果低8位全0则直接查表4，否则查表1 */
	if (v) {

		/* 先查表1 */
		ch = ctx->ch_code_tab1[v];

		/* 字符为'\xff'则特殊处理（见表1初始化时的说明） */
		if (ch == 0xff) {

			/* 如果低6位全为0则查表3，否则查表2 */
			if (v & 0x3f) {

				/* 此时低4位一定全为0，在此跳过 */
				if (!skip_bits(&ctx->bs, 4))
					return -1;

				/* 查表2，由于最大有256个索引因此查8个位 */
				ch = ctx->ch_code_tab2[peek_bits(&ctx->bs, 8)];

			} else {

				/* 跳过低6位的0 */
				if (!skip_bits(&ctx->bs, 6))
					return -1;

				/* 查表2，由于最大有128个索引因此查7个位 */
				ch = ctx->ch_code_tab3[peek_bits(&ctx->bs, 7)];
			}
		}

	} else {

		/* 跳过低8位的0 */
		if (!skip_bits(&ctx->bs, 8))
			return -1;

		/* 查表4，由于最大只有32个索引因此只查5个位 */
		ch = ctx->ch_code_tab4[peek_bits(&ctx->bs, 5)];
	}

	/* 跳过剩余的编码位 */
	return skip_bits(&ctx->bs, ctx->ch_bits[ch]) ? ch : -1;
}

static INT get_copy_length(struct EXP_CONTEXT *ctx)
{
	UINT copy_len, ex_len_bits, ex_len_code;

	DAssert(ctx);

	/* 获取编码对应的复制长度（不包括扩展部分） */
	copy_len = ctx->len_tab[peek_bits(&ctx->bs, 7)];

	/* 跳过缓存中对应的位串 */
	if (!skip_bits(&ctx->bs, s_LenBits[copy_len]))
		return -1;

	/* 获得复制长度编码扩展部分的位数 */
	ex_len_bits = s_ExLenBits[copy_len];

	/* 如果没有扩展部分（仅复制2-9个字节）则直接返回 */
	if (!ex_len_bits)
		return copy_len;

	/* 读取扩展部分的位串 */
	ex_len_code = peek_bits(&ctx->bs, ex_len_bits);

	/* 跳过缓存中对应的位串 */
	if (!skip_bits(&ctx->bs, ex_len_bits))
		return -1;

	/* 计算实际(复制长度-2)并返回 */
	return s_LenBase[copy_len] + ex_len_code;
}

static INT get_copy_offset(struct EXP_CONTEXT *ctx, UINT copy_len)
{
	UINT code, copy_off;

	DAssert(ctx && copy_len <= COPY_LENGTH_MAX);

	/* 接下来的2-8个位中存放着复制起始偏移编码，对应着(偏移值-1)的高6位 */
	code = ctx->off_tab[peek_bits(&ctx->bs, 8)];

	/* 跳过缓存中对应的位串 */
	if (!skip_bits(&ctx->bs, s_OffBits[code]))
		return -1;

	/* 仅复制2个字节时特殊处理 */
	if (!copy_len) {

		/* (偏移值-1)固定为8位，接下来的2个位即(偏移值-1)的低2位 */
		copy_off = (code << 2) | peek_bits(&ctx->bs, 2);

		/* 跳过缓存中对应的位串 */
		if (!skip_bits(&ctx->bs, 2))
			return -1;

	} else {

		/* (偏移值-1)为(6+dict)个位，接下来的dict个位即(偏移值-1)的低dict位 */
		copy_off = (code << ctx->dict) | peek_bits(&ctx->bs, ctx->dict);

		/* 跳过缓存中对应的位串 */
		if (!skip_bits(&ctx->bs, ctx->dict))
			return -1;
	}

	/* 返回实际(复制起始偏移-1) */
	return copy_off;
}

/************************************************************************/
