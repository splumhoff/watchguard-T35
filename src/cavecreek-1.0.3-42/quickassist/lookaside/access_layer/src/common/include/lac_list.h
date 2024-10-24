/***************************************************************************
 *
 *   BSD LICENSE
 * 
 *   Copyright(c) 2007-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 * 
 *   Redistribution and use in source and binary forms, with or without 
 *   modification, are permitted provided that the following conditions 
 *   are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright 
 *       notice, this list of conditions and the following disclaimer in 
 *       the documentation and/or other materials provided with the 
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its 
 *       contributors may be used to endorse or promote products derived 
 *       from this software without specific prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 *  version: QAT1.7.Upstream.L.1.0.3-42
 *
 ***************************************************************************/

/**
 ***************************************************************************
 * @file lac_list.h
 *
 * @defgroup SalList
 *
 * @ingroup SalCtrl
 *
 * List structure and list functions.
 *
 ***************************************************************************/

#ifndef LAC_LIST_H
#define LAC_LIST_H

/**
 *****************************************************************************
 * @ingroup SalList
 *
 * @description
 *      List structure
 *
 *****************************************************************************/
typedef struct sal_list_s
{

    struct sal_list_s* next;
    void* pObj;

} sal_list_t;

/**
*******************************************************************************
 * @ingroup SalList
 *      Add a structure to tail of a list.
 *
 * @description
 *      Adds pObj to the tail of  list (if it exists). Allocates and sets a
 *      new sal_list_t structure.
 *
 * @param[in] list                      Pointer to the head pointer of the list.
 *                                      Can be NULL if no elements yet in list.
 * @param[in/out] tail                  Pointer to tail pointer of the list.
 *                                      Can be NULL if no elements yet in list.
 *                                      Is updated by the function to point to tail
 *                                      of list if pObj has been successfully added.
 * @param[in] pObj                      Pointer to structure to add to tail of
 *                                      the list.
 * @retval status
 *
 *****************************************************************************/
CpaStatus
SalList_add(sal_list_t** list, sal_list_t** tail, void* pObj);

/**
*******************************************************************************
 * @ingroup SalList
 *      Delete an element from the list.
 *
 * @description
 *      Delete an element from the list.
 *
 * @param[in/out] head_list             Pointer to the head pointer of the list.
 *                                      Can be NULL if no elements yet in list.
 *                                      Is updated by the function 
 *                                      to point to list->next if head_list is list.
 * @param[in/out] pre_list              Pointer to the previous pointer of the list.
 *                                      Can be NULL if no elements yet in list.
 *                                      (*pre_list)->next is updated 
 *                                      by the function to point to list->next
 * @param[in] list                      Pointer to list.
 *
 *****************************************************************************/
void
SalList_del(sal_list_t** head_list, sal_list_t** pre_list, sal_list_t* list);

/**
*******************************************************************************
 * @ingroup SalList
 *      Returns pObj element in list structure.
 *
 * @description
 *      Returns pObj associated with sal_list_t structure.
 *
 * @param[in] list                      Pointer to list element.
 * @retval void*                        pObj member of list structure.
 *
 *****************************************************************************/
void*
SalList_getObject(sal_list_t* list);

/**
*******************************************************************************
 * @ingroup SalList
 *      Set pObj to be NULL in the list.
 *
 * @description
 *      Set pObj of a element in the list to be NULL.
 *
 * @param[in] list                      Pointer to list element.
 *
 *****************************************************************************/
void
SalList_delObject(sal_list_t** list);

/**
*******************************************************************************
 * @ingroup SalList
 *      Returns next element in list structure.
 *
 * @description
 *      Returns next associated with sal_list_t structure.
 *
 * @param[in] list                      Pointer to list element.
 * @retval void*                        next member of list structure.
 *
 *****************************************************************************/
void*
SalList_next(sal_list_t*);

/**
*******************************************************************************
 * @ingroup SalList
 *      Frees memory associated with list structure.
 *
 * @description
 *      Frees memory associated with list structure and the Obj pointed to by
 *      the list.
 *
 * @param[in] list                      Pointer to list.
 *
 *****************************************************************************/
void
SalList_free(sal_list_t**);

#endif
