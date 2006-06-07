// Berkeley Open Infrastructure for Network Computing
// http://boinc.berkeley.edu
// Copyright (C) 2006 University of California
//
// This is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation;
// either version 2.1 of the License, or (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU Lesser General Public License for more details.
//
// To view the GNU Lesser General Public License visit
// http://www.gnu.org/copyleft/lesser.html
// or write to the Free Software Foundation, Inc.,
// 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

// SetupSecurity.cpp

#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>

#include <grp.h>	// getgrname, getgrgid
#include <pwd.h>	// getpwname, getpwuid, getuid
#include <unistd.h>
#include <sys/param.h>  // for MAXPATHLEN
#include <sys/stat.h>   // for umask()

#include <Carbon/Carbon.h>

#include "SetupSecurity.h"

static OSStatus CreateUserAndGroup(char * name, Boolean * createdNew);
static OSStatus GetAuthorization(void);
static pascal Boolean ErrorDlgFilterProc(DialogPtr theDialog, EventRecord *theEvent, short *theItemHit);
static AuthorizationRef        gOurAuthRef = NULL;

#define AUTHORIZE_LOOKUPD_MEMBERD false

static char                    dsclPath[] = "/usr/bin/dscl";
static char                    chmodPath[] = "/bin/chmod";
static char                    chownPath[] = "/usr/sbin/chown";
#if AUTHORIZE_LOOKUPD_MEMBERD
static char                    lookupdPath[] = "/usr/sbin/lookupd";
static char                    memberdPath[] = "/usr/sbin/memberd";
#define RIGHTS_COUNT 5
#else
#define RIGHTS_COUNT 3
#endif

#define boinc_master_name "boinc_master"
#define boinc_project_name "boinc_project"

#define MIN_ID 25   /* Minimum user ID / Group ID to create */

OSStatus CreateBOINCUsersAndGroups() {
    unsigned long           endSleep;
    Boolean                 createdNew;
    OSStatus                err = noErr;

    err = CreateUserAndGroup(boinc_master_name, &createdNew);
    if (err != noErr)
        return err;
    
    if (createdNew) {
        endSleep = TickCount() + (1*60);
        while (TickCount() < endSleep) {
            sleep (1);
        }
    }
    
    err = CreateUserAndGroup(boinc_project_name, &createdNew);
    return err;
}


