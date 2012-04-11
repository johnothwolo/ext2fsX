#
#   File:       QKernelTraceBuffer.gdb
# 
#   Contains:   GDB commands to dump the kernel trace buffer.
# 
#   Written by: DTS
# 
#   Copyright:  Copyright (c) 2003 by Apple Computer, Inc., All Rights Reserved.
# 
#   Disclaimer: IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
#               ("Apple") in consideration of your agreement to the following terms, and your
#               use, installation, modification or redistribution of this Apple software
#               constitutes acceptance of these terms.  If you do not agree with these terms,
#               please do not use, install, modify or redistribute this Apple software.
# 
#               In consideration of your agreement to abide by the following terms, and subject
#               to these terms, Apple grants you a personal, non-exclusive license, under Apple's
#               copyrights in this original Apple software (the "Apple Software"), to use,
#               reproduce, modify and redistribute the Apple Software, with or without
#               modifications, in source and/or binary forms; provided that if you redistribute
#               the Apple Software in its entirety and without modifications, you must retain
#               this notice and the following text and disclaimers in all such redistributions of
#               the Apple Software.  Neither the name, trademarks, service marks or logos of
#               Apple Computer, Inc. may be used to endorse or promote products derived from the
#               Apple Software without specific prior written permission from Apple.  Except as
#               expressly stated in this notice, no other rights or licenses, express or implied,
#               are granted by Apple herein, including but not limited to any patent rights that
#               may be infringed by your derivative works or by other works in which the Apple
#               Software may be incorporated.
# 
#               The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
#               WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
#               WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#               PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
#               COMBINATION WITH YOUR PRODUCTS.
# 
#               IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
#               CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
#               GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#               ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
#               OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
#               (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
#               ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
#   Change History (most recent first):
# 
# $Log: QKernelTraceBuffer.gdb,v $
# Revision 1.1  2003/11/14 22:35:42  bbergstrand
# First check in. Thanks Quinn.
#
#
#

define showkerneldebugheader
	printf "kd_buf     thread     q Class Sub  Code  Code Specific Info\n"
end

define printevflags
	if $arg0 & 1
		printf "EV_RE "
	end
	if $arg0 & 2
		printf "EV_WR "
	end
	if $arg0 & 4
		printf "EV_EX "
	end
	if $arg0 & 8
		printf "EV_RM "
	end

	if $arg0 & 0x00100
		printf "EV_RBYTES "
	end
	if $arg0 & 0x00200
		printf "EV_WBYTES "
	end
	if $arg0 & 0x00400
		printf "EV_RCLOSED "
	end
	if $arg0 & 0x00800
		printf "EV_RCONN "
	end
	if $arg0 & 0x01000
		printf "EV_WCLOSED "
	end
	if $arg0 & 0x02000
		printf "EV_WCONN "
	end
	if $arg0 & 0x04000
		printf "EV_OOB "
	end
	if $arg0 & 0x08000
		printf "EV_FIN "
	end
	if $arg0 & 0x10000
		printf "EV_RESET "
	end
	if $arg0 & 0x20000
		printf "EV_TIMEOUT "
	end
end

