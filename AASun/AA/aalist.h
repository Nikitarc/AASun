/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aalist.h	Some list implementations
				aaListHead_t	Kernel doubly linked list management
				aaStackHead_t		Generic stack
				aaFifoHead_t	Generic FIFO

	When		Who	What
	25/11/13	ac	Creation
	02/08/17	ac	Change to aaListHead_t, remove aaListCount
	30/12/17	ac	Add FIFO list
					Add aaListIsEnd aaListIsInUse aaListIsLast aaListGetFirst aaListGetNext aaListGetPrev

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

/*
Doubly linked list:
A trick is used to simplify insertions and remove from the list (operations done without a test).
The list head structure is identical to the list node structure
When the list is empty, the pointers pNext and pPrev both point to the head of the list.
=> the list is in practice never empty, but there is no useful element if pNext == @ head of list
=> the list has only one element when: pList->pNext == pList->pPrev and pNext! == @ head of list

Only 2 operations are necessary:

Insert after a given node: aaListAddAfter (pPos, pNew), where it follows:
- Insert at the head of the list: aaListAddAfter (pList, pNew)
- Insert at the end  of the list: aaListAddAfter (pList->pPrev, pNew)
- It is always possible to insert

Remove node after a given node: aaListRemove (pNodeToRemove), where it follows:
- Removing the 1st  item from the list: aaListRemove (pList->pNext)
- Removing the last item from the list: aaListRemove (pList->pPrev)
- Nothing to remove if pList->pNext == pList
*/


#if ! defined AA_LIST_H_
#define AA_LIST_H_

//--------------------------------------------------------------------------------
//	Define an assert specific to list module.
//	For performance reason it may be necessary to not enable theses checks

#if (AA_WITH_LISTCHECK == 1)
	#include	"aaerror.h"
	#define		AA_LIST_NOTIFY(errorNumber)			AA_ERRORNOTIFY (errorNumber)
	#define		AA_LIST_ASSERT(test, errorNumber)	AA_ERRORASSERT ((test), (errorNumber))
#else
	#define		AA_LIST_NOTIFY(errorNumber)
	#define		AA_LIST_ASSERT(test, errorNumber)
#endif

//--------------------------------------------------------------------------------
// Doubly Linked List node structure
// It is checked that pNext and pPrev of a node added to a list are NULL.
// It is the responsibility of the user to nullify theses pointers with aaListClearNode()

// The list never use pOwner or value in a node, these are user data.

typedef	void (* aaListFnPtr) (void) ;

typedef struct aaListNode_s
{
	struct aaListNode_s	* pNext ;	// Mandatory first in struct
	struct aaListNode_s	* pPrev ;
	union
	{
		void		* pOwner ;		// Pointer to the object that contain the node (TCB, SCB ...)
		aaListFnPtr	pFn ;			// Pointer to a function
	} ptr ;
	uint32_t			value ;		// May be used to contain timeout value, priority value ...

} aaListNode_t ;

typedef struct aaListHead_s
{
	aaListNode_t	* pNext ;	// Mandatory first in struct
	aaListNode_t	* pPrev ;

} aaListHead_t ;

//--------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif


void	aaListAddAfter	(aaListHead_t * pList, aaListNode_t * pPos, aaListNode_t * pNode) ;
void	aaListRemove	(aaListHead_t * pList, aaListNode_t * pNode) ;

//--------------------------------------------------------------------------------
//	Initialize empty list

__STATIC_INLINE void	aaListInit (aaListHead_t * pList)
{
	pList->pNext = pList->pPrev = (aaListNode_t *) pList ;
}

//--------------------------------------------------------------------------------

__STATIC_INLINE void aaListClearNode (aaListNode_t * pNode)
{
	pNode->pNext = NULL ;
	pNode->pPrev = NULL ;
}

//--------------------------------------------------------------------------------
// Add pNode at head of list pList

__STATIC_INLINE void aaListAddHead (aaListHead_t * pList, aaListNode_t * pNode)
{
	aaListAddAfter (pList, (aaListNode_t *) pList, pNode) ;
}

//--------------------------------------------------------------------------------
// Add pNode at end of list pList

__STATIC_INLINE void aaListAddTail (aaListHead_t * pList, aaListNode_t * pNode)
{
	aaListAddAfter (pList, pList->pPrev, pNode) ;
}

//--------------------------------------------------------------------------------
//	Check list content and return 1 if list is empty

__STATIC_INLINE uint8_t aaListIsEmpty (aaListHead_t * pList)
{
	return (pList->pNext == (aaListNode_t *) pList) ? 1 : 0 ;
}


//--------------------------------------------------------------------------------
//	Return 1 if count of elements in list is one