OSStatus SetBOINCAppOwnersGroupsAndPermissions(char *path, char *managerName, Boolean development) {
    char            fullpath[MAXPATHLEN];
    char            buf1[80];
    char            *args[8];
    short           i;
    mode_t          old_mask;
    unsigned long   endSleep;
    OSStatus        err = noErr;
    
    strlcpy(fullpath, path, MAXPATHLEN);
    strlcat(fullpath, "/", MAXPATHLEN);
    strlcat(fullpath, managerName, MAXPATHLEN);
    strlcat(fullpath, ".app", MAXPATHLEN);
    if (strlen(fullpath) >= (MAXPATHLEN-1)) {
        ShowSecurityError("SetBOINCAppOwnersGroupsAndPermissions: path to Manager is too long");
        return -1;
    }

    err = GetAuthorization();
    if (err != noErr) {
        if (err != errAuthorizationCanceled)
            ShowSecurityError("SetBOINCAppOwnersGroupsAndPermissions: GetAuthorization returned error %d", err);
        return err;
    }

    sprintf(buf1, "%s:%s", boinc_master_name, boinc_master_name);
    for (i=0; i<5; i++) {       // Retry 5 times if error
        // chown boinc_master:boinc_master path/BOINCManager.app
        args[0] = "-R";
        args[1] = buf1;
        args[2] = fullpath;
        args[3] = NULL;
        err = AuthorizationExecuteWithPrivileges (gOurAuthRef, chownPath, 0, args, NULL);
        if (err == noErr)
            break;
    }
    if (err != noErr) {
        ShowSecurityError("\"chown %s %s\" returned error %d", buf1, fullpath, err);
        return err;
    }

    if (development) {
        old_mask = umask(0);
        
        for (i=0; i<5; i++) {       // Retry 5 times if error
            // chmod -R u=rwsx,g=rwsx,o=rx path/BOINCManager.app'''
            // 0775 = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH
            //  read, write and execute permission for user, group & others
            args[0] = "-R";
            args[1] = "u=rwx,g=rwx,o=rx";
            args[2] = fullpath;
            args[3] = NULL;
            err = AuthorizationExecuteWithPrivileges (gOurAuthRef, chmodPath, 0, args, NULL);
            if (err == noErr)
                break;
        }
            
        umask(old_mask);

        if (err != noErr) {
            ShowSecurityError("\"chmod -R %s %s\" returned error %d", args[1], fullpath, err);
            return err;
        }

        endSleep = TickCount() + (30);
        while (TickCount() < endSleep) {
            sleep (1);
        }
    }       // End if (development)

    strlcat(fullpath, "/Contents/MacOS/", MAXPATHLEN);
    strlcat(fullpath, managerName, MAXPATHLEN);
    if (strlen(fullpath) >= (MAXPATHLEN-1)) {
        ShowSecurityError("SetBOINCAppOwnersGroupsAndPermissions: path to Manager is too long");
        return -1;
    }

    old_mask = umask(0);

    for (i=0; i<5; i++) {       // Retry 5 times if error
        if (development) {
            // chmod u=rwx,g=rwsx,o=rx path/BOINCManager.app/Contents/MacOS/BOINCManager
            // 02775 = S_ISGID | S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH
            //  setgid-on-execution plus read, write and execute permission for user, group & others
            args[0] = "u=rwx,g=rwsx,o=rx";
        } else {
            // chmod u=rx,g=rsx,o=rx path/BOINCManager.app/Contents/MacOS/BOINCManager
            // 02555 = S_ISGID | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH
            //  setgid-on-execution plus read and execute permission for user, group & others
            args[0] = "u=rx,g=rsx,o=rx";
        }
        args[1] = fullpath;
        args[2] = NULL;
        err = AuthorizationExecuteWithPrivileges (gOurAuthRef, chmodPath, 0, args, NULL);
        if (err == noErr)
            break;
    }
        
    umask(old_mask);

    if (err != noErr) {
        ShowSecurityError("\"chmod %s %s\" returned error %d", args[0], fullpath, err);
        return err;
    }

    endSleep = TickCount() + (30);
    while (TickCount() < endSleep) {
        sleep (1);
    }

    strlcpy(fullpath, path, MAXPATHLEN);
    strlcat(fullpath, "/", MAXPATHLEN);
    strlcat(fullpath, managerName, MAXPATHLEN);
    strlcat(fullpath, ".app/Contents/Resources/boinc", MAXPATHLEN);
    if (strlen(fullpath) >= (MAXPATHLEN-1)) {
        ShowSecurityError("SetBOINCAppOwnersGroupsAndPermissions: path to client is too long");
        return -1;
    }
    
    old_mask = umask(0);

    for (i=0; i<5; i++) {       // Retry 5 times if error
        if (development) {
            // chmod u=rwsx,g=rwsx,o=rx path/BOINCManager.app/Contents/Resources/boinc
            // 06775 = S_ISUID | S_ISGID | S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH
            //  setuid-on-execution, setgid-on-execution plus read, write and execute permission for user, group & others
            args[0] = "u=rwsx,g=rwsx,o=rx";
        } else {
            // chmod u=rsx,g=rsx,o=rx path/BOINCManager.app/Contents/Resources/boinc
            // 06555 = S_ISUID | S_ISGID | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH
            //  setuid-on-execution, setgid-on-execution plus read and execute permission for user, group & others
            args[0] = "u=rsx,g=rsx,o=rx";
        }
        args[1] = fullpath;
        args[2] = NULL;
        err = AuthorizationExecuteWithPrivileges (gOurAuthRef, chmodPath, 0, args, NULL);
        if (err == noErr)
            break;
    }
        
    umask(old_mask);

    if (err != noErr) {
        ShowSecurityError("\"chmod %s %s\" returned error %d", args[0], fullpath, err);
        return err;
    }

    if (development) {
        sprintf(buf1, "/groups/%s", boinc_master_name);

        // Something like "dscl . -merge /groups/boinc_master users login_name"
        for (i=0; i<5; i++) {       // Retry 5 times if error
            args[0] = ".";
            args[1] = "-merge";    // "-append";
            args[2] = buf1;
            args[3] = "users";
            args[4] = getlogin();
            args[5] = NULL;
            err = AuthorizationExecuteWithPrivileges (gOurAuthRef, dsclPath, 0, args, NULL);
            if (err == noErr)
                break;

                endSleep = TickCount() + (30);
                while (TickCount() < endSleep) {
                    sleep (1);
            }

        }
        
        if (err != noErr) {
            ShowSecurityError("\"dscl . -create -merge %s users %s\" returned error %d", buf1, getlogin(), err);
            return err;
        }

        system("lookupd -flushcache");
        system("memberd -r");
    }       // End if (development)

    return err;
}