define showkerneldebugbufferentry
	set $q_entry = (kd_buf *) $arg0
	
	set $q_debugid = $q_entry->debugid
	set $q_arg1 = $q_entry->arg1
	set $q_arg2 = $q_entry->arg2
	set $q_arg3 = $q_entry->arg3
	set $q_arg4 = $q_entry->arg4
	
	set $q_class    = ($q_debugid >> 24) & 0x000FF
	set $q_subclass = ($q_debugid >> 16) & 0x000FF
	set $q_code     = ($q_debugid >>  2) & 0x03FFF
	set $q_qual     = ($q_debugid      ) & 0x00003
	
	if $q_qual == 0
		set $q_qual = '-'
	else
		if $q_qual == 1
			set $q_qual = '>'
		else
			if $q_qual == 2
				set $q_qual = '<'
			else
				if $q_qual == 3
					set $q_qual = '?'
				end
			end
		end
	end

	# preamble and qual
	
	printf "0x%08x 0x%08x %c ", $q_entry, $q_entry->arg5, $q_qual
	
	# class
	
	if $q_class == 1
		printf "MACH"
	else
	if $q_class == 2
		printf "NET "
	else
	if $q_class == 3
		printf "FS  "
	else
	if $q_class == 4
		printf "BSD "
	else
	if $q_class == 5
		printf "IOK "
	else
	if $q_class == 6
		printf "DRVR"
	else
	if $q_class == 7
		printf "TRAC"
	else
	if $q_class == 8
		printf "DLIL"
	else
	if $q_class == 20
		printf "MISC"
	else
	if $q_class == 31
		printf "DYLD"
	else
	if $q_class == 255
		printf "MIG "
	else
		printf "0x%02x", $q_class
	end
	end
	end
	end
	end
	end
	end
	end
	end
	end
	end
	
	# subclass and code
	
	printf "  0x%02x %-5d ", $q_subclass, $q_code

	# space for debugid-specific processing
	
	if $q_debugid == 0x14100048
		printf "waitevent "
		if $q_arg1 == 1
			printf "before sleep"
		else
		if $q_arg1 == 2
			printf "after  sleep"
		else
			printf "????????????"
		end
		end
		printf " chan=0x%08x ", $q_arg2
	else
	if $q_debugid == 0x14100049
		printf "waitevent "
	else
	if $q_debugid == 0x1410004a
		printf "waitevent error=%d ", $q_arg1
		printf "eqp=0x%08x ", $q_arg4
		printevflags $q_arg3
		printf "er_handle=%d ", $q_arg2
	else
	if $q_debugid == 0x14100059
		printf "evprocdeque proc=0x%08x ", $q_arg1
		if $q_arg2 == 0
			printf "remove first "
		else
			printf "remove 0x%08x ", $q_arg2
		end
	else
	if $q_debugid == 0x1410005a
		printf "evprocdeque "
		if $q_arg1 == 0
			printf "result=NULL "
		else
			printf "result=0x%08x ", $q_arg1
		end
	else
	if $q_debugid == 0x14100041
		printf "postevent "
		printevflags $q_arg1
	else
	if $q_debugid == 0x14100040
		printf "postevent "
		printf "evq=0x%08x ", $q_arg1
		printf "er_eventbits="
		printevflags $q_arg2
		printf "mask="
		printevflags $q_arg3
	else
	if $q_debugid == 0x14100042
		printf "postevent "
	else
	if $q_debugid == 0x14100055
		printf "evprocenque eqp=0x%08d ", $q_arg1
		if $q_arg2 & 1
			printf "EV_QUEUED "
		end
		printevflags $q_arg3
	else
	if $q_debugid == 0x14100050
		printf "evprocenque before wakeup eqp=0x%08d ", $q_arg4
	else
	if $q_debugid == 0x14100056
		printf "evprocenque "
	else
	if $q_debugid == 0x1410004d
		printf "modwatch "
	else
	if $q_debugid == 0x1410004c
		printf "modwatch er_handle=%d ", $q_arg1
		printevflags $q_arg2
		printf "evq=0x%08x ", $q_arg3
	else
	if $q_debugid == 0x1410004e
		printf "modwatch er_handle=%d ", $q_arg1
		printf "ee_eventmask="
		printevflags $q_arg2
		printf "sp=0x%08x ", $q_arg3
		printf "flag="
		printevflags $q_arg4
	else
		if $q_arg1 != 0
			printf "arg1=0x%08x ", $q_arg1
		end
		if $q_arg2 != 0
			printf "arg2=0x%08x ", $q_arg2
		end
		if $q_arg3 != 0
			printf "arg3=0x%08x ", $q_arg3
		end
		if $q_arg4 != 0
			printf "arg4=0x%08x ", $q_arg4
		end
	end
	end
	end
	end
	end
	end
	end
	end
	end
	end
	end
	end
	end
	end
	
	# finish up
	
	printf "\n"
end

define showkerneldebugbuffer
	set $q_count = (int) $arg0

	if $q_count == 0
		printf "<count> is 0, dumping 50 entries\n"
		set $q_count = 50
	end

	if kd_buffer == 0
		printf "No kernel debug buffer\n"
	else
		if $q_count > nkdbufs
			printf "There are only %d entries in the buffer\n", nkdbufs
		else
			showkerneldebugheader
			
			set $q_cursor = kd_bufptr
			while $q_count > 0
				if $q_cursor == kd_buffer 
					set $q_cursor = kd_buflast
				end
				set $q_cursor = $q_cursor - 1
				set $q_count = $q_count - 1
			
				showkerneldebugbufferentry $q_cursor
			end
		end
	end	
end

document showkerneldebugbuffer
| Prints the last N entries in the kernel debug buffer.
| The syntax is:
|     (gdb) showkerneldebugbuffer <count>
end

