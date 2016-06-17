#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#define EXPORT_BOOLEAN
#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <mach/mach_types.h>
#include <mach/mig_errors.h>
#include <mach/mig_support.h>
#include <mach/msg_type.h>

#ifndef	mig_internal
#define	mig_internal	static
#endif

#ifndef	mig_external
#define mig_external
#endif

#ifndef	mig_unlikely
#define	mig_unlikely(X)	__builtin_expect (!! (X), 0)
#endif

#define msgh_request_port	msgh_remote_port
#define msgh_reply_port		msgh_local_port

#include <mach/std_types.h>
#include <mach/mach_types.h>
#include <mach_debug/mach_debug_types.h>

/* SimpleRoutine thread_terminate_release2 */
mig_external kern_return_t thread_terminate_release2
(
	mach_port_t thread,
	mach_port_t task,
	mach_port_t thread_name,
	mach_port_t reply_port,
	vm_address_t address,
	vm_size_t size,
	vm_offset_t death_event
)
{
	typedef struct {
		mach_msg_header_t Head;
		mach_msg_type_t taskType;
		mach_port_t task;
		mach_msg_type_t thread_nameType;
		mach_port_t thread_name;
		mach_msg_type_t reply_portType;
		mach_port_t reply_port;
		mach_msg_type_t addressType;
		vm_address_t address;
		mach_msg_type_t sizeType;
		vm_size_t size;
		mach_msg_type_t death_eventType;
		vm_offset_t death_event;
	} Request;

	union {
		Request In;
	} Mess;

	Request *InP = &Mess.In;


	const mach_msg_type_t taskType = {
		/* msgt_name = */		19,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	const mach_msg_type_t thread_nameType = {
		/* msgt_name = */		15,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	const mach_msg_type_t reply_portType = {
		/* msgt_name = */		15,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	const mach_msg_type_t addressType = {
		/* msgt_name = */		2,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	const mach_msg_type_t sizeType = {
		/* msgt_name = */		2,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	const mach_msg_type_t death_eventType = {
		/* msgt_name = */		2,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	InP->taskType = taskType;

	InP->task = task;

	InP->thread_nameType = thread_nameType;

	InP->thread_name = thread_name;

	InP->reply_portType = reply_portType;

	InP->reply_port = reply_port;

	InP->addressType = addressType;

	InP->address = address;

	InP->sizeType = sizeType;

	InP->size = size;

	InP->death_eventType = death_eventType;

	InP->death_event = death_event;

	InP->Head.msgh_bits = MACH_MSGH_BITS_COMPLEX|
		MACH_MSGH_BITS(19, 0);
	/* msgh_size passed as argument */
	InP->Head.msgh_request_port = thread;
	InP->Head.msgh_reply_port = MACH_PORT_NULL;
	InP->Head.msgh_seqno = 0;
	InP->Head.msgh_id = 4207;

	return mach_msg(&InP->Head, MACH_SEND_MSG|MACH_MSG_OPTION_NONE, 72, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
}