static OSStatus CreateUserAndGroup(char * name, Boolean * createdNew) {
    OSStatus    err = noErr;
    passwd      *pw = NULL;
    group       *grp = NULL;
    uid_t       userid = 0;
    gid_t       groupid = 0;
    gid_t       usergid = 0;
    Boolean     userExists = false;
    Boolean     groupExists = false;
    short       i;
    char        *args[8];
    char        buf1[80];
    char        buf2[80];
    char        buf3[80];
    
    *createdNew = false;

    pw = getpwnam(name);
    if (pw) {
        userid = pw->pw_uid;
        userExists = true;
    }

    grp = getgrnam(name);
    if (grp) {
        groupid = grp->gr_gid;
        groupExists = true;
    }
    
    if ( userExists && groupExists )
        return noErr;       // User and group already exist

    *createdNew = true;
    
    // If only user or only group exists, try to use the same ID for the one we create
    if (userExists) {      // User exists but group does not
        usergid = pw->pw_gid;
        if (usergid) {
            grp = getgrgid(usergid);
            if (grp == NULL)    // Set the group ID = users existing group if this group ID is available
                groupid = usergid;
        }
        if (groupid == 0) {
            grp = getgrgid(userid);
            if (grp == NULL)    // Set the group ID = user ID if this group ID is available
                groupid = userid;
        }
    } else {
        if (groupExists) {      // Group exists but user does not
            pw = getpwuid(groupid);
            if (pw == NULL)    // Set the user ID = group ID if this user ID is available
                userid = groupid;
        }
    }
    
    // We need to find an available user ID, group ID, or both.  Find a value that is currently 
    // neither a user ID or a group ID.
    // If we need both a new user ID and a new group ID, finds a value that can be used for both.
    if ( (userid == 0) || (groupid == 0) ) {
        for(i=MIN_ID; ; i++) {
           if ((uid_t)i != userid) {
                pw = getpwuid((uid_t)i);
                if (pw)
                    continue;               // Already exists as a user ID of a different user
            }
            
            if ((gid_t)i != groupid) {
                grp = getgrgid((gid_t)i);
                if (grp)
                    continue;               // Already exists as a group ID of a different group
            }
            
            if (! userExists)
                userid = (uid_t)i;
            if (! groupExists)
                groupid = (gid_t)i;
                
            break;                          // Success!
        }
    }

    err = GetAuthorization();
    if (err != noErr) {
        if (err != errAuthorizationCanceled)
            ShowSecurityError("CreateUserAndGroup: GetAuthorization returned error %d", err);
        return err;
    }

    sprintf(buf1, "/groups/%s", name);
    sprintf(buf2, "%d", groupid);

    if (! groupExists) {             // If we need to create group
        for (i=0; i<5; i++) {       // Retry 5 times if error
            // Something like "dscl . -create /groups/boinc_master"
            args[0] = ".";
            args[1] = "-create";
            args[2] = buf1;
            args[3] = NULL;
            err = AuthorizationExecuteWithPrivileges (gOurAuthRef, dsclPath, 0, args, NULL);
            if (err == noErr)
                break;
        }
        if (err != noErr) {
            ShowSecurityError("\"dscl . -create %s\" returned error %d", buf1, err);
            return err;
        }

        // Something like "dscl . -create /groups/boinc_master gid 33"
        for (i=0; i<5; i++) {       // Retry 5 times if error
            args[0] = ".";
            args[1] = "-create";    // "-append";
            args[2] = buf1;
            args[3] = "gid";
            args[4] = buf2;
            args[5] = NULL;
            err = AuthorizationExecuteWithPrivileges (gOurAuthRef, dsclPath, 0, args, NULL);
            if (err == noErr)
                break;
        }
        if (err != noErr) {
            ShowSecurityError("\"dscl . -create %s gid %s\" returned error %d", buf1, buf2, err);
            return err;
        }
    }

    sprintf(buf1, "/users/%s", name);
    sprintf(buf2, "%d", groupid);
    sprintf(buf3, "%d", userid);
        
    if (! userExists) {             // If we need to create user
        for (i=0; i<5; i++) {       // Retry 5 times if error
            // Something like "dscl . -create /users/boinc_master"
            args[0] = ".";
            args[1] = "-create";
            args[2] = buf1;
            args[3] = NULL;
            err = AuthorizationExecuteWithPrivileges (gOurAuthRef, dsclPath, 0, args, NULL);
            if (err == noErr)
                break;
        }
        if (err != noErr) {
            ShowSecurityError("\"dscl . -create %s\" returned error %d", buf1, err);
            return err;
        }

        for (i=0; i<5; i++) {       // Retry 5 times if error
            // Something like "dscl . -create /users/boinc_master uid 33"
            args[0] = ".";
            args[1] = "-create";    // "-append";
            args[2] = buf1;
            args[3] = "uid";
            args[4] = buf3;
            args[5] = NULL;
            err = AuthorizationExecuteWithPrivileges (gOurAuthRef, dsclPath, 0, args, NULL);
            if (err == noErr)
                break;
        }
        if (err != noErr) {
            ShowSecurityError("\"dscl . -create %s uid %s\" returned error %d", buf1, buf3, err);
            return err;
        }

        for (i=0; i<5; i++) {       // Retry 5 times if error
            // Prevent a security hole by not allowing a login from this user
            // Something like "dscl . -create /users/boinc_master shell /sbin/nologin"
            args[0] = ".";
            args[1] = "-create";
            args[2] = buf1;
            args[3] = "shell";
            args[4] = "/sbin/nologin";
            args[5] = NULL;
            err = AuthorizationExecuteWithPrivileges (gOurAuthRef, dsclPath, 0, args, NULL);
            if (err == noErr)
                break;
        }
        if (err != noErr) {
            ShowSecurityError("\"dscl . -create %s shell /sbin/nologin\" returned error %d", buf1, err);
            return err;
        }
    }

    for (i=0; i<5; i++) {       // Retry 5 times if error
        // Always set the user gid if we created either the user or the group or both
        // Something like "dscl . -create /users/boinc_master gid 33"
        args[0] = ".";
        args[1] = "-create";    // "-append";
        args[2] = buf1;
        args[3] = "gid";
        args[4] = buf2;
        args[5] = NULL;
        err = AuthorizationExecuteWithPrivileges (gOurAuthRef, dsclPath, 0, args, NULL);
        if (err == noErr)
            break;
    }
    if (err != noErr) {
        ShowSecurityError("\"dscl . -create %s gid %s\" returned error %d", buf1, buf2, err);
        return err;
    }
    
#if AUTHORIZE_LOOKUPD_MEMBERD
    for (i=0; i<5; i++) {       // Retry 5 times if error
        // "lookupd -flushcache"
        args[0] = "-flushcache";
        args[1] = NULL;
        err = AuthorizationExecuteWithPrivileges (gOurAuthRef, lookupdPath, 0, args, NULL);
        if (err == noErr)
            break;
    }
    if (err != noErr) {
        ShowSecurityError("\"lookupdPath -flushcache\" returned error %d", err);
        return err;
    }

    for (i=0; i<5; i++) {       // Retry 5 times if error
        // "memberd -r"
        args[0] = "-r";
        args[1] = NULL;
        err = AuthorizationExecuteWithPrivileges (gOurAuthRef, memberdPath, 0, args, NULL);
        if (err == noErr)
            break;
    }
    if (err != noErr) {
        ShowSecurityError("\"memberd -r\" returned error %d", err);
        return err;
    }
#else
    system("lookupd -flushcache");
    system("memberd -r");
#endif
    
    return noErr;
}


