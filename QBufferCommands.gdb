#
#   File:       QBufferCommands.gdb
# 
#   Contains:   GDB commands to dump buffer cache.
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
# $Log: QBufferCommands.gdb,v $
# Revision 1.1  2003/11/14 22:35:42  bbergstrand
# First check in. Thanks Quinn.
#
#
#

define qint_showbufhdr
    printf "buf      b_vp     b_lblkno  b_blkno b_bcount b_whichq b_flags\n"
end

define qint_showbuf
    set $q_buf = (struct buf *) $arg0
    if $arg1 != 0
        printf "    "
    end
    printf "%08x %08x %8d %8d %8d ", $q_buf, $q_buf->b_vp, $q_buf->b_lblkno, $q_buf->b_blkno, $q_buf->b_bcount
    
    set $q_buf_whichq = $q_buf->b_whichq
    if $q_buf_whichq == 0
        printf "LOCKED   "
    else
    if $q_buf_whichq == 1
        printf "LRU      "
    else
    if $q_buf_whichq == 2
        printf "AGE      "
    else
    if $q_buf_whichq == 3
        printf "EMPTY    "
    else
    if $q_buf_whichq == 4
        printf "META     "
    else
    if $q_buf_whichq == 5
        printf "LAUNDRY  "
    else
        printf "%-7d  ", $q_buf_whichq
    end
    end
    end
    end
    end
    end
    
    set $q_buf_flags = $q_buf->b_flags
    
    # B_BUSY
    if $q_buf_flags & 0x00000010
        printf "B"
    else
        printf "-"
    end

    # B_CACHE
    if $q_buf_flags & 0x00000020
        printf "C"
    else
        printf "-"
    end

    # B_DELWRI
    if $q_buf_flags & 0x00000080
        printf "D"
    else
        printf "-"
    end

    # B_INVAL
    if $q_buf_flags & 0x00002000
        printf "I"
    else
        printf "-"
    end

    # B_LOCKED
    if $q_buf_flags & 0x00004000
        printf "L"
    else
        printf "-"
    end

    # B_PAGELIST
    if $q_buf_flags & 0x00400000
        printf "P"
    else
        printf "-"
    end

    # B_META
    if $q_buf_flags & 0x40000000
        printf "M"
    else
        printf "-"
    end
    
    printf "\n"
end

define showbuf
    qint_showbufhdr 0
    qint_showbuf $arg0 0
end

document showbuf
| Prints a block buffer (struct buf *) summary.
| The syntax is:
|     (gdb) showbuf <buf>
end

define qint_showbufhashchain
    set $q_this_buf = (struct buf *) $arg0
    
    set $q_indent_buf = 0
    
    while $q_this_buf != 0
        qint_showbuf $q_this_buf $q_indent_buf
        
        set $q_indent_buf = 1
        set $q_this_buf = ($q_this_buf)->b_hash.le_next
    end
end

define showbufhashtbl
    printf "ndx "
    qint_showbufhdr
    
    set $q_buf_index = 0
    while $q_buf_index <= bufhash
        set $q_buf_first = bufhashtbl[$q_buf_index].lh_first
        
        if $q_buf_first != 0
            printf "%-3d ", $q_buf_index
            qint_showbufhashchain $q_buf_first
        end
        
        set $q_buf_index = $q_buf_index + 1
    end
end

document showbufhashtbl
| Prints the block buffer hash table (bufhashtbl).
| The syntax is:
|     (gdb) showbufhashtbl
end

define qint_showbuffreelist 
    
    set $q_this_buf = $arg0
    set $q_buf_count = 0
    
    while $q_this_buf != 0
        qint_showbuf $q_this_buf $arg1
        
        set $q_buf_count = $q_buf_count + 1
        set $q_this_buf = ($q_this_buf)->b_freelist.tqe_next
    end
    if $arg1 != 0
        printf "    "
    end
    if $q_buf_count == 1
        printf "%d entry\n", $q_buf_count
    else
        printf "%d entries\n", $q_buf_count
    end
end

define showbufqueues
    printf "q   "
    qint_showbufhdr 1

    printf "BQ_LOCKED  - bufqueues[0]\n"
    qint_showbuffreelist bufqueues[0].tqh_first 1
    printf "BQ_LRU     - bufqueues[1]\n"
    qint_showbuffreelist bufqueues[1].tqh_first 1
    printf "BQ_AGE     - bufqueues[2]\n"
    qint_showbuffreelist bufqueues[2].tqh_first 1
    printf "BQ_EMPTY   - bufqueues[3]\n"
    qint_showbuffreelist bufqueues[3].tqh_first 1
    printf "BQ_LAUNDRY - bufqueues[5]\n"
    qint_showbuffreelist bufqueues[5].tqh_first 1

    # We show BQ_META last because apparently it's always the long one.
    
    printf "BQ_META    - bufqueues[4]\n"
    qint_showbuffreelist bufqueues[4].tqh_first 1
end

document showbufqueues
| Prints the block buffer free lists (bufqueues).
| The syntax is:
|     (gdb) showbufqueues
end

define showiobufqueue
    qint_showbufhdr 0
    qint_showbuffreelist iobufqueue.tqh_first 0
end

