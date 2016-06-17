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

extern mach_msg_return_t _hurd_intr_rpc_mach_msg (mach_msg_header_t *,
  mach_msg_option_t, mach_msg_size_t, mach_msg_size_t,
  mach_port_t, mach_msg_timeout_t, mach_port_t);

/* Routine gsync_wait */
mig_external kern_return_t gsync_wait
(
	mach_port_t task,
	vm_offset_t addr,
	unsigned val1,
	unsigned val2,
	natural_t msec,
	int flags
)
{
	typedef struct {
		mach_msg_header_t Head;
		mach_msg_type_t addrType;
		vm_offset_t addr;
		mach_msg_type_t val1Type;
		unsigned val1;
		mach_msg_type_t val2Type;
		unsigned val2;
		mach_msg_type_t msecType;
		natural_t msec;
		mach_msg_type_t flagsType;
		int flags;
	} Request;

	typedef struct {
		mach_msg_header_t Head;
		mach_msg_type_t RetCodeType;
		kern_return_t RetCode;
	} Reply;

	union {
		Request In;
		Reply Out;
	} Mess;

	Request *InP = &Mess.In;
	Reply *OutP = &Mess.Out;

	mach_msg_return_t msg_result;

	const mach_msg_type_t addrType = {
		/* msgt_name = */		2,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	const mach_msg_type_t val1Type = {
		/* msgt_name = */		2,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	const mach_msg_type_t val2Type = {
		/* msgt_name = */		2,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	const mach_msg_type_t msecType = {
		/* msgt_name = */		2,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	const mach_msg_type_t flagsType = {
		/* msgt_name = */		2,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	InP->addrType = addrType;

	InP->addr = addr;

	InP->val1Type = val1Type;

	InP->val1 = val1;

	InP->val2Type = val2Type;

	InP->val2 = val2;

	InP->msecType = msecType;

	InP->msec = msec;

	InP->flagsType = flagsType;

	InP->flags = flags;

	InP->Head.msgh_bits =
		MACH_MSGH_BITS(19, MACH_MSG_TYPE_MAKE_SEND_ONCE);
	/* msgh_size passed as argument */
	InP->Head.msgh_request_port = task;
	InP->Head.msgh_reply_port = mig_get_reply_port();
	InP->Head.msgh_seqno = 0;
	InP->Head.msgh_id = 4204;

    msg_result = _hurd_intr_rpc_mach_msg (&InP->Head, MACH_SEND_MSG |
      MACH_RCV_MSG | MACH_RCV_INTERRUPT | MACH_SEND_INTERRUPT, 64, 
      sizeof (Reply), InP->Head.msgh_reply_port,
      MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	if (msg_result != MACH_MSG_SUCCESS) {
		mig_dealloc_reply_port(InP->Head.msgh_reply_port);
		return msg_result;
	}
	mig_put_reply_port(InP->Head.msgh_reply_port);

	if (mig_unlikely (OutP->Head.msgh_id != 4304)) {
		if (OutP->Head.msgh_id == MACH_NOTIFY_SEND_ONCE)
			return MIG_SERVER_DIED;
		else {
			mig_dealloc_reply_port(InP->Head.msgh_reply_port);
			return MIG_REPLY_MISMATCH;
		}
	}

	return OutP->RetCode;
}

/* SimpleRoutine gsync_wake */
mig_external kern_return_t gsync_wake
(
	mach_port_t task,
	vm_offset_t addr,
	unsigned val,
	int flags
)
{
	typedef struct {
		mach_msg_header_t Head;
		mach_msg_type_t addrType;
		vm_offset_t addr;
		mach_msg_type_t valType;
		unsigned val;
		mach_msg_type_t flagsType;
		int flags;
	} Request;

	union {
		Request In;
	} Mess;

	Request *InP = &Mess.In;


	const mach_msg_type_t addrType = {
		/* msgt_name = */		2,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	const mach_msg_type_t valType = {
		/* msgt_name = */		2,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	const mach_msg_type_t flagsType = {
		/* msgt_name = */		2,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	InP->addrType = addrType;

	InP->addr = addr;

	InP->valType = valType;

	InP->val = val;

	InP->flagsType = flagsType;

	InP->flags = flags;

	InP->Head.msgh_bits =
		MACH_MSGH_BITS(19, 0);
	/* msgh_size passed as argument */
	InP->Head.msgh_request_port = task;
	InP->Head.msgh_reply_port = MACH_PORT_NULL;
	InP->Head.msgh_seqno = 0;
	InP->Head.msgh_id = 4205;

	return mach_msg(&InP->Head, MACH_SEND_MSG|MACH_MSG_OPTION_NONE, 48, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
}

/* SimpleRoutine gsync_requeue */
mig_external kern_return_t gsync_requeue
(
	mach_port_t task,
	vm_offset_t src_addr,
	vm_offset_t dst_addr,
	boolean_t wake_one,
	int flags
)
{
	typedef struct {
		mach_msg_header_t Head;
		mach_msg_type_t src_addrType;
		vm_offset_t src_addr;
		mach_msg_type_t dst_addrType;
		vm_offset_t dst_addr;
		mach_msg_type_t wake_oneType;
		boolean_t wake_one;
		mach_msg_type_t flagsType;
		int flags;
	} Request;

	union {
		Request In;
	} Mess;

	Request *InP = &Mess.In;


	const mach_msg_type_t src_addrType = {
		/* msgt_name = */		2,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	const mach_msg_type_t dst_addrType = {
		/* msgt_name = */		2,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	const mach_msg_type_t wake_oneType = {
		/* msgt_name = */		0,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	const mach_msg_type_t flagsType = {
		/* msgt_name = */		2,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_deallocate = */		FALSE,
		/* msgt_unused = */		0
	};

	InP->src_addrType = src_addrType;

	InP->src_addr = src_addr;

	InP->dst_addrType = dst_addrType;

	InP->dst_addr = dst_addr;

	InP->wake_oneType = wake_oneType;

	InP->wake_one = wake_one;

	InP->flagsType = flagsType;

	InP->flags = flags;

	InP->Head.msgh_bits =
		MACH_MSGH_BITS(19, 0);
	/* msgh_size passed as argument */
	InP->Head.msgh_request_port = task;
	InP->Head.msgh_reply_port = MACH_PORT_NULL;
	InP->Head.msgh_seqno = 0;
	InP->Head.msgh_id = 4206;

	return mach_msg(&InP->Head, MACH_SEND_MSG|MACH_MSG_OPTION_NONE, 56, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
}