static OSStatus GetAuthorization (void) {
    static Boolean          sIsAuthorized = false;
    AuthorizationRights     ourAuthRights;
    AuthorizationFlags      ourAuthFlags;
    AuthorizationItem       ourAuthItem[RIGHTS_COUNT];
    OSStatus                err = noErr;

    if (sIsAuthorized)
        return noErr;
        
    ourAuthRights.count = 0;
    ourAuthRights.items = NULL;

    err = AuthorizationCreate (&ourAuthRights, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &gOurAuthRef);
    if (err != noErr) {
        ShowSecurityError("AuthorizationCreate returned error %d", err);
        return err;
    }
     
    ourAuthItem[0].name = kAuthorizationRightExecute;
    ourAuthItem[0].value = dsclPath;
    ourAuthItem[0].valueLength = strlen (dsclPath);
    ourAuthItem[0].flags = 0;

    ourAuthItem[1].name = kAuthorizationRightExecute;
    ourAuthItem[1].value = chmodPath;
    ourAuthItem[1].valueLength = strlen (chmodPath);
    ourAuthItem[1].flags = 0;

    ourAuthItem[2].name = kAuthorizationRightExecute;
    ourAuthItem[2].value = chownPath;
    ourAuthItem[2].valueLength = strlen (chownPath);
    ourAuthItem[2].flags = 0;

#if AUTHORIZE_LOOKUPD_MEMBERD
    ourAuthItem[3].name = kAuthorizationRightExecute;
    ourAuthItem[3].value = lookupdPath;
    ourAuthItem[3].valueLength = strlen (lookupdPath);
    ourAuthItem[3].flags = 0;

    ourAuthItem[4].name = kAuthorizationRightExecute;
    ourAuthItem[4].value = memberdPath;
    ourAuthItem[4].valueLength = strlen (memberdPath);
    ourAuthItem[4].flags = 0;
#endif

    ourAuthRights.count = RIGHTS_COUNT;
    ourAuthRights.items = ourAuthItem;
    
    ourAuthFlags = kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights;
    
    // When this is called from the installer, the installer has already authenticated.  
    // In that case we are already running with full root privileges so AuthorizationCopyRights() 
    // does not request a password from the user again.
    err = AuthorizationCopyRights (gOurAuthRef, &ourAuthRights, kAuthorizationEmptyEnvironment, ourAuthFlags, NULL);
    
    if (err == noErr)
        sIsAuthorized = true;
    
    return err;
}