document showiobufqueue
| Prints the block buffer I/O list (iobufqueue).
| The syntax is:
|     (gdb) showiobufqueue
end

define showbufflags
    set $q_buf_flags = (int) $arg0
    
    printf "B_AGE          %c  Move to age queue when I/O done\n", ($q_buf_flags & 0x00000001 ? '1' : '-')
    printf "B_NEEDCOMMIT   %c  Append-write in progress\n", ($q_buf_flags & 0x00000002 ? '1' : '-')
    printf "B_ASYNC        %c  Start I/O, do not wait\n", ($q_buf_flags & 0x00000004 ? '1' : '-')
    printf "B_BAD          %c  Bad block revectoring in progress\n", ($q_buf_flags & 0x00000008 ? '1' : '-')
    printf "B_BUSY         %c  I/O in progress\n", ($q_buf_flags & 0x00000010 ? '1' : '-')
    printf "B_CACHE        %c  bread() found us in the cache\n", ($q_buf_flags & 0x00000020 ? '1' : '-')
    printf "B_CALL         %c  Call b_iodone from biodone\n", ($q_buf_flags & 0x00000040 ? '1' : '-')
    printf "B_DELWRI       %c  Delay I/O until buffer reused\n", ($q_buf_flags & 0x00000080 ? '1' : '-')
    printf "B_DIRTY        %c  Dirty page to be pushed out async\n", ($q_buf_flags & 0x00000100 ? '1' : '-')
    printf "B_DONE         %c  I/O completed\n", ($q_buf_flags & 0x00000200 ? '1' : '-')
    printf "B_EINTR        %c  I/O was interrupted\n", ($q_buf_flags & 0x00000400 ? '1' : '-')
    printf "B_ERROR        %c  I/O error occurred\n", ($q_buf_flags & 0x00000800 ? '1' : '-')
    printf "B_WASDIRTY     %c  Page was found dirty in the VM cache\n", ($q_buf_flags & 0x00001000 ? '1' : '-')
    printf "B_INVAL        %c  Does not contain valid info\n", ($q_buf_flags & 0x00002000 ? '1' : '-')
    printf "B_LOCKED       %c  Locked in core (not reusable)\n", ($q_buf_flags & 0x00004000 ? '1' : '-')
    printf "B_NOCACHE      %c  Do not cache block after use\n", ($q_buf_flags & 0x00008000 ? '1' : '-')
    printf "B_PAGEOUT      %c  Page out indicator\n", ($q_buf_flags & 0x00010000 ? '1' : '-')
    printf "B_PGIN         %c  Pagein op, so swap() can count it\n", ($q_buf_flags & 0x00020000 ? '1' : '-')
    printf "B_PHYS         %c  I/O to user memory\n", ($q_buf_flags & 0x00040000 ? '1' : '-')
    printf "B_RAW          %c  Set by physio for raw transfers\n", ($q_buf_flags & 0x00080000 ? '1' : '-')
    printf "B_READ         %c  Read buffer\n", ($q_buf_flags & 0x00100000 ? '1' : '-')
    printf "B_TAPE         %c  Magnetic tape I/O\n", ($q_buf_flags & 0x00200000 ? '1' : '-')
    printf "B_PAGELIST     %c  Buffer describes pagelist I/O\n", ($q_buf_flags & 0x00400000 ? '1' : '-')
    printf "B_WANTED       %c  Process wants this buffer\n", ($q_buf_flags & 0x00800000 ? '1' : '-')
    printf "B_WRITE        %c  Write buffer (pseudo flag)\n", ($q_buf_flags & 0x00000000 ? '1' : '-')
    printf "B_WRITEINPROG  %c  Write in progress\n", ($q_buf_flags & 0x01000000 ? '1' : '-')
    printf "B_HDRALLOC     %c  Zone allocated buffer header\n", ($q_buf_flags & 0x02000000 ? '1' : '-')
    printf "B_NORELSE      %c  Don't brelse() in bwrite()\n", ($q_buf_flags & 0x04000000 ? '1' : '-')
    printf "B_NEED_IODONE  %c  Do biodone on the real_bp associated with a cluster_io\n", ($q_buf_flags & 0x08000000 ? '1' : '-')
    printf "B_COMMIT_UPL   %c  commit pages in upl when I/O completes/fails\n", ($q_buf_flags & 0x10000000 ? '1' : '-')
    printf "B_ZALLOC       %c  b_data is zalloc'ed\n", ($q_buf_flags & 0x20000000 ? '1' : '-')
    printf "B_META         %c  Buffer contains meta-data\n", ($q_buf_flags & 0x40000000 ? '1' : '-')
    printf "B_VECTORLIST   %c  Used by device drivers\n", ($q_buf_flags & 0x80000000 ? '1' : '-')
end

document showbufflags
| Prints which buffer flags are set in an integer.
| The syntax is:
|     (gdb) showbufflags <int>
end

define showbufflagsofbuf
    set $q_buf = (struct buf *) $arg0
    
    showbufflags $q_buf->b_flags
end

document showbufflagsofbuf
| Prints which buffer flags are set in a (struct buf *).
| The syntax is:
|     (gdb) showbufflagsofbuf <buf>
end
