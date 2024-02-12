/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aalist.c	Kernel doubly linked list management

	When		Who	What
	25/11/13	ac	Creation

----------------------------------------------------------------------
*/
/*
----------------------------------------------------------------------
	Copyright (c) 2013-2023 Alain Chebrou - AdAstra-Soft . com
	All rights reserved.

	This file is part of the AdAstra Real Time Kernel distribution.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	- Redistributions of source code must retain the above copyright
	  notice, this list of conditions and the following disclaimer.

	- Redistributions in binary form must reproduce the above copyright
	  notice, this list of conditions and the following disclaimer in the
	  documentation and/or other materials provided with the distribution.

	- Neither the name of the copyright holder nor the
	  names of its contributors may be used to endorse or promote products
	  derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
----------------------------------------------------------------------
*/

#include "aa.h"				// For __STATIC_INLINE
#include "aalist.h"

//--------------------------------------------------------------------------------
// Add pNode after node pPos
// Do not check that pPos is actually in the list

void	aaListAddAfter	(aaListHead_t * pList, aaListNode_t * pPos, aaListNode_t * pNode)
{
	(void) pList ;
	AA_LIST_ASSERT ((pPos->pPrev != NULL)  &&  (pPos->pNext != NULL), AA_ERROR_LIST_1) ;	// Check pPos is not out of any list
	pNode->pPrev = pPos ;
	pPos->pNext->pPrev = pNode ;

	pNode->pNext = pPos->pNext ;
	pPos->pNext  = pNode ;

}

//--------------------------------------------------------------------------------
//	Remove node pNode from list pList
//	Return node pointer or NULL if list is empty

void	aaListRemove	(aaListHead_t * pList, aaListNode_t * pNode)
{
	if (pList->pPrev == (aaListNode_t *) pList)
	{
		AA_LIST_NOTIFY (AA_ERROR_LIST_2) ;
		return ;	// Empty list
	}

	// Check if pNode is out of any list
	AA_LIST_ASSERT ((pNode->pPrev != NULL)  &&  (pNode->pNext != NULL), AA_ERROR_LIST_3) ;

	pNode->pNext->pPrev = pNode->pPrev ;
	pNode->pPrev->pNext = pNode->pNext; 

	aaListClearNode (pNode) ;
}

//--------------------------------------------------------------------------------
