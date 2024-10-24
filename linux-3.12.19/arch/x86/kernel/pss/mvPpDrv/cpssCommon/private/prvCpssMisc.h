/*******************************************************************************
*              (c), Copyright 2001, Marvell International Ltd.                 *
* THIS CODE CONTAINS CONFIDENTIAL INFORMATION OF MARVELL SEMICONDUCTOR, INC.   *
* NO RIGHTS ARE GRANTED HEREIN UNDER ANY PATENT, MASK WORK RIGHT OR COPYRIGHT  *
* OF MARVELL OR ANY THIRD PARTY. MARVELL RESERVES THE RIGHT AT ITS SOLE        *
* DISCRETION TO REQUEST THAT THIS CODE BE IMMEDIATELY RETURNED TO MARVELL.     *
* THIS CODE IS PROVIDED "AS IS". MARVELL MAKES NO WARRANTIES, EXPRESSED,       *
* IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY, COMPLETENESS OR PERFORMANCE.   *
********************************************************************************
* prvCpssMisc.h
*
* DESCRIPTION:
*       Miscellaneous operations for CPSS.
*
* FILE REVISION NUMBER:
*       $Revision$
*
*******************************************************************************/
#ifndef __prvCpssMisch
#define __prvCpssMisch

#include <cpss/extServices/os/gtOs/gtGenTypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*******************************************************************************
* prvCpssPeriodicFieldValueSet
*
* DESCRIPTION:
*       Set a value of a field into entry that lay in array of words.
*       the entry length is in bits and can be any non-zero number
*
* INPUTS:
*       dataArray - array of words to set the field value into.
*       entryIndex - entry index inside dataArray[]
*       entryNumBits - number of bits for each entry.(non-zero)
*       fieldStartBit - start bit of field . this bit is counted from the start
*                   of the entry(and not from entry[0]).
*       fieldNumBits - number of bits to set (field length) (1..32)
*       fieldValue - the value of the field that need to set.
*
* OUTPUTS:
*       dataArray - array of words with updated data on the field.
*
* RETURNS :
*       GT_OK           - on success
*       GT_BAD_PARAM    - bad parameter entryNumBits or fieldNumBits
*                         or fieldNumBits > entryNumBits
*       GT_BAD_PTR      - one of the parameters is NULL pointer
*       GT_FAIL         - otherwise
*
* COMMENTS:
*       None.
*
*******************************************************************************/
GT_STATUS prvCpssPeriodicFieldValueSet(
    INOUT GT_U32    dataArray[],
    IN GT_U32       entryIndex,
    IN GT_U32       entryNumBits,
    IN GT_U32       fieldStartBit,
    IN GT_U32       fieldNumBits,
    IN GT_U32       fieldValue
);

/*******************************************************************************
* prvCpssPeriodicFieldValueGet
*
* DESCRIPTION:
*       Get a value of a field from entry that lay in array of words.
*       the entry length is in bits and can be any non-zero number
*
* INPUTS:
*       dataArray - array of words to Get the field value from.
*       entryIndex - entry index inside dataArray[]
*       entryNumBits - number of bits for each entry.(non-zero)
*       fieldStartBit - start bit of field . this bit is counted from the start
*                   of the entry(and not from entry[0]).
*       fieldNumBits - number of bits to get (field length) (1..32)
*
* OUTPUTS:
*       fieldValue - (pointer to)the value of the field that need to set.
*
* RETURNS :
*       GT_OK           - on success
*       GT_BAD_PARAM    - bad parameter entryNumBits or fieldNumBits
*                         or fieldNumBits > entryNumBits
*       GT_BAD_PTR      - one of the parameters is NULL pointer
*       GT_FAIL         - otherwise
*
* COMMENTS:
*       None.
*
*******************************************************************************/
GT_STATUS prvCpssPeriodicFieldValueGet(
    IN GT_U32       dataArray[],
    IN GT_U32       entryIndex,
    IN GT_U32       entryNumBits,
    IN GT_U32       fieldStartBit,
    IN GT_U32       fieldNumBits,
    OUT GT_U32      *fieldValuePtr
);

/*******************************************************************************
*   prvCpssFieldValueGet
*
* DESCRIPTION:
*        get the value of field (up to 32 bits) that located in any start bit in
*       memory
* INPUTS:
*        startMemPtr - pointer to memory
*        startBit  - start bit of field (0..)
*        numBits   - number of bits of field (1..32)
* OUTPUTS:
*        valuePtr - (pointer to) value get
* Returns:
*
* COMMENTS:
*       GT_OK - no error
*       GT_BAD_PTR - on NULL pointer
*       GT_BAD_PARAM - on bad parameter : numBits > 32 or numBits == 0
*
*******************************************************************************/
GT_STATUS  prvCpssFieldValueGet
(
    IN GT_U32                  *startMemPtr,
    IN GT_U32                  startBit,
    IN GT_U32                  numBits,
    OUT GT_U32                 *valuePtr
);

/*******************************************************************************
*   prvCpssFieldValueSet
*
* DESCRIPTION:
*       set the value of field (up to 32 bits) that located in any start bit in
*       memory
* INPUTS:
*        startMemPtr - pointer to memory
*        startBit  - start bit of field (0..)
*        numBits   - number of bits of field (1..32)
*        value - the value to set
* Returns:
*
* COMMENTS:
*       GT_OK - no error
*       GT_BAD_PTR - on NULL pointer
*       GT_BAD_PARAM - on bad parameter : numBits > 32 or numBits == 0
*
* COMMENTS:
*       None.
*
*******************************************************************************/
GT_STATUS  prvCpssFieldValueSet
(
    IN GT_U32                  *startMemPtr,
    IN GT_U32                  startBit,
    IN GT_U32                  numBits,
    IN GT_U32                  value
);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __prvCpssMisch */

