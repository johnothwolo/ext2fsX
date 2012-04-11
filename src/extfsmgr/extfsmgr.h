/*
* Copyright 2003-2004 Brian Bergstrand.
*
* Redistribution and use in source and binary forms, with or without modification, 
* are permitted provided that the following conditions are met:
*
* 1.	Redistributions of source code must retain the above copyright notice, this list of
*     conditions and the following disclaimer.
* 2.	Redistributions in binary form must reproduce the above copyright notice, this list of
*     conditions and the following disclaimer in the documentation and/or other materials provided
*     with the distribution.
* 3.	The name of the author may not be used to endorse or promote products derived from this
*     software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
* AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#ifndef EXTFSMGR_H
#define EXTFSMGR_H

#ifdef __OBJC__
/*!
@defined EXT_PREF_ID
@abstract Name of preference file.
@discussion This is used to store a copy of the local user preferences
in the global preferences. Only the global copy affects mount_ext2fs.
*/
#define EXT_PREF_ID @"net.sourceforge.ext2fsx.ExtFSManager"
/*!
@defined EXT_PREF_KEY_GLOBAL
@abstract Dictionary key to get the preferences common to any media.
*/
#define EXT_PREF_KEY_GLOBAL @"Global"
/*!
@defined EXT_PREF_KEY_MEDIA
@abstract Dictionary key to get the preferences specific to a single disk/partition.
@discussion The list is stored as a dictionary keyed on the filesystem UUID.
*/
#define EXT_PREF_KEY_MEDIA @"Media"
/*!
@defined EXT_PREF_KEY_MGR
@abstract Dictionary key to get the preferences for the manager application.
*/
#define EXT_PREF_KEY_MGR @"ExtFS Manager"
/*!
@defined EXT_PREF_KEY_SMARTD
@abstract Dictionary key to get the preferences for the efssmartd application.
*/
#define EXT_PREF_KEY_SMARTD @"SMART Daemon"

/*!
@defined EXT_PREF_KEY_RDONLY
@abstract Dictionary key to determine if the media should be mounted read-only.
*/
#define EXT_PREF_KEY_RDONLY @"Read Only"
/*!
@defined EXT_PREF_KEY_NOAUTO
@abstract Dictionary key to determine if automount requests
for the media should be ignored by mount_ext2fs.
*/
#define EXT_PREF_KEY_NOAUTO @"Ignore Automount"
/*!
@defined EXT_PREF_KEY_NOPERMS
@abstract Dictionary key to determine if media should be mounted sans
permissions.
*/
#define EXT_PREF_KEY_NOPERMS @"Ignore Permissions"
/*!
@defined EXT_PREF_KEY_DIRINDEX
@abstract Dictionary key to determine if media should be mounted with the
indexed directories option. This is only valid for Ext2/Ext3 filesystems.
*/
#define EXT_PREF_KEY_DIRINDEX @"Index Directories"


/*!
@defined EXT_PREF_KEY_MGR_SMARTD_ADDED
@abstract Dictionary key to determine if efssmartd has been added
to the user's login list.
*/
#define EXT_PREF_KEY_MGR_SMARTD_ADDED @"SMARTD Added"

/*
@define EXT_PREF_KEY_SMARTD_MON_INTERVAL
@abstract Dictionary key to determine the S.M.A.R.T. poll interval.
*/
#define EXT_PREF_KEY_SMARTD_MON_INTERVAL @"Poll Interval"
#else
#define EXT_PREF_ID CFSTR("net.sourceforge.ext2fsx.ExtFSManager")
#define EXT_PREF_KEY_GLOBAL CFSTR("Global")
#define EXT_PREF_KEY_MEDIA CFSTR("Media")
#define EXT_PREF_KEY_MGR CFSTR("ExtFS Manager")

#define EXT_PREF_KEY_RDONLY CFSTR("Read Only")
#define EXT_PREF_KEY_NOAUTO CFSTR("Ignore Automount")
#define EXT_PREF_KEY_NOPERMS CFSTR("Ignore Permissions")
#define EXT_PREF_KEY_DIRINDEX CFSTR("Index Directories")
#define EXT_PREF_KEY_MGR_SMARTD_ADDED CFSTR("SMARTD Added")
#define EXT_PREF_KEY_SMARTD CFSTR("SMART Daemon")
#define EXT_PREF_KEY_SMARTD_MON_INTERVAL CFSTR("Poll Interval")
#endif

#endif /* EXTFSMGR_H */