__STATIC_INLINE uint32_t aaListCount1 (aaListHead_t * pList)
{
	if ((pList->pNext != (aaListNode_t *) pList)  &&  pList->pNext == pList->pPrev)
	{
		return 1 ;
	}
	return 0 ;
}

//--------------------------------------------------------------------------------
//	Return 1 if pNode is the address of pList (end of list iterator)

__STATIC_INLINE uint32_t aaListIsEnd (aaListHead_t * pList, aaListNode_t * pNode)
{
	return (pNode == (aaListNode_t *) pList) ? 1 : 0 ;
}

//--------------------------------------------------------------------------------
//	Return 1 if pNode is in a list (pNext not NULL)

__STATIC_INLINE uint32_t aaListIsInUse (aaListNode_t * pNode)
{
	return (pNode->pNext == NULL) ? 0 : 1 ;
}

//--------------------------------------------------------------------------------
//	Return 1 if pNode is the last item in the pList

__STATIC_INLINE uint32_t aaListIsLast (aaListHead_t * pList, aaListNode_t * pNode)
{
	return (pNode->pNext == (aaListNode_t *) pList) ? 1 : 0 ;
}

//--------------------------------------------------------------------------------
//	Return the first item of pList (the item at head of list)

__STATIC_INLINE aaListNode_t * aaListGetFirst (aaListHead_t * pList)
{
	return pList->pNext ;
}

//--------------------------------------------------------------------------------
//	Return the next item of an item

__STATIC_INLINE aaListNode_t * aaListGetNext (aaListNode_t * pNode)
{
	return pNode->pNext ;
}

//--------------------------------------------------------------------------------
//	Return the previous item of an item

__STATIC_INLINE aaListNode_t * aaListGetPrev (aaListNode_t * pNode)
{
	return pNode->pPrev ;
}

//--------------------------------------------------------------------------------
//	Remove item at head of list

__STATIC_INLINE aaListNode_t	* aaListRemoveHead	(aaListHead_t * pList)
{
	aaListNode_t	* pNode = pList->pNext ;

	// If pointers are NULL, pList is not a list
	AA_LIST_ASSERT (((pNode != NULL)  &&  (pList->pPrev != NULL)), AA_ERROR_LIST_4) ;

	if (pNode == (aaListNode_t *) pList)
	{
		pNode = NULL ;	// Empty list
	}
	else
	{
		aaListRemove (pList, pNode) ;
	}
	return pNode ;
}


//--------------------------------------------------------------------------------
//	Remove item at tail of list

