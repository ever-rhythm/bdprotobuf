/***************************************************************************
 * 
 * Copyright (c) 2012 Baidu.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file protobuf.h
 * @author zhangwei26(zhangwei26@baidu.com)
 * @date 2015/02/15 13:40:41
 * @version $Revision$ 
 * @brief 
 *  
 **/


#ifndef  __PROTOBUF_H_
#define  __PROTOBUF_H_
#include <stdio.h>
#include <stdint.h>


enum
{
	WIRE_TYPE_VARINT = 0,
	WIRE_TYPE_64BIT  = 1,
	WIRE_TYPE_LENGTH_DELIMITED = 2,
	WIRE_TYPE_32BIT = 5
};

typedef union
{
	float f_val;
	int32_t i_val;
	uint32_t u_val;
} fixed32_t;

typedef union
{
	double d_val;
	int64_t i_val;
	uint64_t u_val;
} fixed64_t;













#endif  //__PROTOBUF_H_

/* vim: set ts=4 sw=4 sts=4 tw=100 */
