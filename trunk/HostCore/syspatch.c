/*
 *	syspatch.c is part of HostCore
 *	Copyright (C) 2008  Poison
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *	Description:	
 *	Author:			Poison <hbpoison@gmail.com>
 *	Date Created:	2008-05-15
 */

#include <pspkernel.h>
#include <pspsdk.h>
#include <psputilsforkernel.h>
#include <pspsysmem_kernel.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

int fw_version = FW_371;

int ( * getDevkitVersion )( void );

void initPatches()
{
	getDevkitVersion = ( void * )findProc( "sceSystemMemoryManager", "SysMemForKernel", 0x3fc9Ae6a );
	if ( !getDevkitVersion )
		getDevkitVersion = ( void * )findProc( "sceSystemMemoryManager", "SysMemForKernel", 0xee1718bc );
	fw_version = getDevkitVersion();
}

unsigned int getFindDriverAddr()
{
	tSceModule * pMod = ( tSceModule * )sceKernelFindModuleByName( "sceIOFileManager" );
	unsigned int addr = 0;
	if ( !pMod )
		return 0;
	if ( fw_version == FW_371 )
		addr = pMod->text_addr + 0x2844;
	else if ( fw_version == FW_380 || fw_version == FW_390 )
		addr = pMod->text_addr + 0x2808;
	return addr;
}

void getCtrlNids( unsigned int * nid )
{
	if ( fw_version == FW_371 )
	{
		nid[0] = 0x454455ac; //sceCtrlReadBufferPositive
		nid[1] = 0xc4aad55f; //sceCtrlPeekBufferPositive
	}
	else if ( fw_version == FW_380 || fw_version == FW_390 )
	{
		nid[0] = 0xad0510f6;
		nid[1] = 0xd65d4e9a;
	}
}

void getUtilsNids( unsigned int * nid )
{
	if ( fw_version == FW_371 )
	{
		nid[0] = 0xa3d5e142; //sceKernelExitVSHVSH
		nid[1] = 0xd9739b89; //sceKernelUnregisterExitCallback
		nid[2] = 0x659188e1; //sceKernelCheckExitCallback
		nid[3] = 0xa1a78C58; //sceKernelLoadModuleForLoadExecVSHDisc
	}
	else if ( fw_version == FW_380 || fw_version == FW_390 )
	{
		nid[0] = 0x62879ad8;
		nid[1] = 0xf1c99c38;
		nid[2] = 0x753ef37c;
		nid[3] = 0xc8f0090d;
	} 
}

unsigned int getKillMutexNid()
{
	return 0xf8170fbe;
}

void patchMemPartitionInfo()
{
	sceKernelSetDdrMemoryProtection( ( void * )0x88300000, 0x00100000, 0xf );
	tSceModule * pMod = ( tSceModule * )sceKernelFindModuleByName( "sceSystemMemoryManager" );
	// 0x02001021 move $v0 $s0
	_sw( 0x02001021, pMod->text_addr + 0x00001304 ); //for 3.71, 3.80, 3.90
	sceKernelIcacheInvalidateAll();
	sceKernelDcacheWritebackInvalidateAll();
	PspSysmemPartitionInfo info;
	memset( &info, 0, sizeof( PspSysmemPartitionInfo ) );
	info.size = sizeof( PspSysmemPartitionInfo );
	PspSysmemPartitionInfo * p_info = ( PspSysmemPartitionInfo * )sceKernelQueryMemoryPartitionInfo( 4, &info );
	p_info->startaddr = 0x08300000;
	p_info->attr = 0xf;
	//restore
	_sw( 0x00001021, pMod->text_addr + 0x00001304 ); //for 3.71, 3.80, 3.90
	sceKernelIcacheInvalidateAll();
	sceKernelDcacheWritebackInvalidateAll();
}

typedef struct PatchSav
{
	unsigned int addr;
	unsigned int val;
} PatchSav;

PatchSav LoadExecVSHCommon_ori[2];

void restoreLoadExecVSHCommon()
{
	_sw( LoadExecVSHCommon_ori[0].val, LoadExecVSHCommon_ori[0].addr );
	_sw( LoadExecVSHCommon_ori[1].val, LoadExecVSHCommon_ori[1].addr );
	sceKernelIcacheInvalidateAll();
	sceKernelDcacheWritebackInvalidateAll();
}