__STATIC_INLINE aaListNode_t	* aaListRemoveTail	(aaListHead_t * pList)
{
	aaListNode_t	* pNode = pList->pPrev ;

	// If pointers are NULL, it is not a list
	AA_LIST_ASSERT (((pNode != NULL)  &&  (pList->pNext != NULL)), AA_ERROR_LIST_5) ;

	if (pNode == (aaListNode_t *) pList)
	{
		pNode =  NULL ;	// Empty list
	}
	else
	{
		aaListRemove (pList, pNode) ;
	}
	return pNode ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Simple linked list, used as stack: Last In First Out (LIFO)

//	Stack head or node
typedef struct aaStackNode_s
{
	struct aaStackNode_s * pNext ;

} aaStackNode_t ;

//--------------------------------------------------------------------------------

__STATIC_INLINE void		aaStackInitHead (aaStackNode_t * pHead)
{
	pHead->pNext = pHead ;
}

//--------------------------------------------------------------------------------

__STATIC_INLINE void		aaStackInitNode (aaStackNode_t * pNode)
{
	pNode->pNext = NULL ;
}

//--------------------------------------------------------------------------------
//	Return 1 if the list is empty

__STATIC_INLINE uint32_t	aaStackIsEmpty (aaStackNode_t * pHead)
{
	AA_LIST_ASSERT (((pHead != NULL)  &&  (pHead->pNext != NULL)), AA_ERROR_LIST_6) ;
	return (pHead->pNext == pHead) ? 1 : 0 ;
}

//--------------------------------------------------------------------------------
//	Insert pNode 

__STATIC_INLINE void	aaStackPush (aaStackNode_t * pHead, aaStackNode_t * pNode)
{
	AA_LIST_ASSERT (((pHead != NULL)  &&  (pHead->pNext != NULL)), AA_ERROR_LIST_7) ;
	pNode->pNext = pHead->pNext ;
	pHead->pNext =  pNode ;
}

//--------------------------------------------------------------------------------
// Remove top node
// Not allowed if stack is empty

__STATIC_INLINE aaStackNode_t *	aaStackPop (aaStackNode_t * pHead)
{
	aaStackNode_t * pNode ;

	AA_LIST_ASSERT (((pHead != NULL)  &&  (pHead->pNext != NULL)), AA_ERROR_LIST_8) ;
	AA_LIST_ASSERT ((pHead->pNext != pHead), AA_ERROR_LIST_9) ;
	pNode = pHead->pNext ;
	pHead->pNext = pNode->pNext ;
	pNode->pNext = NULL ;

	return pNode ;
}

//--------------------------------------------------------------------------------
// Get top node of stack, without removing the node
// Return NULL if stack is empty

__STATIC_INLINE aaStackNode_t *	aaStackPeek (aaStackNode_t * pHead)
{
	aaStackNode_t * pNode = NULL ;

	// Check that pointer is not null, and it is not a pointer to a stack node
	AA_LIST_ASSERT (((pHead != NULL)  &&  (pHead->pNext != NULL)), AA_ERROR_LIST_10) ;

	if (pHead->pNext != pHead)
	{
		pNode = pHead->pNext ;
	}

	return pNode ;
}

//--------------------------------------------------------------------------------
// Get last node of stack
// If list is empty return pHead
// So can always add after the returned node to add at end of pHead stack
// Warning: not deterministic because of a loop to walk the stack

__STATIC_INLINE aaStackNode_t *	aaStackPeekLast (aaStackNode_t * pHead)
{
	 aaStackNode_t * pNode ;

	// Check that pointer is not null, and it is not a pointer to a stack node
	AA_LIST_ASSERT (((pHead != NULL)  &&  (pHead->pNext != NULL)), AA_ERROR_LIST_11) ;

	pNode = pHead ;
	while (pNode->pNext != pHead)
	{
		pNode = pNode->pNext ;
	}

	return pNode ;
}

//--------------------------------------------------------------------------------
// Count of items in pHead stack
// Warning: not deterministic because of a loop to walk the stack

__STATIC_INLINE uint32_t	aaStackCount (aaStackNode_t * pHead)
{
	aaStackNode_t * pNode ;
	uint32_t    count = 0 ;

	// Check that pointer is not null, and it is not a pointer to a stack node
	AA_LIST_ASSERT (((pHead != NULL)  &&  (pHead->pNext != NULL)), AA_ERROR_LIST_12) ;

	pNode = pHead->pNext ;
	while (pNode != pHead)
	{
		count++ ;
		pNode = pNode->pNext ;
	}

	return count ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Simple linked list used as First In First Out (FIFO)

//	Simple Linked List (FIFO) node
typedef struct aaFifoNode_s
{
	struct aaFifoNode_s * pNext ;

} aaFifoNode_t ;

//	Simple Linked List (FIFO) head
//	Insert using pLast, remove using pFirst
typedef struct aaFifoHead_s
{
	aaFifoNode_t * pFirst ;
	aaFifoNode_t * pLast ;

} aaFifoHead_t ;

//--------------------------------------------------------------------------------

__STATIC_INLINE void		aaFifoInitHead (aaFifoHead_t * pHead)
{
	pHead->pFirst = NULL ;
}

//--------------------------------------------------------------------------------

__STATIC_INLINE void		aaFifoInitNode (aaFifoNode_t * pNode)
{
	pNode->pNext = NULL ;
}

//--------------------------------------------------------------------------------
//	Return 1 if the list is empty

__STATIC_INLINE uint32_t	aaFifoIsEmpty (aaFifoHead_t * pHead)
{
	return (pHead->pFirst == NULL) ? 1 : 0 ;
}

//--------------------------------------------------------------------------------
//	Insert pNode to pHead list 

__STATIC_INLINE void	aaFifoAdd (aaFifoHead_t * pHead, aaFifoNode_t * pNode)
{
	if (pHead->pFirst == NULL)
	{
		pHead->pFirst = pNode ;
	}
	else
	{
		pHead->pLast->pNext = pNode ;
	}
	pHead->pLast = pNode ;
	pNode->pNext = NULL ;
}

//--------------------------------------------------------------------------------
// Remove node from pHead list
// Not allowed if FIFO is empty

__STATIC_INLINE aaFifoNode_t *	aaFifoRemove (aaFifoHead_t * pHead)
{
	aaFifoNode_t * pNode ;

	AA_LIST_ASSERT ((pHead->pFirst != NULL), AA_ERROR_LIST_13) ; 	// Empty FIFO
	pNode = pHead->pFirst ;
	pHead->pFirst = pNode->pNext ;
	pNode->pNext = NULL ;

	return pNode ;
}

//--------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif
//--------------------------------------------------------------------------------
#endif	// AA_LIST_H_
