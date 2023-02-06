/*
*  Copyright (C) 2007 Jolien Creighton, Kipp Cannon, Patrick Brady, Saikat Ray-Majumder, Xavier Siemens
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with with program; see the file COPYING. If not, write to the
*  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
*  MA  02110-1301  USA
*/


#include <string.h>
#include <lal/Date.h>
#include <lal/LIGOMetadataTables.h>
#include <lal/SnglBurstUtils.h>
#include <lal/XLALError.h>


/**
 * Assign event_id values to the entries in a SnglBurst linked list.  All
 * SnglBurst rows in the list will be blamed on the given process_id, and
 * assigned sequential event_ids starting with the given event_id.  The
 * return value is the next event_id after the last one assigned to a row
 * in the list.
 */
long XLALSnglBurstAssignIDs(
	SnglBurst *head,
	long process_id,
	long event_id
)
{
	for(; head; head = head->next) {
		head->process_id = process_id;
		head->event_id = event_id++;
	}
	return event_id;
}


/**
 * Compute the length of a linked list of SnglBurst objects.
 */
int XLALSnglBurstTableLength(SnglBurst *head)
{
	int length;

	for(length = 0; head; head = head->next)
		length++;

	return length;
}


/**
 * Sort a list of SnglBurst events into increasing order according to the
 * supplied comparison function.
 */
SnglBurst **XLALSortSnglBurst(
	SnglBurst **head,
	int (*comparefunc)(const SnglBurst * const *, const SnglBurst * const *)
)
{
	int i;
	int length;
	SnglBurst *event;
	SnglBurst **array;
	SnglBurst **next;

	/* empty list --> no-op */
	if(!*head)
		return head;

	/* construct an array of pointers into the list */
	length = XLALSnglBurstTableLength(*head);
	array = XLALCalloc(length, sizeof(*array));
	if(!array)
		XLAL_ERROR_NULL(XLAL_EFUNC);
	for(i = 0, event = *head; event; event = event->next)
		array[i++] = event;

	/* sort the array using the specified function */
	qsort(array, length, sizeof(*array), (int(*)(const void *, const void *)) comparefunc);

	/* re-link the list according to the sorted array */
	next = head;
	for(i = 0; i < length; i++, next = &(*next)->next)
		*next = array[i];
	*next = NULL;

	/* free the array */
	XLALFree(array);

	/* success */
	return head;
}


/**
 * Compare the peak times and SNRs of two SnglBurst events.
 */


int XLALCompareSnglBurstByPeakTimeAndSNR(
	const SnglBurst * const *a,
	const SnglBurst * const *b
)
{
	INT8 ta = XLALGPSToINT8NS(&(*a)->peak_time);
	INT8 tb = XLALGPSToINT8NS(&(*b)->peak_time);
	float snra = (*a)->snr;
	float snrb = (*b)->snr;

	if(ta > tb)
		return 1;
	if(ta < tb)
		return -1;
	/* ta == tb */
	if(snra > snrb)
		return 1;
	if(snra < snrb)
		return -1;
	/* snra == snrb */
	return 0;
}


/**
 * Assign simulation_id values to the entries in a sim_burst linked list.
 * All sim_burst rows in the list will be blamed on the given process_id,
 * and assigned simulation_ids in order starting with the given
 * simulation_id.  The return value is the next simulation_id after the
 * last one assigned to a row in the list.
 */
long XLALSimBurstAssignIDs(
	SimBurst *sim_burst,
	long process_id,
	long time_slide_id,
	long simulation_id)
{
	for(; sim_burst; sim_burst = sim_burst->next) {
		sim_burst->process_id = process_id;
		sim_burst->time_slide_id = time_slide_id;
		sim_burst->simulation_id = simulation_id++;
	}
	return simulation_id;
}