void * patchLoadExecVSHCommon( void * func )
{
	tSceModule * pMod = ( tSceModule * )sceKernelFindModuleByName( "sceLoadExec" );
	if ( fw_version == FW_371 )
		LoadExecVSHCommon_ori[0].addr = pMod->text_addr + 0x0000121c; //same in standare/slim
	else if ( fw_version == FW_380 || fw_version == FW_390 )
		LoadExecVSHCommon_ori[0].addr = pMod->text_addr + 0x000014cc; //same in standare/slim
	LoadExecVSHCommon_ori[1].addr = LoadExecVSHCommon_ori[0].addr + 4;
	LoadExecVSHCommon_ori[0].val = _lw( LoadExecVSHCommon_ori[0].addr );
	LoadExecVSHCommon_ori[1].val = _lw( LoadExecVSHCommon_ori[1].addr );
	MAKE_JUMP( LoadExecVSHCommon_ori[0].addr, func );
	_sw( NOP, LoadExecVSHCommon_ori[1].addr );
	sceKernelIcacheInvalidateAll();
	sceKernelDcacheWritebackInvalidateAll();
	return ( void * )LoadExecVSHCommon_ori[0].addr;
}

void wifiModulesPatch1()
{
	tSceModule * pMod = ( tSceModule * )sceKernelFindModuleByName( "sceThreadManager" );
	//a0 = 4, change partition id to 4
	if ( fw_version == FW_371 )
		_sw( 0x34040004, pMod->text_addr + 0x00010B30 );
	else if ( fw_version == FW_380 || fw_version == FW_390 )
		_sw( 0x34040004, pMod->text_addr + 0x00010CB8 );

	pMod = ( tSceModule * )sceKernelFindModuleByName( "sceModuleManager" );
	//a3 stack size 0x40000 -> 0x10000
	if ( fw_version == FW_371 )
		_sw( 0x3C070001, pMod->text_addr + 0x000076A0);
	else if ( fw_version == FW_380 || fw_version == FW_390 )
		_sw( 0x3C070001, pMod->text_addr + 0x00007C9C );
	sceKernelIcacheInvalidateAll();
	sceKernelDcacheWritebackInvalidateAll();
}

void wifiModulesPatch2()
{
	tSceModule * pMod = ( tSceModule * )sceKernelFindModuleByName( "sceNetInterface_Service" );
	//a2 partid = 4 of ifhandler
	_sw( 0x34050004, pMod->text_addr + 0x00001440 );  //for 3.71, 3.80, 3.90

	pMod = ( tSceModule * )sceKernelFindModuleByName( "sceNet_Library" );
	_sw( 0x34020002, pMod->text_addr + 0x00001800 ); //for 3.71, 3.80, 3.90
	_sw( 0xAFA20000, pMod->text_addr + 0x00001804 );
	_sw( 0x3C020000, pMod->text_addr + 0x0000180C );
	
	pMod = ( tSceModule * )sceKernelFindModuleByName( "sceModuleManager" );
	//a3 stack size 0x10000 -> 0x4000
	if ( fw_version == FW_371 )
		_sw( 0x34074000, pMod->text_addr + 0x000076A0);
	else if ( fw_version == FW_380 || fw_version == FW_390 )
		_sw( 0x34074000, pMod->text_addr + 0x00007C9C );
	sceKernelIcacheInvalidateAll();
	sceKernelDcacheWritebackInvalidateAll();
}

void wifiModulesPatch3()
{
	tSceModule * pMod = ( tSceModule * )sceKernelFindModuleByName( "sceModuleManager" );
	//restore
	if ( fw_version == FW_371 )
		_sw( 0x02403821, pMod->text_addr + 0x000076A0);
	else if ( fw_version == FW_380 || fw_version == FW_390 )
		_sw( 0x02403821, pMod->text_addr + 0x00007C9C );

	pMod = ( tSceModule * )sceKernelFindModuleByName( "sceThreadManager" );
	//restore
	if ( fw_version == FW_371 )
		_sw( 0x02402021, pMod->text_addr + 0x00010B30 );
	else if ( fw_version == FW_380 || fw_version == FW_390 )
		_sw( 0x02402021, pMod->text_addr + 0x00010CB8 );
	sceKernelIcacheInvalidateAll();
	sceKernelDcacheWritebackInvalidateAll();
}

