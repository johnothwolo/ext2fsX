#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <sys/errno.h>

#define FS_TYPE			"ext2fs"

int main()
{
        struct vfsconf vfc;
        int name[4], maxtypenum, cnt;
        size_t buflen;

        name[0] = CTL_VFS;
        name[1] = VFS_GENERIC;
        name[2] = VFS_MAXTYPENUM;
        buflen = 4;
        if (sysctl(name, 3, &maxtypenum, &buflen, (void *)0, (size_t)0) < 0)
                return (-1);
        name[2] = VFS_CONF;
        buflen = sizeof vfc;
        for (cnt = 0; cnt < maxtypenum; cnt++) {
                name[3] = cnt;
                if (sysctl(name, 4, &vfc, &buflen, (void *)0, (size_t)0) < 0) {
                        if (errno != EOPNOTSUPP && errno != ENOENT)
                                return (-1);
                        continue;
                }
printf("%s\n", vfc.vfc_name);
                //if (!strcmp(FS_TYPE, vfc.vfc_name))
                  //      return (0);
        }
        errno = ENOENT;
        return (0);

}