void ShowSecurityError(const char *format, ...) {
    va_list                 args;
    char                    s[1024];
    short                   itemHit;
    AlertStdAlertParamRec   alertParams;
    ModalFilterUPP          ErrorDlgFilterProcUPP;

    ProcessSerialNumber	ourProcess;

    va_start(args, format);
    s[0] = vsprintf(s+1, format, args);
    va_end(args);

    ErrorDlgFilterProcUPP = NewModalFilterUPP(ErrorDlgFilterProc);

    alertParams.movable = true;
    alertParams.helpButton = false;
    alertParams.filterProc = ErrorDlgFilterProcUPP;
    alertParams.defaultText = "\pOK";
    alertParams.cancelText = NULL;
    alertParams.otherText = NULL;
    alertParams.defaultButton = kAlertStdAlertOKButton;
    alertParams.cancelButton = 0;
    alertParams.position = kWindowDefaultPosition;

    ::GetCurrentProcess (&ourProcess);
    ::SetFrontProcess(&ourProcess);

    StandardAlert (kAlertStopAlert, (StringPtr)s, NULL, &alertParams, &itemHit);

    DisposeModalFilterUPP(ErrorDlgFilterProcUPP);
}


static pascal Boolean ErrorDlgFilterProc(DialogPtr theDialog, EventRecord *theEvent, short *theItemHit) {
    // We need this because this is a command-line application so it does not get normal events
    if (Button()) {
        *theItemHit = kStdOkItemIndex;
        return true;
    }
    
    return StdFilterProc(theDialog, theEvent, theItemHit);
}

