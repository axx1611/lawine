/************************************************************************/
/* File Name   : scm.hpp                                                */
/* Creator     : ax.minaduki@gmail.com                                  */
/* Create Time : April 11st, 2010                                       */
/* Module      : Lawine library                                         */
/* Descript    : DScm class declaration                                 */
/************************************************************************/

#ifndef __SD_LAWINE_DATA_SCM_HPP__
#define __SD_LAWINE_DATA_SCM_HPP__

/************************************************************************/

#include <common.h>
#include <image.hpp>
#include <lawinedef.h>
#include "chk.hpp"
#include "mpq.hpp"
#include "tileset.hpp"
#include "../misc/isomap.h"

/************************************************************************/

class DScm {

public:

	DScm();
	~DScm();

	BOOL Create(CONST DTileset &ts, INT def, CONST SIZE &size);
	BOOL Load(STRCPTR name, BOOL for_edit);
	BOOL Save(STRCPTR name) CONST;
	VOID Clear(VOID);

	BOOL GetEditable(VOID) CONST;
	WORD GetVersion(VOID) CONST;
	INT GetEra(VOID) CONST;
	BOOL GetSize(SIZE &size) CONST;
	LTILECPTR GetTileData(VOID) CONST;
	LISOMCPTR GetIsoMapData(VOID) CONST;

	IMGCPTR GetMinimap(VOID) CONST;
	BOOL GenMinimap(CONST DTileset &ts);

	BOOL IsoBrush(INT brush, CONST POINT &tile_pos);
	BOOL UpdateTile(VOID);

protected:

	struct ERA_DATA {
		WORD era:3;
		WORD unused:13;
	};

	struct DIM_DATA {
		WORD width;
		WORD height;
	};

	struct DD2_DATA {
		WORD dd_no;				/* 关联到dddata.bin的索引号 */
		WORD x;					/* Doodad正中心所在X位置（相对于地图最左侧的像素值） */
		WORD y;					/* Doodad正中心所在Y位置（相对于地图最上端的像素值） */
		BYTE owner;				/* 所属玩家，仅对炮台等东西才有意义，0表示中立 */
		BYTE disabled:1;		/* 炮台不可攻击、门打开等 */
		BYTE unused:7;
	};

	struct THG2_DATA {
		WORD thg_no;			/* 如果是Unit则为units.dat中的索引号，否则为sprites.dat中的索引号 */
		WORD x;					/* Doodad正中心所在X位置（相对于地图最左侧的像素值） */
		WORD y;					/* Doodad正中心所在Y位置（相对于地图最上端的像素值） */
		BYTE owner;				/* 所属玩家，仅对炮台等东西才有意义，0表示中立 */
		BYTE unused;
		WORD unknown:13;
		WORD unit:1;			/* 是否Unit类型 */
		WORD flipped:1;			/* 是否左右翻转位图 */
		WORD disabled:1;		/* 炮台不可攻击、门打开等 */
	};

	struct VCODE {
		DWORD code[256];
		BYTE op[16];
	};

	struct VDATA {
		BYTE unused1[8];
		BYTE owner;
		BYTE side;
		BYTE unused2[26];
	};

	BOOL LoadMap(VOID);
	BOOL Verify(VOID);
	BOOL ReadVersion(VOID);
	BOOL ReadEra(VOID);
	BOOL ReadMapSize(VOID);
	BOOL ReadTile(VOID);
	BOOL ReadIsoMap(VOID);
	BOOL ReadThingy(VOID);

	BOOL MakeMinimap(CONST DTileset &ts);
	VOID MakeSmallMinimap(CONST DTileset &ts, CONST SIZE &size);
	VOID MakeMediumMinimap(CONST DTileset &ts, CONST SIZE &size);
	VOID MakeLargeMinimap(CONST DTileset &ts, CONST SIZE &size);

	static DWORD CalcVerifyHash(CONST VCODE *vcode, VCPTR vdata, UINT vdata_size);

	BOOL			m_Valid;
	BOOL			m_Edit;
	WORD			m_Version;
	LTILEPTR		m_Tile;
	ISOM_MAP		m_IsoMap;
	DChk			m_Chk;
	DImage			m_Minimap;
	DMpq			*m_Archive;

	static CONST VCODE VERIFY_CODE;

};

/************************************************************************/

#endif	/* __SD_LAWINE_DATA_SCM_HPP__ */
